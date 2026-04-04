#pragma once

#include "nexus/core/types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace nexus::platform {

// ============================================================================
// Build & Export Pipeline - platform export configuration and asset cooking
// ============================================================================

enum class TargetPlatform : u8 {
    Windows,
    Linux,
    MacOS,
    Android,
    iOS,
    WebGL,
};

enum class GraphicsAPI : u8 {
    OpenGL,
    Vulkan,
    Metal,
    WebGL2,
    GLES3,
};

enum class BuildConfig : u8 {
    Debug,
    Release,
    Distribution,   // optimized + stripped
};

enum class TextureCompression : u8 {
    None,
    BC1,             // DXT1 - desktop
    BC3,             // DXT5 - desktop
    BC7,             // best quality desktop
    ETC2,            // mobile (Android/iOS)
    ASTC,            // high-quality mobile
};

struct ExportProfile {
    std::string name;
    TargetPlatform platform{TargetPlatform::Windows};
    GraphicsAPI graphics_api{GraphicsAPI::OpenGL};
    BuildConfig config{BuildConfig::Release};

    // Output settings
    std::string output_directory{"export/"};
    std::string executable_name{"game"};
    bool strip_debug_symbols{true};
    bool embed_assets{false};  // embed assets in executable

    // Asset cooking
    TextureCompression texture_compression{TextureCompression::None};
    bool compress_textures{true};
    bool generate_mipmaps{true};
    u32 max_texture_size{4096};
    bool strip_unused_assets{true};

    // Platform-specific
    std::string android_package_name;   // e.g. "com.studio.game"
    u32 android_min_sdk{21};
    u32 android_target_sdk{33};
    std::string ios_bundle_id;
    std::string ios_team_id;

    // Web-specific
    u32 web_memory_mb{256};
    bool web_threading{false};
    std::string web_shell_template;
};

// ============================================================================
// AssetCooker - processes assets for target platform
// ============================================================================

struct CookResult {
    std::string asset_path;
    std::string output_path;
    u64 original_size{0};
    u64 cooked_size{0};
    bool success{true};
    std::string error;
};

class AssetCooker {
public:
    /// Configure for a target platform.
    void set_profile(const ExportProfile& profile) { profile_ = profile; }

    /// Cook a single asset. Returns cooking result.
    CookResult cook_asset(const std::string& input_path, const std::string& output_dir) const;

    /// Cook all assets in a directory recursively.
    std::vector<CookResult> cook_directory(const std::string& input_dir,
                                            const std::string& output_dir) const;

    /// Get recommended texture compression for the target platform.
    static TextureCompression default_compression(TargetPlatform platform);

    /// Get recommended graphics API for the target platform.
    static GraphicsAPI default_graphics_api(TargetPlatform platform);

    /// Get file extension for the target platform executable.
    static std::string executable_extension(TargetPlatform platform);

private:
    ExportProfile profile_;
};

// ============================================================================
// BuildPipeline - orchestrates the full build process
// ============================================================================

struct BuildStep {
    std::string name;
    std::string description;
    enum Status : u8 { Pending, Running, Success, Failed, Skipped };
    Status status{Pending};
    float progress{0.0f};
    std::string error_message;
};

class BuildPipeline {
public:
    /// Configure build with an export profile.
    void configure(const ExportProfile& profile);

    /// Get all build steps.
    const std::vector<BuildStep>& steps() const { return steps_; }

    /// Run all build steps sequentially. Returns true if all succeed.
    bool run();

    /// Get overall progress [0, 1].
    float progress() const;

    /// Get the currently running step index (-1 if not running).
    i32 current_step() const { return current_step_; }

    /// Check if the build completed.
    bool is_complete() const { return complete_; }
    bool is_successful() const;

    /// Get the final output path.
    const std::string& output_path() const { return output_path_; }

    /// Registered profiles.
    void add_profile(const ExportProfile& profile);
    const std::vector<ExportProfile>& profiles() const { return profiles_; }

private:
    ExportProfile profile_;
    std::vector<BuildStep> steps_;
    std::vector<ExportProfile> profiles_;
    i32 current_step_{-1};
    bool complete_{false};
    std::string output_path_;
};

} // namespace nexus::platform
