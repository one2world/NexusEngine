#include "nexus/core/log.h"
#include "nexus/core/timer.h"
#include "nexus/platform/input.h"
#include "nexus/platform/window.h"

int main() {
    // Initialize logging
    nexus::Log::init();
    NX_APP_INFO("Sandbox starting...");

    // Create window with default configuration
    nexus::WindowConfig config;
    nexus::Window window(config);
    NX_APP_INFO("Window created: {}x{}", config.width, config.height);

    // Initialize input system
    nexus::Input::init(window.native_handle());

    // Create timer
    nexus::Timer timer;

    // Main loop
    while (!window.should_close()) {
        window.poll_events();
        nexus::Input::update();
        timer.tick();

        // Close on ESC
        if (nexus::Input::key_pressed(nexus::Key::Escape)) {
            break;
        }

        // Log FPS every 60 frames
        if (timer.frame_count() % 60 == 0 && timer.frame_count() > 0) {
            NX_APP_INFO("FPS: {:.1f}", timer.fps());
        }

        window.swap_buffers();
    }

    // Cleanup
    NX_APP_INFO("Sandbox shutting down...");
    nexus::Log::shutdown();
    return 0;
}
