#pragma once

namespace nexus::editor {

/// Plug ScriptComponent into SceneSerializer's extension hook so scenes
/// round-trip the script binding (script_name + enabled) without
/// requiring engine/scene to depend on engine/scripting.  Idempotent at
/// the per-process level: call once at editor / runtime startup.  See
/// editor/src/script_serializer_ext.cpp for the writer/reader pair.
void install_script_serializer_extension();

}  // namespace nexus::editor
