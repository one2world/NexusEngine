#include "nexus/platform/android_bridge.h"
#include "nexus/core/log.h"

#include <cstring>

namespace nexus::platform {

// ── Lifecycle ───────────────────────────────────────────────────────────────

bool AndroidApp::init(void* native_activity) {
    if (initialized_) return true;

#ifdef __ANDROID__
    activity_ = static_cast<ANativeActivity*>(native_activity);
    if (!activity_) {
        NX_ERROR("AndroidApp: null native activity");
        return false;
    }

    // Store reference to this app instance
    activity_->instance = this;

    // Set up lifecycle callbacks
    activity_->callbacks->onStart   = on_start;
    activity_->callbacks->onResume  = on_resume;
    activity_->callbacks->onPause   = on_pause;
    activity_->callbacks->onStop    = on_stop;
    activity_->callbacks->onDestroy = on_destroy;
    activity_->callbacks->onNativeWindowCreated   = on_window_created;
    activity_->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    activity_->callbacks->onNativeWindowResized   = on_window_resized;
    activity_->callbacks->onInputQueueCreated = nullptr; // handled via ALooper
    activity_->callbacks->onInputQueueDestroyed = nullptr;

    // Asset manager
    asset_mgr_ = activity_->assetManager;

    // Configuration
    config_ = AConfiguration_new();
    AConfiguration_fromAssetManager(config_, asset_mgr_);

    // Display density
    display_info_.dpi = static_cast<f32>(AConfiguration_getDensity(config_));
    display_info_.density = display_info_.dpi / 160.0f;

    // Storage paths
    if (activity_->internalDataPath) {
        internal_path_ = activity_->internalDataPath;
    }
    if (activity_->externalDataPath) {
        external_path_ = activity_->externalDataPath;
    }

    // Looper for event processing
    looper_ = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

    NX_INFO("Android app initialized (DPI: {}, density: {})",
            display_info_.dpi, display_info_.density);
#else
    (void)native_activity;
    NX_INFO("AndroidApp::init() called on non-Android platform (stub)");
    internal_path_ = "./data";
    external_path_ = "./data";
#endif

    initialized_ = true;
    return true;
}

void AndroidApp::shutdown() {
    if (!initialized_) return;

#ifdef __ANDROID__
    if (config_) {
        AConfiguration_delete(config_);
        config_ = nullptr;
    }
    window_ = nullptr;
    activity_ = nullptr;
    asset_mgr_ = nullptr;
    looper_ = nullptr;
#endif

    has_window_ = false;
    initialized_ = false;
    state_ = AndroidLifecycleState::Destroyed;
    NX_INFO("Android app shut down");
}

void AndroidApp::poll_events() {
#ifdef __ANDROID__
    int events;
    void* data;
    while (ALooper_pollAll(0, nullptr, &events, &data) >= 0) {
        // Events are processed via callbacks
    }
#endif
}

void AndroidApp::transition_state(AndroidLifecycleState new_state) {
    state_ = new_state;
    if (lifecycle_cb_) lifecycle_cb_(new_state);
}

// ── Assets ──────────────────────────────────────────────────────────────────

std::vector<u8> AndroidApp::read_asset(const std::string& path) const {
#ifdef __ANDROID__
    if (!asset_mgr_) return {};

    AAsset* asset = AAssetManager_open(asset_mgr_, path.c_str(),
                                        AASSET_MODE_BUFFER);
    if (!asset) return {};

    size_t size = static_cast<size_t>(AAsset_getLength(asset));
    std::vector<u8> data(size);
    AAsset_read(asset, data.data(), size);
    AAsset_close(asset);
    return data;
#else
    (void)path;
    return {};
#endif
}

bool AndroidApp::asset_exists(const std::string& path) const {
#ifdef __ANDROID__
    if (!asset_mgr_) return false;
    AAsset* asset = AAssetManager_open(asset_mgr_, path.c_str(),
                                        AASSET_MODE_UNKNOWN);
    if (!asset) return false;
    AAsset_close(asset);
    return true;
#else
    (void)path;
    return false;
#endif
}

// ── Android NDK Callbacks ───────────────────────────────────────────────────

#ifdef __ANDROID__
void AndroidApp::on_start(ANativeActivity* activity) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    NX_INFO("Android: onStart");
    app->transition_state(AndroidLifecycleState::Started);
}

void AndroidApp::on_resume(ANativeActivity* activity) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    NX_INFO("Android: onResume");
    app->transition_state(AndroidLifecycleState::Resumed);
}

void AndroidApp::on_pause(ANativeActivity* activity) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    NX_INFO("Android: onPause");
    app->transition_state(AndroidLifecycleState::Paused);
}

void AndroidApp::on_stop(ANativeActivity* activity) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    NX_INFO("Android: onStop");
    app->transition_state(AndroidLifecycleState::Stopped);
}

void AndroidApp::on_destroy(ANativeActivity* activity) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    NX_INFO("Android: onDestroy");
    app->transition_state(AndroidLifecycleState::Destroyed);
}

void AndroidApp::on_window_created(ANativeActivity* activity,
                                    ANativeWindow* window) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    app->window_ = window;
    app->has_window_ = true;

    app->display_info_.width = ANativeWindow_getWidth(window);
    app->display_info_.height = ANativeWindow_getHeight(window);

    NX_INFO("Android: window created ({}x{})",
            app->display_info_.width, app->display_info_.height);

    if (app->resize_cb_) {
        app->resize_cb_(app->display_info_.width, app->display_info_.height);
    }
}

void AndroidApp::on_window_destroyed(ANativeActivity* activity,
                                      ANativeWindow* /*window*/) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    app->window_ = nullptr;
    app->has_window_ = false;
    NX_INFO("Android: window destroyed");
}

void AndroidApp::on_window_resized(ANativeActivity* activity,
                                    ANativeWindow* window) {
    auto* app = static_cast<AndroidApp*>(activity->instance);
    app->display_info_.width = ANativeWindow_getWidth(window);
    app->display_info_.height = ANativeWindow_getHeight(window);

    if (app->resize_cb_) {
        app->resize_cb_(app->display_info_.width, app->display_info_.height);
    }
}

i32 AndroidApp::on_input_event(ANativeActivity* /*activity*/,
                                AInputEvent* event) {
    i32 type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        // Forward to TouchInput system
        i32 action = AMotionEvent_getAction(event);
        (void)action; // Will be forwarded to TouchInput in full integration
        return 1; // Event handled
    }
    return 0; // Not handled
}
#endif

} // namespace nexus::platform
