#include "nexus/platform/build_export.h"
#include "nexus/core/log.h"
#include <algorithm>
#include <filesystem>

namespace nexus::platform {

namespace fs = std::filesystem;

// ── AssetCooker ────────────────────────────────────────────────────────────

TextureCompression AssetCooker::default_compression(TargetPlatform platform) {
    switch (platform) {
        case TargetPlatform::Windows:
        case TargetPlatform::Linux:
        case TargetPlatform::MacOS:
            return TextureCompression::BC7;
        case TargetPlatform::Android:
        case TargetPlatform::iOS:
            return TextureCompression::ASTC;
        case TargetPlatform::WebGL:
            return TextureCompression::ETC2;
    }
    return TextureCompression::None;
}

GraphicsAPI AssetCooker::default_graphics_api(TargetPlatform platform) {
    switch (platform) {
        case TargetPlatform::Windows:
        case TargetPlatform::Linux:
            return GraphicsAPI::Vulkan;
        case TargetPlatform::MacOS:
        case TargetPlatform::iOS:
            return GraphicsAPI::Metal;
        case TargetPlatform::Android:
            return GraphicsAPI::GLES3;
        case TargetPlatform::WebGL:
            return GraphicsAPI::WebGL2;
    }
    return GraphicsAPI::OpenGL;
}

std::string AssetCooker::executable_extension(TargetPlatform platform) {
    switch (platform) {
        case TargetPlatform::Windows: return ".exe";
        case TargetPlatform::Linux:   return "";
        case TargetPlatform::MacOS:   return ".app";
        case TargetPlatform::Android: return ".apk";
        case TargetPlatform::iOS:     return ".ipa";
        case TargetPlatform::WebGL:   return ".html";
    }
    return "";
}

CookResult AssetCooker::cook_asset(const std::string& input_path,
                                    const std::string& output_dir) const {
    CookResult result;
    result.asset_path = input_path;

    // Determine output path
    fs::path in_path(input_path);
    fs::path out_path = fs::path(output_dir) / in_path.filename();
    result.output_path = out_path.string();

    // Get file size
    std::error_code ec;
    auto file_size = fs::file_size(in_path, ec);
    if (ec) {
        result.success = false;
        result.error = "File not found: " + input_path;
        return result;
    }
    result.original_size = file_size;

    // Determine asset type from extension
    std::string ext = in_path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // For texture assets, apply compression settings
    if (ext == ".bmp" || ext == ".tga" || ext == ".png" || ext == ".jpg" || ext == ".hdr") {
        // In a real implementation:
        // 1. Load texture
        // 2. Generate mipmaps if profile_.generate_mipmaps
        // 3. Resize if larger than profile_.max_texture_size
        // 4. Compress with profile_.texture_compression
        // 5. Write to output
        // TODO(1.1): integrate GPU texture compression (BCn/ASTC/ETC2)
        // Currently passes through uncompressed; mipmap generation pending.
        NX_INFO("Cook texture: {} → {} (compression: {}, pass-through)",
                input_path, result.output_path,
                static_cast<int>(profile_.texture_compression));
        result.cooked_size = file_size;
    }
    // For other asset types, copy or process
    else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
        NX_INFO("Cook mesh: {} → {}", input_path, result.output_path);
        result.cooked_size = file_size;
    }
    else if (ext == ".wav" || ext == ".ogg") {
        NX_INFO("Cook audio: {} → {}", input_path, result.output_path);
        result.cooked_size = file_size;
    }
    else if (ext == ".lua") {
        NX_INFO("Cook script: {} → {}", input_path, result.output_path);
        result.cooked_size = file_size;
    }
    else {
        // Pass through
        result.cooked_size = file_size;
    }

    result.success = true;
    return result;
}

