#pragma once

#include "nexus/ui/ui_core.h"
#include <string>

namespace nexus::ui {

// ─────────────────────────────────────────────────────────────────────────────
// Panel — container widget (flexbox container)
// ─────────────────────────────────────────────────────────────────────────────

class Panel : public Widget {
public:
    const char* type_name() const override { return "Panel"; }

protected:
    void on_draw(std::vector<DrawCommand>& commands) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// Label — text display
// ─────────────────────────────────────────────────────────────────────────────

class Label : public Widget {
public:
    const char* type_name() const override { return "Label"; }

    std::string text;
    bool wrap{false};

    explicit Label(std::string t = "") : text(std::move(t)) {}

protected:
    Vec2 measure_content(float available_width, float available_height) const override;
    void on_draw(std::vector<DrawCommand>& commands) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// Button — clickable button
// ─────────────────────────────────────────────────────────────────────────────

class Button : public Widget {
public:
    const char* type_name() const override { return "Button"; }

    std::string text;
    UICallback on_click;

    explicit Button(std::string t = "") : text(std::move(t)) {
        focusable = true;
        interactive = true;
    }

protected:
    Vec2 measure_content(float available_width, float available_height) const override;
    void on_draw(std::vector<DrawCommand>& commands) const override;
    bool on_event(UIEvent& event) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// Slider — horizontal/vertical slider
// ─────────────────────────────────────────────────────────────────────────────

class Slider : public Widget {
public:
    const char* type_name() const override { return "Slider"; }

    float value{0.0f};
    float min_value{0.0f};
    float max_value{1.0f};
    float step{0.0f};          // 0 = continuous
    bool  vertical{false};

    UICallback on_value_changed;

    Slider() {
        focusable = true;
        interactive = true;
    }

    /// Set value, clamped and stepped.
    void set_value(float v);

    /// Normalized value [0, 1].
    float normalized() const;

protected:
    Vec2 measure_content(float available_width, float available_height) const override;
    void on_draw(std::vector<DrawCommand>& commands) const override;
    bool on_event(UIEvent& event) override;

private:
    bool dragging_{false};
    void update_from_mouse(Vec2 pos);
};

// ─────────────────────────────────────────────────────────────────────────────
// TextInput — single-line text entry
// ─────────────────────────────────────────────────────────────────────────────

class TextInput : public Widget {
public:
    const char* type_name() const override { return "TextInput"; }

    std::string text;
    std::string placeholder;
    u32 max_length{256};
    bool password{false};

    UICallback on_text_changed;
    UICallback on_submit;

    TextInput() {
        focusable = true;
        interactive = true;
    }

    /// Cursor position.
    u32  cursor() const { return cursor_; }
    void set_cursor(u32 pos);

    /// Selection.
    u32  selection_start() const { return sel_start_; }
    u32  selection_end() const { return sel_end_; }
    bool has_selection() const { return sel_start_ != sel_end_; }
    void select_all();
    void clear_selection();

    /// Edit operations.
    void insert_char(char c);
    void delete_backward();
    void delete_forward();

protected:
    Vec2 measure_content(float available_width, float available_height) const override;
    void on_draw(std::vector<DrawCommand>& commands) const override;
    bool on_event(UIEvent& event) override;

private:
    u32 cursor_{0};
    u32 sel_start_{0};
    u32 sel_end_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// Image — displays a texture
// ─────────────────────────────────────────────────────────────────────────────

class Image : public Widget {
public:
    const char* type_name() const override { return "Image"; }

    TextureHandle texture{UI_INVALID_HANDLE};
    Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 uv_rect{0.0f, 0.0f, 1.0f, 1.0f};   // x,y = offset, z,w = size

protected:
    Vec2 measure_content(float available_width, float available_height) const override;
    void on_draw(std::vector<DrawCommand>& commands) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// ProgressBar — displays a progress value
// ─────────────────────────────────────────────────────────────────────────────

class ProgressBar : public Widget {
public:
    const char* type_name() const override { return "ProgressBar"; }

    float value{0.0f};     // [0, 1]
    bool  show_text{true};
    Vec4  fill_color{0.2f, 0.7f, 0.3f, 1.0f};

protected:
    Vec2 measure_content(float available_width, float available_height) const override;
    void on_draw(std::vector<DrawCommand>& commands) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// ScrollView — scrollable container
// ─────────────────────────────────────────────────────────────────────────────

class ScrollView : public Widget {
public:
    const char* type_name() const override { return "ScrollView"; }

    Vec2 scroll_offset{0.0f};
    Vec2 content_size{0.0f};     // auto-computed from children
    bool scroll_x{false};
    bool scroll_y{true};
    float scroll_speed{20.0f};

    ScrollView() {
        interactive = true;
        layout.overflow_x = Overflow::Hidden;
        layout.overflow_y = Overflow::Scroll;
    }

    void scroll_to(Vec2 target);
    void scroll_to_top();
    void scroll_to_bottom();

protected:
    void on_draw(std::vector<DrawCommand>& commands) const override;
    bool on_event(UIEvent& event) override;
};

} // namespace nexus::ui
