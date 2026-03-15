#include "nexus/ui/widgets.h"
#include <algorithm>
#include <sstream>

namespace nexus::ui {

// ── Panel ───────────────────────────────────────────────────────────────────

void Panel::on_draw(std::vector<DrawCommand>& commands) const {
    if (style.background_color.a > 0.0f || style.border_width > 0.0f) {
        DrawCommand cmd;
        cmd.type = DrawCommand::Type::Rect;
        cmd.rect = computed_rect_;
        cmd.color = style.background_color;
        cmd.corner_radius = style.corner_radius;
        cmd.border_width = style.border_width;
        cmd.border_color = style.border_color;

        if (style.nine_slice.texture != UI_INVALID_HANDLE) {
            cmd.type = DrawCommand::Type::NineSlice;
            cmd.nine_slice = style.nine_slice;
        }

        commands.push_back(cmd);
    }
}

// ── Label ───────────────────────────────────────────────────────────────────

Vec2 Label::measure_content(float /*available_width*/, float /*available_height*/) const {
    // Approximate text size: ~0.6 * font_size per character width, font_size height
    float char_width = style.font_size * 0.6f;
    float w = static_cast<float>(text.size()) * char_width;
    float h = style.font_size;
    return {w, h};
}

void Label::on_draw(std::vector<DrawCommand>& commands) const {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Text;
    cmd.rect = computed_rect_;
    cmd.color = disabled ? Vec4(0.5f, 0.5f, 0.5f, 1.0f) : style.text_color;
    cmd.text = text;
    cmd.font_size = style.font_size;
    commands.push_back(cmd);
}

// ── Button ──────────────────────────────────────────────────────────────────

Vec2 Button::measure_content(float /*available_width*/, float /*available_height*/) const {
    float char_width = style.font_size * 0.6f;
    float w = static_cast<float>(text.size()) * char_width + 24.0f; // padding
    float h = style.font_size + 16.0f;
    return {w, h};
}

void Button::on_draw(std::vector<DrawCommand>& commands) const {
    // Background
    DrawCommand bg;
    bg.type = DrawCommand::Type::Rect;
    bg.rect = computed_rect_;
    if (disabled) {
        bg.color = Vec4(0.2f, 0.2f, 0.2f, 0.5f);
    } else if (pressed) {
        bg.color = Vec4(0.15f, 0.15f, 0.15f, 1.0f);
    } else if (hovered) {
        bg.color = Vec4(0.35f, 0.35f, 0.35f, 1.0f);
    } else {
        bg.color = style.background_color.a > 0 ? style.background_color
                                                 : Vec4(0.25f, 0.25f, 0.25f, 1.0f);
    }
    bg.corner_radius = style.corner_radius > 0 ? style.corner_radius : 4.0f;
    bg.border_width = style.border_width;
    bg.border_color = focused ? Vec4(0.2f, 0.5f, 0.9f, 1.0f) : style.border_color;
    commands.push_back(bg);

    // Text
    DrawCommand txt;
    txt.type = DrawCommand::Type::Text;
    txt.rect = computed_rect_;
    txt.color = disabled ? Vec4(0.5f, 0.5f, 0.5f, 1.0f) : style.text_color;
    txt.text = text;
    txt.font_size = style.font_size;
    commands.push_back(txt);
}

bool Button::on_event(UIEvent& event) {
    if (disabled) return false;

    if (event.type == UIEventType::Click) {
        if (on_click) {
            on_click(event);
        }
        event.consume();
        return true;
    }
    return false;
}

// ── Slider ──────────────────────────────────────────────────────────────────

void Slider::set_value(float v) {
    v = std::max(min_value, std::min(v, max_value));
    if (step > 0.0f) {
        v = std::round((v - min_value) / step) * step + min_value;
        v = std::max(min_value, std::min(v, max_value));
    }
    value = v;
}

float Slider::normalized() const {
    if (max_value <= min_value) return 0.0f;
    return (value - min_value) / (max_value - min_value);
}

Vec2 Slider::measure_content(float /*available_width*/, float /*available_height*/) const {
    return vertical ? Vec2(24.0f, 120.0f) : Vec2(120.0f, 24.0f);
}

void Slider::on_draw(std::vector<DrawCommand>& commands) const {
    Rect r = computed_rect_;
    float track_height = 4.0f;
    float thumb_size = 16.0f;

    // Track background
    DrawCommand track;
    track.type = DrawCommand::Type::Rect;
    if (vertical) {
        track.rect = {{r.position.x + r.size.x * 0.5f - track_height * 0.5f, r.position.y},
                      {track_height, r.size.y}};
    } else {
        track.rect = {{r.position.x, r.position.y + r.size.y * 0.5f - track_height * 0.5f},
                      {r.size.x, track_height}};
    }
    track.color = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    track.corner_radius = track_height * 0.5f;
    commands.push_back(track);

    // Fill
    float t = normalized();
    DrawCommand fill;
    fill.type = DrawCommand::Type::Rect;
    if (vertical) {
        float fill_h = r.size.y * t;
        fill.rect = {{r.position.x + r.size.x * 0.5f - track_height * 0.5f,
                       r.position.y + r.size.y - fill_h},
                      {track_height, fill_h}};
    } else {
        fill.rect = {{r.position.x, r.position.y + r.size.y * 0.5f - track_height * 0.5f},
                      {r.size.x * t, track_height}};
    }
    fill.color = Vec4(0.2f, 0.5f, 0.9f, 1.0f);
    fill.corner_radius = track_height * 0.5f;
    commands.push_back(fill);

    // Thumb
    DrawCommand thumb;
    thumb.type = DrawCommand::Type::Rect;
    if (vertical) {
        float y = r.position.y + r.size.y * (1.0f - t) - thumb_size * 0.5f;
        thumb.rect = {{r.position.x + r.size.x * 0.5f - thumb_size * 0.5f, y},
                      {thumb_size, thumb_size}};
    } else {
        float x = r.position.x + r.size.x * t - thumb_size * 0.5f;
        thumb.rect = {{x, r.position.y + r.size.y * 0.5f - thumb_size * 0.5f},
                      {thumb_size, thumb_size}};
    }
    thumb.color = (dragging_ || hovered) ? Vec4(1.0f) : Vec4(0.9f, 0.9f, 0.9f, 1.0f);
    thumb.corner_radius = thumb_size * 0.5f;
    commands.push_back(thumb);
}

bool Slider::on_event(UIEvent& event) {
    if (disabled) return false;

    if (event.type == UIEventType::MouseDown) {
        dragging_ = true;
        update_from_mouse(event.mouse_position);
        event.consume();
        return true;
    }
    if (event.type == UIEventType::MouseUp) {
        dragging_ = false;
        return false;
    }
    if (event.type == UIEventType::DragMove && dragging_) {
        update_from_mouse(event.mouse_position);
        event.consume();
        return true;
    }
    return false;
}

void Slider::update_from_mouse(Vec2 pos) {
    Rect r = computed_rect_;
    float t;
    if (vertical) {
        t = 1.0f - (pos.y - r.position.y) / r.size.y;
    } else {
        t = (pos.x - r.position.x) / r.size.x;
    }
    t = std::max(0.0f, std::min(1.0f, t));
    float old_value = value;
    set_value(min_value + t * (max_value - min_value));
    if (value != old_value && on_value_changed) {
        UIEvent ve;
        ve.type = UIEventType::ValueChanged;
        on_value_changed(ve);
    }
}

// ── TextInput ───────────────────────────────────────────────────────────────

void TextInput::set_cursor(u32 pos) {
    cursor_ = std::min(pos, static_cast<u32>(text.size()));
}

void TextInput::select_all() {
    sel_start_ = 0;
    sel_end_ = static_cast<u32>(text.size());
    cursor_ = sel_end_;
}

void TextInput::clear_selection() {
    sel_start_ = sel_end_ = cursor_;
}

void TextInput::insert_char(char c) {
    if (text.size() >= max_length) return;
    if (has_selection()) {
        u32 lo = std::min(sel_start_, sel_end_);
        u32 hi = std::max(sel_start_, sel_end_);
        text.erase(lo, hi - lo);
        cursor_ = lo;
        clear_selection();
    }
    text.insert(text.begin() + static_cast<std::ptrdiff_t>(cursor_), c);
    ++cursor_;
    clear_selection();
}

void TextInput::delete_backward() {
    if (has_selection()) {
        u32 lo = std::min(sel_start_, sel_end_);
        u32 hi = std::max(sel_start_, sel_end_);
        text.erase(lo, hi - lo);
        cursor_ = lo;
        clear_selection();
        return;
    }
    if (cursor_ > 0) {
        text.erase(--cursor_, 1);
    }
}

void TextInput::delete_forward() {
    if (has_selection()) {
        u32 lo = std::min(sel_start_, sel_end_);
        u32 hi = std::max(sel_start_, sel_end_);
        text.erase(lo, hi - lo);
        cursor_ = lo;
        clear_selection();
        return;
    }
    if (cursor_ < text.size()) {
        text.erase(cursor_, 1);
    }
}

Vec2 TextInput::measure_content(float /*available_width*/, float /*available_height*/) const {
    float char_width = style.font_size * 0.6f;
    float w = std::max(100.0f, static_cast<float>(text.size()) * char_width + 16.0f);
    float h = style.font_size + 12.0f;
    return {w, h};
}

void TextInput::on_draw(std::vector<DrawCommand>& commands) const {
    // Background
    DrawCommand bg;
    bg.type = DrawCommand::Type::Rect;
    bg.rect = computed_rect_;
    bg.color = Vec4(0.08f, 0.08f, 0.08f, 1.0f);
    bg.corner_radius = style.corner_radius > 0 ? style.corner_radius : 4.0f;
    bg.border_width = 1.0f;
    bg.border_color = focused ? Vec4(0.2f, 0.5f, 0.9f, 1.0f) : Vec4(0.3f, 0.3f, 0.3f, 1.0f);
    commands.push_back(bg);

    // Text or placeholder
    DrawCommand txt;
    txt.type = DrawCommand::Type::Text;
    txt.rect = computed_rect_;
    txt.rect.position.x += 6.0f;
    txt.rect.size.x -= 12.0f;
    txt.font_size = style.font_size;

    if (text.empty() && !focused) {
        txt.text = placeholder;
        txt.color = Vec4(0.4f, 0.4f, 0.4f, 1.0f);
    } else {
        txt.text = password ? std::string(text.size(), '*') : text;
        txt.color = style.text_color;
    }
    commands.push_back(txt);
}

bool TextInput::on_event(UIEvent& event) {
    if (disabled) return false;

    if (event.type == UIEventType::TextInput) {
        if (event.character >= 32 && event.character < 127) {
            insert_char(static_cast<char>(event.character));
            if (on_text_changed) {
                on_text_changed(event);
            }
            event.consume();
            return true;
        }
    }
    if (event.type == UIEventType::KeyDown) {
        // Backspace
        if (event.key_code == 259) {
            delete_backward();
            if (on_text_changed) on_text_changed(event);
            event.consume();
            return true;
        }
        // Delete
        if (event.key_code == 261) {
            delete_forward();
            if (on_text_changed) on_text_changed(event);
            event.consume();
            return true;
        }
        // Left arrow
        if (event.key_code == 263 && cursor_ > 0) {
            --cursor_;
            clear_selection();
            event.consume();
            return true;
        }
        // Right arrow
        if (event.key_code == 262 && cursor_ < text.size()) {
            ++cursor_;
            clear_selection();
            event.consume();
            return true;
        }
        // Enter
        if (event.key_code == 257 && on_submit) {
            on_submit(event);
            event.consume();
            return true;
        }
    }
    return false;
}

// ── Image ───────────────────────────────────────────────────────────────────

Vec2 Image::measure_content(float /*available_width*/, float /*available_height*/) const {
    return {64.0f, 64.0f};  // default image size
}

void Image::on_draw(std::vector<DrawCommand>& commands) const {
    if (texture == UI_INVALID_HANDLE) return;

    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Image;
    cmd.rect = computed_rect_;
    cmd.color = tint;
    cmd.texture = texture;
    commands.push_back(cmd);
}

// ── ProgressBar ─────────────────────────────────────────────────────────────

Vec2 ProgressBar::measure_content(float /*available_width*/, float /*available_height*/) const {
    return {200.0f, 24.0f};
}

void ProgressBar::on_draw(std::vector<DrawCommand>& commands) const {
    Rect r = computed_rect_;
    float v = std::max(0.0f, std::min(1.0f, value));

    // Background
    DrawCommand bg;
    bg.type = DrawCommand::Type::Rect;
    bg.rect = r;
    bg.color = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    bg.corner_radius = style.corner_radius > 0 ? style.corner_radius : 4.0f;
    commands.push_back(bg);

    // Fill
    if (v > 0.0f) {
        DrawCommand fill;
        fill.type = DrawCommand::Type::Rect;
        fill.rect = {r.position, {r.size.x * v, r.size.y}};
        fill.color = fill_color;
        fill.corner_radius = bg.corner_radius;
        commands.push_back(fill);
    }

    // Text
    if (show_text) {
        DrawCommand txt;
        txt.type = DrawCommand::Type::Text;
        txt.rect = r;
        txt.color = style.text_color;
        int pct = static_cast<int>(v * 100.0f);
        txt.text = std::to_string(pct) + "%";
        txt.font_size = style.font_size > 0 ? style.font_size : 14.0f;
        commands.push_back(txt);
    }
}

// ── ScrollView ──────────────────────────────────────────────────────────────

void ScrollView::scroll_to(Vec2 target) {
    scroll_offset = target;
}

void ScrollView::scroll_to_top() {
    scroll_offset.y = 0.0f;
}

void ScrollView::scroll_to_bottom() {
    float max_scroll = content_size.y - computed_rect_.size.y;
    scroll_offset.y = std::max(0.0f, max_scroll);
}

void ScrollView::on_draw(std::vector<DrawCommand>& commands) const {
    // Background
    if (style.background_color.a > 0) {
        DrawCommand bg;
        bg.type = DrawCommand::Type::Rect;
        bg.rect = computed_rect_;
        bg.color = style.background_color;
        commands.push_back(bg);
    }

    // Scrollbar (vertical)
    if (scroll_y && content_size.y > computed_rect_.size.y) {
        float ratio = computed_rect_.size.y / content_size.y;
        float bar_height = computed_rect_.size.y * ratio;
        float bar_y = (scroll_offset.y / content_size.y) * computed_rect_.size.y;
        float bar_width = 6.0f;

        // Track
        DrawCommand track;
        track.type = DrawCommand::Type::Rect;
        track.rect = {{computed_rect_.right() - bar_width, computed_rect_.position.y},
                       {bar_width, computed_rect_.size.y}};
        track.color = Vec4(0.1f, 0.1f, 0.1f, 0.5f);
        commands.push_back(track);

        // Thumb
        DrawCommand thumb;
        thumb.type = DrawCommand::Type::Rect;
        thumb.rect = {{computed_rect_.right() - bar_width,
                        computed_rect_.position.y + bar_y},
                       {bar_width, bar_height}};
        thumb.color = Vec4(0.4f, 0.4f, 0.4f, 0.7f);
        thumb.corner_radius = bar_width * 0.5f;
        commands.push_back(thumb);
    }
}

bool ScrollView::on_event(UIEvent& event) {
    if (event.type == UIEventType::Scroll) {
        if (scroll_y) {
            scroll_offset.y -= event.scroll_delta * scroll_speed;
            float max_scroll = std::max(0.0f, content_size.y - computed_rect_.size.y);
            scroll_offset.y = std::max(0.0f, std::min(scroll_offset.y, max_scroll));
            event.consume();
            return true;
        }
    }
    return false;
}

} // namespace nexus::ui
