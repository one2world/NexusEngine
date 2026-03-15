#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/rhi/rhi_types.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace nexus::ui {

using TextureHandle = nexus::rhi::TextureHandle;
constexpr TextureHandle UI_INVALID_HANDLE = nexus::rhi::INVALID_HANDLE;

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

enum class Direction : u8 { Row, Column, RowReverse, ColumnReverse };
enum class Alignment : u8 { Start, Center, End, Stretch };
enum class Justify : u8 { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };
enum class Overflow : u8 { Visible, Hidden, Scroll };
enum class SizeMode : u8 { Fixed, Percent, Auto, Grow };
enum class Anchor : u8 {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

// ─────────────────────────────────────────────────────────────────────────────
// SizeValue — a value that can be fixed px, percentage, auto, or grow-weight
// ─────────────────────────────────────────────────────────────────────────────

struct SizeValue {
    SizeMode mode{SizeMode::Auto};
    float    value{0.0f};

    static SizeValue px(float v)      { return {SizeMode::Fixed, v}; }
    static SizeValue pct(float v)     { return {SizeMode::Percent, v}; }
    static SizeValue auto_size()      { return {SizeMode::Auto, 0.0f}; }
    static SizeValue grow(float w=1)  { return {SizeMode::Grow, w}; }
};

// ─────────────────────────────────────────────────────────────────────────────
// EdgeInsets — padding/margin for each side
// ─────────────────────────────────────────────────────────────────────────────

struct EdgeInsets {
    float top{0}, right{0}, bottom{0}, left{0};

