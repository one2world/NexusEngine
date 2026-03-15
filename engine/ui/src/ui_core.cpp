#include "nexus/ui/ui_core.h"
#include <algorithm>
#include <cmath>

namespace nexus::ui {

// ── Widget ID generation ────────────────────────────────────────────────────

u32 Widget::next_id() {
    static u32 counter = 0;
    return ++counter;
}

// ── Tree operations ─────────────────────────────────────────────────────────

void Widget::add_child(WidgetPtr child) {
    if (!child) return;
    child->parent_ = this;
    children_.push_back(std::move(child));
}

void Widget::remove_child(u32 child_id) {
    children_.erase(
        std::remove_if(children_.begin(), children_.end(),
            [child_id](const WidgetPtr& c) { return c->id() == child_id; }),
        children_.end());
}

void Widget::remove_all_children() {
    for (auto& c : children_) {
        c->parent_ = nullptr;
    }
    children_.clear();
}

// ── Events ──────────────────────────────────────────────────────────────────

void Widget::on(UIEventType type, UICallback callback) {
    callbacks_[type].push_back(std::move(callback));
}

bool Widget::dispatch_event(UIEvent& event) {
    if (!visible || !interactive || disabled) return false;

    // Custom handler
    if (on_event(event) || event.consumed) return true;

    // Registered callbacks
    auto it = callbacks_.find(event.type);
    if (it != callbacks_.end()) {
        for (auto& cb : it->second) {
            cb(event);
            if (event.consumed) return true;
        }
    }
    return event.consumed;
}

bool Widget::on_event(UIEvent& /*event*/) {
    return false;
}

// ── Layout ──────────────────────────────────────────────────────────────────

static float resolve_size(SizeValue sv, float available, float content) {
    switch (sv.mode) {
        case SizeMode::Fixed:   return sv.value;
        case SizeMode::Percent: return available * sv.value / 100.0f;
        case SizeMode::Auto:    return content;
        case SizeMode::Grow:    return available; // handled by parent
    }
    return content;
}

void Widget::perform_layout(const Rect& available) {
    // Resolve own size
    Vec2 content_measure = measure_content(available.size.x, available.size.y);

    float w = resolve_size(layout.width, available.size.x, content_measure.x);
    float h = resolve_size(layout.height, available.size.y, content_measure.y);

    // Apply min/max
    float min_w = resolve_size(layout.min_width, available.size.x, 0.0f);
    float min_h = resolve_size(layout.min_height, available.size.y, 0.0f);
    float max_w = resolve_size(layout.max_width, available.size.x, 99999.0f);
    float max_h = resolve_size(layout.max_height, available.size.y, 99999.0f);
    w = std::max(min_w, std::min(w, max_w));
    h = std::max(min_h, std::min(h, max_h));

    // Position: if absolute, use anchor; otherwise parent assigns position
    if (layout.absolute) {
        Vec2 pos = available.position;
        switch (layout.anchor) {
            case Anchor::TopLeft:       break;
            case Anchor::TopCenter:     pos.x += (available.size.x - w) * 0.5f; break;
            case Anchor::TopRight:      pos.x += available.size.x - w; break;
            case Anchor::MiddleLeft:    pos.y += (available.size.y - h) * 0.5f; break;
            case Anchor::Center:        pos.x += (available.size.x - w) * 0.5f;
                                        pos.y += (available.size.y - h) * 0.5f; break;
            case Anchor::MiddleRight:   pos.x += available.size.x - w;
                                        pos.y += (available.size.y - h) * 0.5f; break;
            case Anchor::BottomLeft:    pos.y += available.size.y - h; break;
            case Anchor::BottomCenter:  pos.x += (available.size.x - w) * 0.5f;
                                        pos.y += available.size.y - h; break;
            case Anchor::BottomRight:   pos.x += available.size.x - w;
                                        pos.y += available.size.y - h; break;
        }
        pos.x += layout.margin.left;
        pos.y += layout.margin.top;
        computed_rect_ = {pos, {w, h}};
    } else {
        computed_rect_ = {available.position, {w, h}};
    }

    // Layout children (flexbox-like)
    if (children_.empty()) return;

    bool is_row = (layout.direction == Direction::Row ||
                   layout.direction == Direction::RowReverse);
    bool reverse = (layout.direction == Direction::RowReverse ||
                    layout.direction == Direction::ColumnReverse);

    float inner_x = computed_rect_.position.x + layout.padding.left;
    float inner_y = computed_rect_.position.y + layout.padding.top;
    float inner_w = w - layout.padding.horizontal();
    float inner_h = h - layout.padding.vertical();

    // Collect non-absolute children
    std::vector<Widget*> flow_children;
    for (auto& c : children_) {
        if (!c->visible) continue;
        if (c->layout.absolute) {
            c->perform_layout(computed_rect_);
            continue;
        }
        flow_children.push_back(c.get());
    }

    if (flow_children.empty()) return;
    if (reverse) {
        std::reverse(flow_children.begin(), flow_children.end());
    }

    // First pass: measure children to get their desired sizes
    float total_fixed = 0.0f;
    float total_grow = 0.0f;
    float spacing_total = layout.spacing * static_cast<float>(flow_children.size() - 1);

    struct ChildInfo { float size; float grow; };
    std::vector<ChildInfo> infos(flow_children.size());

    for (size_t i = 0; i < flow_children.size(); ++i) {
        auto* child = flow_children[i];
        SizeValue sz = is_row ? child->layout.width : child->layout.height;
        Vec2 child_content = child->measure_content(inner_w, inner_h);
        float desired = is_row ? child_content.x : child_content.y;

        float margin_main = is_row
            ? child->layout.margin.horizontal()
            : child->layout.margin.vertical();

        if (sz.mode == SizeMode::Grow) {
            infos[i] = {0.0f, sz.value};
            total_grow += sz.value;
            total_fixed += margin_main;
        } else {
            float resolved = resolve_size(sz, is_row ? inner_w : inner_h, desired);
            infos[i] = {resolved, 0.0f};
            total_fixed += resolved + margin_main;
        }
    }

    // Distribute remaining space to grow children
    float remaining = (is_row ? inner_w : inner_h) - total_fixed - spacing_total;
    if (remaining > 0 && total_grow > 0) {
        for (size_t i = 0; i < infos.size(); ++i) {
            if (infos[i].grow > 0) {
                infos[i].size = remaining * infos[i].grow / total_grow;
            }
        }
    }

    // Justify: compute start offset and gap
    float total_child_size = 0.0f;
    for (auto& info : infos) total_child_size += info.size;
    float total_margins = 0.0f;
    for (auto* c : flow_children) {
        total_margins += is_row
            ? c->layout.margin.horizontal()
            : c->layout.margin.vertical();
    }

    float main_space = (is_row ? inner_w : inner_h);
    float free_space = main_space - total_child_size - total_margins - spacing_total;
    float justify_offset = 0.0f;
    float justify_gap = 0.0f;
    auto n = static_cast<float>(flow_children.size());

    switch (layout.justify) {
        case Justify::Start:        break;
        case Justify::Center:       justify_offset = free_space * 0.5f; break;
        case Justify::End:          justify_offset = free_space; break;
        case Justify::SpaceBetween:
            if (n > 1) justify_gap = free_space / (n - 1);
            break;
        case Justify::SpaceAround:
            justify_gap = free_space / n;
            justify_offset = justify_gap * 0.5f;
            break;
        case Justify::SpaceEvenly:
            justify_gap = free_space / (n + 1);
            justify_offset = justify_gap;
            break;
    }

    // Second pass: position children
    float cursor = (is_row ? inner_x : inner_y) + justify_offset;

    for (size_t i = 0; i < flow_children.size(); ++i) {
        auto* child = flow_children[i];
        float main_size = infos[i].size;

        float cross_avail = is_row ? inner_h : inner_w;
        SizeValue cross_sv = is_row ? child->layout.height : child->layout.width;
        Vec2 child_content = child->measure_content(inner_w, inner_h);
        float cross_desired = is_row ? child_content.y : child_content.x;
        float cross_size = resolve_size(cross_sv, cross_avail, cross_desired);

        // Align
        Alignment align = (child->layout.align_self != Alignment::Start)
            ? child->layout.align_self : layout.align_items;
        float cross_offset = 0.0f;
        switch (align) {
            case Alignment::Start:   break;
            case Alignment::Center:  cross_offset = (cross_avail - cross_size) * 0.5f; break;
            case Alignment::End:     cross_offset = cross_avail - cross_size; break;
            case Alignment::Stretch: cross_size = cross_avail; break;
        }

        float margin_before = is_row ? child->layout.margin.left : child->layout.margin.top;
        float margin_after = is_row ? child->layout.margin.right : child->layout.margin.bottom;
        float cross_margin = is_row ? child->layout.margin.top : child->layout.margin.left;

        cursor += margin_before;

        Rect child_rect;
        if (is_row) {
            child_rect = {{cursor, inner_y + cross_offset + cross_margin},
                          {main_size, cross_size}};
        } else {
            child_rect = {{inner_x + cross_offset + cross_margin, cursor},
                          {cross_size, main_size}};
        }

        child->perform_layout(child_rect);
        cursor += main_size + margin_after + layout.spacing + justify_gap;
    }
}

Vec2 Widget::measure_content(float available_width, float available_height) const {
    (void)available_width; (void)available_height;

    if (children_.empty()) return {0.0f, 0.0f};

    // Measure all children and compute content size
    bool is_row = (layout.direction == Direction::Row ||
                   layout.direction == Direction::RowReverse);

    float main_total = 0.0f;
    float cross_max = 0.0f;
    u32 visible_count = 0;

    for (auto& child : children_) {
        if (!child->visible || child->layout.absolute) continue;
        Vec2 child_size = child->measure_content(available_width, available_height);

        // Add resolved size
        SizeValue child_main_sv = is_row ? child->layout.width : child->layout.height;
        float child_main = resolve_size(child_main_sv, is_row ? available_width : available_height,
                                         is_row ? child_size.x : child_size.y);

        SizeValue child_cross_sv = is_row ? child->layout.height : child->layout.width;
        float child_cross = resolve_size(child_cross_sv, is_row ? available_height : available_width,
                                          is_row ? child_size.y : child_size.x);

        main_total += child_main;
        cross_max = std::max(cross_max, child_cross);
        ++visible_count;
    }

    if (visible_count > 1) {
        main_total += layout.spacing * static_cast<float>(visible_count - 1);
    }

    float w = is_row ? main_total : cross_max;
    float h = is_row ? cross_max : main_total;

    w += layout.padding.horizontal();
    h += layout.padding.vertical();

    return {w, h};
}

// ── Draw commands ───────────────────────────────────────────────────────────

void Widget::collect_draw_commands(std::vector<DrawCommand>& commands) const {
    if (!visible) return;

    on_draw(commands);

    for (auto& child : children_) {
        child->collect_draw_commands(commands);
    }
}

void Widget::on_draw(std::vector<DrawCommand>& /*commands*/) const {
    // Base widget: no drawing
}

} // namespace nexus::ui