std::vector<CookResult> AssetCooker::cook_directory(const std::string& input_dir,
                                                      const std::string& output_dir) const {
    std::vector<CookResult> results;
    std::error_code ec;

    if (!fs::exists(input_dir, ec)) {
        CookResult err;
        err.success = false;
        err.error = "Directory not found: " + input_dir;
        results.push_back(err);
        return results;
    }

    for (const auto& entry : fs::recursive_directory_iterator(input_dir, ec)) {
        if (entry.is_regular_file()) {
            // Compute relative output path preserving directory structure
            auto rel = fs::relative(entry.path(), input_dir, ec);
            fs::path out_path = fs::path(output_dir) / rel;

            auto result = cook_asset(entry.path().string(), out_path.parent_path().string());
            results.push_back(std::move(result));
        }
    }

    NX_INFO("Cooked {} assets from '{}'", results.size(), input_dir);
    return results;
}

// ── BuildPipeline ──────────────────────────────────────────────────────────

static BuildStep make_step(const std::string& name, const std::string& desc) {
    BuildStep s;
    s.name = name;
    s.description = desc;
    return s;
}

void BuildPipeline::configure(const ExportProfile& profile) {
    profile_ = profile;
    steps_.clear();
    current_step_ = -1;
    complete_ = false;

    // Define build steps based on platform
    steps_.push_back(make_step("validate", "Validate project configuration"));
    steps_.push_back(make_step("cook_assets", "Cook and compress assets"));
    steps_.push_back(make_step("compile_shaders", "Compile shaders for target platform"));
    steps_.push_back(make_step("build_code", "Build engine and game code"));
    steps_.push_back(make_step("package", "Package executable and assets"));

    if (profile.strip_debug_symbols && profile.config != BuildConfig::Debug) {
        steps_.push_back(make_step("strip", "Strip debug symbols"));
    }

    if (profile.platform == TargetPlatform::Android) {
        steps_.push_back(make_step("sign_apk", "Sign APK for distribution"));
    } else if (profile.platform == TargetPlatform::iOS) {
        steps_.push_back(make_step("sign_ipa", "Code sign for iOS"));
    } else if (profile.platform == TargetPlatform::WebGL) {
        steps_.push_back(make_step("generate_html", "Generate HTML shell and service worker"));
    }

    steps_.push_back(make_step("finalize", "Finalize build output"));

    output_path_ = profile.output_directory + "/" + profile.executable_name
                 + AssetCooker::executable_extension(profile.platform);

    NX_INFO("Build pipeline configured: {} steps for platform {}",
            steps_.size(), static_cast<int>(profile.platform));
}

bool BuildPipeline::run() {
    NX_INFO("Starting build pipeline: '{}'", profile_.name);

    for (u32 i = 0; i < steps_.size(); ++i) {
        current_step_ = static_cast<i32>(i);
        steps_[i].status = BuildStep::Running;

        NX_INFO("  [{}/{}] {}", i + 1, steps_.size(), steps_[i].description);

        // Execute step (in a real implementation, each step would do real work)
        // Here we simulate success for all steps.
        steps_[i].progress = 1.0f;
        steps_[i].status = BuildStep::Success;
    }

    complete_ = true;
    current_step_ = -1;

    bool all_ok = is_successful();
    if (all_ok) {
        NX_INFO("Build complete: {}", output_path_);
    } else {
        NX_ERROR("Build failed");
    }
    return all_ok;
}

float BuildPipeline::progress() const {
    if (steps_.empty()) return 0.0f;
    float total = 0.0f;
    for (const auto& step : steps_) {
        if (step.status == BuildStep::Success || step.status == BuildStep::Skipped) {
            total += 1.0f;
        } else if (step.status == BuildStep::Running) {
            total += step.progress;
        }
    }
    return total / static_cast<float>(steps_.size());
}

bool BuildPipeline::is_successful() const {
    for (const auto& step : steps_) {
        if (step.status == BuildStep::Failed) return false;
    }
    return complete_;
}

void BuildPipeline::add_profile(const ExportProfile& profile) {
    profiles_.push_back(profile);
}

} // namespace nexus::platform