    EdgeInsets() = default;
    explicit EdgeInsets(float all) : top(all), right(all), bottom(all), left(all) {}
    EdgeInsets(float v, float h) : top(v), right(h), bottom(v), left(h) {}
    EdgeInsets(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
};

// ─────────────────────────────────────────────────────────────────────────────
// NineSlice — 9-slice sprite descriptor for UI panels
// ─────────────────────────────────────────────────────────────────────────────

struct NineSlice {
    TextureHandle texture{UI_INVALID_HANDLE};
    float border_left{0}, border_right{0}, border_top{0}, border_bottom{0};
    Vec2  texture_size{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// UIStyle — visual styling for a widget
// ─────────────────────────────────────────────────────────────────────────────

struct UIStyle {
    Vec4 background_color{0.0f, 0.0f, 0.0f, 0.0f};
    Vec4 border_color{0.5f, 0.5f, 0.5f, 1.0f};
    Vec4 text_color{1.0f, 1.0f, 1.0f, 1.0f};
    float border_width{0.0f};
    float corner_radius{0.0f};
    float font_size{16.0f};
    float opacity{1.0f};

    // Optional 9-slice background
    NineSlice nine_slice{};
};

// ─────────────────────────────────────────────────────────────────────────────
// UITheme — data-driven theming
// ─────────────────────────────────────────────────────────────────────────────

struct UITheme {
    Vec4 primary_color{0.2f, 0.5f, 0.9f, 1.0f};
    Vec4 secondary_color{0.3f, 0.3f, 0.3f, 1.0f};
    Vec4 accent_color{0.9f, 0.6f, 0.1f, 1.0f};
    Vec4 background_color{0.1f, 0.1f, 0.1f, 0.9f};
    Vec4 surface_color{0.15f, 0.15f, 0.15f, 1.0f};
    Vec4 text_color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 text_disabled_color{0.5f, 0.5f, 0.5f, 1.0f};
    Vec4 border_color{0.3f, 0.3f, 0.3f, 1.0f};

    Vec4 button_normal{0.25f, 0.25f, 0.25f, 1.0f};
    Vec4 button_hovered{0.35f, 0.35f, 0.35f, 1.0f};
    Vec4 button_pressed{0.15f, 0.15f, 0.15f, 1.0f};
    Vec4 button_disabled{0.2f, 0.2f, 0.2f, 0.5f};

    Vec4 input_background{0.08f, 0.08f, 0.08f, 1.0f};
    Vec4 input_border{0.3f, 0.3f, 0.3f, 1.0f};
    Vec4 input_focus_border{0.2f, 0.5f, 0.9f, 1.0f};

    Vec4 slider_track{0.2f, 0.2f, 0.2f, 1.0f};
    Vec4 slider_fill{0.2f, 0.5f, 0.9f, 1.0f};
    Vec4 slider_thumb{0.9f, 0.9f, 0.9f, 1.0f};

    Vec4 scrollbar_track{0.1f, 0.1f, 0.1f, 0.5f};
    Vec4 scrollbar_thumb{0.4f, 0.4f, 0.4f, 0.7f};

    Vec4 progress_background{0.2f, 0.2f, 0.2f, 1.0f};
    Vec4 progress_fill{0.2f, 0.7f, 0.3f, 1.0f};

    float default_font_size{16.0f};
    float border_width{1.0f};
    float corner_radius{4.0f};
    float padding{8.0f};
    float spacing{4.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// UIEvent — event types for the UI system
// ─────────────────────────────────────────────────────────────────────────────

enum class UIEventType : u8 {
    None,
    MouseEnter,
    MouseLeave,
    MouseDown,
    MouseUp,
    Click,
    DoubleClick,
    FocusGained,
    FocusLost,
    KeyDown,
    KeyUp,
    TextInput,
    Scroll,
    ValueChanged,
    DragStart,
    DragMove,
    DragEnd,
};

struct UIEvent {
    UIEventType type{UIEventType::None};
    Vec2 mouse_position{0.0f};
    Vec2 mouse_delta{0.0f};
    i32  key_code{0};
    u32  character{0};      // unicode codepoint for TextInput
    float scroll_delta{0.0f};
    bool consumed{false};

    void consume() { consumed = true; }
};

using UICallback = std::function<void(const UIEvent&)>;

// ─────────────────────────────────────────────────────────────────────────────
// LayoutParams — flexbox-like layout properties
// ─────────────────────────────────────────────────────────────────────────────

struct LayoutParams {
    Direction  direction{Direction::Column};
    Alignment  align_items{Alignment::Stretch};
    Alignment  align_self{Alignment::Start};
    Justify    justify{Justify::Start};
    SizeValue  width{SizeValue::auto_size()};
    SizeValue  height{SizeValue::auto_size()};
    SizeValue  min_width{SizeValue::px(0)};
    SizeValue  min_height{SizeValue::px(0)};
    SizeValue  max_width{SizeValue::px(99999)};
    SizeValue  max_height{SizeValue::px(99999)};
    EdgeInsets padding{};
    EdgeInsets margin{};
    float      spacing{0.0f};
    Overflow   overflow_x{Overflow::Visible};
    Overflow   overflow_y{Overflow::Visible};
    Anchor     anchor{Anchor::TopLeft};
    bool       absolute{false};   // if true, taken out of flow
};

// ─────────────────────────────────────────────────────────────────────────────
// Forward
// ─────────────────────────────────────────────────────────────────────────────

class Widget;
using WidgetPtr = std::shared_ptr<Widget>;

// ─────────────────────────────────────────────────────────────────────────────
// Widget — base class for all UI elements
// ─────────────────────────────────────────────────────────────────────────────

class Widget : public std::enable_shared_from_this<Widget> {
public:
    virtual ~Widget() = default;

    /// Unique widget ID.
    u32 id() const { return id_; }

    /// Widget type name (for debugging/theming).
    virtual const char* type_name() const { return "Widget"; }

    // ── Tree ────────────────────────────────────────────────────────────
    void add_child(WidgetPtr child);
    void remove_child(u32 child_id);
    void remove_all_children();
    Widget* parent() const { return parent_; }
    const std::vector<WidgetPtr>& children() const { return children_; }

    // ── Layout ──────────────────────────────────────────────────────────
    LayoutParams layout;
    UIStyle      style;

    /// Computed rect after layout (screen-space).
    Rect computed_rect() const { return computed_rect_; }

    /// Run layout on this subtree.
    void perform_layout(const Rect& available);

    // ── Visibility / Interaction ────────────────────────────────────────
    bool visible{true};
    bool interactive{true};
    bool focusable{false};
    bool focused{false};
    bool hovered{false};
    bool pressed{false};
    bool disabled{false};

    // ── Events ──────────────────────────────────────────────────────────
    void on(UIEventType type, UICallback callback);
    bool dispatch_event(UIEvent& event);

    // ── World-space UI ──────────────────────────────────────────────────
    bool  world_space{false};
    Vec3  world_position{0.0f};
    Vec2  world_size{200.0f, 100.0f};

    // ── Rendering ───────────────────────────────────────────────────────
    struct DrawCommand {
        enum class Type : u8 { Rect, Text, Image, NineSlice };
        Type type;
        Rect rect;
        Vec4 color;
        std::string text;
        float font_size{16.0f};
        float corner_radius{0.0f};
        float border_width{0.0f};
        Vec4 border_color{0.0f};
        TextureHandle texture{UI_INVALID_HANDLE};
        NineSlice nine_slice{};
    };

    /// Collect draw commands for this widget and children.
    void collect_draw_commands(std::vector<DrawCommand>& commands) const;

protected:
    /// Override to provide custom content size.
    virtual Vec2 measure_content(float available_width, float available_height) const;

    /// Override to generate draw commands.
    virtual void on_draw(std::vector<DrawCommand>& commands) const;

    /// Override for custom event handling.
    virtual bool on_event(UIEvent& event);

    Rect computed_rect_{};

private:
    static u32 next_id();

    u32 id_{next_id()};
    Widget* parent_{nullptr};
    std::vector<WidgetPtr> children_;
    std::unordered_map<UIEventType, std::vector<UICallback>> callbacks_;
};

} // namespace nexus::ui
