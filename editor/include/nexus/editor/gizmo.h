#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include "nexus/renderer/debug_renderer.h"
#include "nexus/renderer/camera.h"

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// GizmoMode — the type of transform manipulation
// ─────────────────────────────────────────────────────────────────────────────

enum class GizmoMode : u8 { Translate, Rotate, Scale };
enum class GizmoSpace : u8 { Local, World };
enum class GizmoAxis : u8 { None, X, Y, Z, XY, XZ, YZ };

// ─────────────────────────────────────────────────────────────────────────────
// Gizmo — 3D transform manipulation handles drawn via DebugRenderer
// ─────────────────────────────────────────────────────────────────────────────

class Gizmo {
public:
    Gizmo() = default;

    /// Set the mode (translate/rotate/scale).
    void set_mode(GizmoMode mode) { mode_ = mode; }
    GizmoMode mode() const { return mode_; }

    /// Set the coordinate space.
    void set_space(GizmoSpace space) { space_ = space; }
    GizmoSpace space() const { return space_; }

    /// Draw the gizmo at a position with an orientation.
    void draw(DebugRenderer& debug, const Camera3D& camera,
              Vec3 position, Quat orientation = Quat{1, 0, 0, 0}) const;

    /// Test if a screen-space ray hits any gizmo axis.
    GizmoAxis hit_test(const Camera3D& camera, Vec3 gizmo_position,
                       Vec2 screen_pos, Vec2 screen_size) const;

    /// Begin dragging on an axis.
    void begin_drag(GizmoAxis axis, Vec3 start_position, Vec2 mouse_pos);

    /// Update drag and compute delta transform.
    Vec3 update_drag(const Camera3D& camera, Vec3 gizmo_position,
                     Vec2 mouse_pos, Vec2 screen_size);

    /// End dragging.
    void end_drag();

    /// Whether currently dragging.
    bool is_dragging() const { return dragging_; }
    GizmoAxis active_axis() const { return active_axis_; }

    /// Size of gizmo handles (in world units, scaled by distance).
    float handle_size{1.0f};

private:
    GizmoMode mode_{GizmoMode::Translate};
    GizmoSpace space_{GizmoSpace::World};
    GizmoAxis active_axis_{GizmoAxis::None};
    bool dragging_{false};
    Vec3 drag_start_{0.0f};
    Vec2 drag_mouse_start_{0.0f};
};

} // namespace nexus::editor
