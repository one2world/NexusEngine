#include "nexus/core/application.h"

namespace nexus {

int Application::run() {
    AppConfig config = configure();

    Timer timer;
    FixedTimestep fixed_step(config.fixed_timestep);

    on_init();

    while (running_) {
        timer.tick();
        dt_ = timer.delta_time();
        elapsed_ = timer.elapsed();

        // Cap frame time to avoid spiral of death
        float frame_dt = dt_;
        if (frame_dt > config.max_frame_time) {
            frame_dt = config.max_frame_time;
        }

        // Fixed timestep accumulation
        fixed_step.accumulate(frame_dt);
        while (fixed_step.should_step()) {
            on_fixed_update(fixed_step.step());
            fixed_step.consume();
        }
        fixed_alpha_ = fixed_step.alpha();

        // Variable timestep update
        on_update(frame_dt);

        // Render
        on_render();

        // UI overlay
        on_ui();

        ++frame_count_;
    }

    on_shutdown();
    return 0;
}

} // namespace nexus
