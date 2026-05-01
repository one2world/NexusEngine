#pragma once

#include "nexus/core/types.h"
#include "nexus/scene/registry.h"

#include <functional>
#include <string>
#include <vector>

namespace nexus::editor {

// ─────────────────────────────────────────────────────────────────────────────
// ComponentRegistry — type-erased editor metadata for ECS components
// ─────────────────────────────────────────────────────────────────────────────
//
// Drives the Inspector's "Add Component" menu without coupling the editor
// to every concrete component type.  Each entry binds a display name +
// category + 3 closures: `has(entity)`, `add(entity)` and `remove(entity)`.
// The registry itself never includes <imgui.h> so the unit tests can drive
// it headlessly.
//
// Design notes
// ─────────────
// • Categories follow Unity's grouping (Mesh / Effects / Physics / Audio /
//   Animation / Navigation / Rendering / Layout / Scripts) so the menu
//   layout matches user expectations from screenshots.
// • Built-in entries are registered by `register_builtin_components()` so
//   the list lives in one place and stays in sync with the engine.
// • Custom user components register themselves via `register_component<C>()`
//   in their own TU — the editor doesn't need to know about them at compile
//   time.
struct ComponentDescriptor {
    std::string  name;          // user-visible label, e.g. "Mesh Renderer"
    std::string  category;      // group header, e.g. "Mesh"
    std::string  type_id;       // stable lookup key, e.g. "MeshRenderer"
    // Whether the entity already carries this component.
    std::function<bool(Registry&, u64)> has;
    // Add a default-constructed component to the entity.  No-op if has() is
    // already true (callers should check first to avoid double-add).
    std::function<void(Registry&, u64)> add;
    // Remove the component from the entity.  No-op if not present.
    std::function<void(Registry&, u64)> remove;
};

class ComponentRegistry {
public:
    ComponentRegistry() = default;

    // Register a component type.  Templated convenience that fills in the
    // three closures via Registry's component-erased API.  Pass the user-
    // visible name and the menu category (e.g. "Mesh", "Physics").
    template <typename T>
    void register_type(std::string name, std::string category) {
        ComponentDescriptor d;
        d.name     = std::move(name);
        d.category = std::move(category);
        d.type_id  = d.name;
        d.has = [](Registry& reg, u64 e) {
            return reg.has_component<T>(static_cast<Entity>(e));
        };
        d.add = [](Registry& reg, u64 e) {
            if (!reg.has_component<T>(static_cast<Entity>(e))) {
                reg.add_component<T>(static_cast<Entity>(e), T{});
            }
        };
        d.remove = [](Registry& reg, u64 e) {
            if (reg.has_component<T>(static_cast<Entity>(e))) {
                reg.remove_component<T>(static_cast<Entity>(e));
            }
        };
        descriptors_.push_back(std::move(d));
    }

    // Generic register — for tests or custom descriptor wiring.
    void register_descriptor(ComponentDescriptor d) {
        descriptors_.push_back(std::move(d));
    }

    // All registered descriptors.  Order is registration order — categories
    // remain grouped for the menu by stable_partition / sort by category.
    const std::vector<ComponentDescriptor>& descriptors() const { return descriptors_; }
    u32 size() const { return static_cast<u32>(descriptors_.size()); }

    // Lookup by exact type_id; returns nullptr if not registered.
    const ComponentDescriptor* find(const std::string& type_id) const;

    // Filter helpers — used by the Inspector's search box.  Both treat the
    // query as case-insensitive substring on `name`.  `available_to(...)`
    // additionally hides descriptors whose `has(reg, e)` returns true so
    // the user can't double-add the same component.
    std::vector<const ComponentDescriptor*>
    matching(const std::string& query) const;

    std::vector<const ComponentDescriptor*>
    available_to(Registry& reg, u64 entity, const std::string& query) const;

    // List unique category names in registration order — drives section
    // headers in the search popup.
    std::vector<std::string> categories() const;

    // Clear registry (test-only).
    void clear() { descriptors_.clear(); }

private:
    std::vector<ComponentDescriptor> descriptors_;
};

// Populate a registry with the engine's built-in component types.  Called
// from editor_main.cpp at startup.  Kept out-of-line so user code that only
// includes the header doesn't pay the cost of pulling in every component.
void register_builtin_components(ComponentRegistry& reg);

}  // namespace nexus::editor
