#pragma once

/// Android NDK native activity bridge for NexusEngine.
///
/// Provides the interface between the Android Java/Kotlin activity and
/// the C++ engine. On non-Android platforms, this is a no-op stub.
///
/// Architecture:
///   Java Activity -> NativeActivity -> ANativeActivity_onCreate
///     -> nexus::platform::AndroidApp::on_create()
///     -> Main loop via ALooper + pipe
///
/// The bridge handles:
///   - Lifecycle callbacks (create, start, resume, pause, stop, destroy)
///   - Window surface management (native window creation/destruction)
///   - Input event forwarding (touch → TouchInput, keys → Input)
///   - Asset access via AAssetManager
///   - Display metrics (DPI, safe area)

#include <nexus/core/types.h>
#include <string>
#include <functional>

#ifdef __ANDROID__
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android/configuration.h>
#endif

namespace nexus::platform {

/// Android display information.
struct AndroidDisplayInfo {
    i32 width{0};
    i32 height{0};
    f32 dpi{160.0f};
    f32 density{1.0f};
    i32 safe_inset_top{0};
    i32 safe_inset_bottom{0};
    i32 safe_inset_left{0};
    i32 safe_inset_right{0};
};

/// Android application lifecycle state.
enum class AndroidLifecycleState : u8 {
    Created,
    Started,
    Resumed,
    Paused,
    Stopped,
    Destroyed
};

/// Android application bridge.
///
/// Manages the Android native activity lifecycle and provides an
/// interface to the engine's platform layer.
class AndroidApp {
public:
    using LifecycleCallback = std::function<void(AndroidLifecycleState)>;
    using ResizeCallback = std::function<void(i32 width, i32 height)>;

    AndroidApp() = default;
    ~AndroidApp() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────

    /// Initialize from ANativeActivity (called from ANativeActivity_onCreate).
    bool init(void* native_activity);

    /// Shut down and release all Android resources.
    void shutdown();

    /// Process pending events (call from main loop).
    void poll_events();

    /// Get current lifecycle state.
    [[nodiscard]] AndroidLifecycleState state() const { return state_; }

    /// Whether the app has a valid rendering surface.
    [[nodiscard]] bool has_window() const { return has_window_; }

    /// Whether the app is in the foreground and can render.
    [[nodiscard]] bool is_active() const {
        return state_ == AndroidLifecycleState::Resumed && has_window_;
    }

    // ── Display ───────────────────────────────────────────────────────

    /// Get display metrics (DPI, resolution, safe area).
    [[nodiscard]] AndroidDisplayInfo display_info() const { return display_info_; }

    // ── Assets ────────────────────────────────────────────────────────

    /// Read an asset file from the APK's assets/ directory.
    /// Returns empty vector if the file doesn't exist.
    std::vector<u8> read_asset(const std::string& path) const;

    /// Check if an asset exists.
    bool asset_exists(const std::string& path) const;

    // ── Callbacks ─────────────────────────────────────────────────────

    /// Register a lifecycle state change callback.
    void set_lifecycle_callback(LifecycleCallback cb) {
        lifecycle_cb_ = std::move(cb);
    }

    /// Register a window resize callback.
    void set_resize_callback(ResizeCallback cb) {
        resize_cb_ = std::move(cb);
    }

    // ── Internal paths ────────────────────────────────────────────────

    /// Get the app's internal storage path.
    [[nodiscard]] const std::string& internal_path() const { return internal_path_; }

    /// Get the app's external storage path.
    [[nodiscard]] const std::string& external_path() const { return external_path_; }

private:
    void transition_state(AndroidLifecycleState new_state);

#ifdef __ANDROID__
    // Android NDK callbacks (static, forwarded to instance)
    static void on_start(ANativeActivity* activity);
    static void on_resume(ANativeActivity* activity);
    static void on_pause(ANativeActivity* activity);
    static void on_stop(ANativeActivity* activity);
    static void on_destroy(ANativeActivity* activity);
    static void on_window_created(ANativeActivity* activity, ANativeWindow* window);
    static void on_window_destroyed(ANativeActivity* activity, ANativeWindow* window);
    static void on_window_resized(ANativeActivity* activity, ANativeWindow* window);
    static i32  on_input_event(ANativeActivity* activity, AInputEvent* event);

    ANativeActivity*  activity_{nullptr};
    AAssetManager*    asset_mgr_{nullptr};
    ANativeWindow*    window_{nullptr};
    AConfiguration*   config_{nullptr};
    ALooper*          looper_{nullptr};
#endif

    AndroidLifecycleState state_{AndroidLifecycleState::Created};
    AndroidDisplayInfo display_info_;
    bool has_window_{false};
    bool initialized_{false};
    std::string internal_path_;
    std::string external_path_;

    LifecycleCallback lifecycle_cb_;
    ResizeCallback resize_cb_;
};

} // namespace nexus::platform
