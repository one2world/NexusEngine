#include "nexus/animation/sprite_animation.h"

namespace nexus::anim {

void SpriteAnimator::add_animation(const std::string& name, SpriteAnimation anim) {
    anim.name = name;
    animations_[name] = std::move(anim);
}

void SpriteAnimator::play(const std::string& name, bool reset) {
    auto it = animations_.find(name);
    if (it == animations_.end()) return;

    if (current_name_ == name && !reset) return;

    current_name_ = name;
    frame_index_ = 0;
    frame_timer_ = 0.0f;
    playing_ = true;
    finished_ = false;
}

void SpriteAnimator::stop() {
    playing_ = false;
}

void SpriteAnimator::update(float dt) {
    if (!playing_ || finished_) return;

    auto it = animations_.find(current_name_);
    if (it == animations_.end() || it->second.frames.empty()) return;

    const auto& anim = it->second;
    frame_timer_ += dt * speed_;

    while (frame_timer_ >= anim.frames[frame_index_].duration) {
        frame_timer_ -= anim.frames[frame_index_].duration;
        ++frame_index_;

        if (frame_index_ >= static_cast<u32>(anim.frames.size())) {
            if (anim.looping) {
                frame_index_ = 0;
            } else {
                frame_index_ = static_cast<u32>(anim.frames.size()) - 1;
                finished_ = true;
                playing_ = false;
                return;
            }
        }
    }
}

Vec2 SpriteAnimator::current_uv_min() const {
    auto it = animations_.find(current_name_);
    if (it == animations_.end() || it->second.frames.empty())
        return {0.0f, 0.0f};
    return it->second.frames[frame_index_].uv_min;
}

Vec2 SpriteAnimator::current_uv_max() const {
    auto it = animations_.find(current_name_);
    if (it == animations_.end() || it->second.frames.empty())
        return {1.0f, 1.0f};
    return it->second.frames[frame_index_].uv_max;
}

SpriteAnimation SpriteAnimator::from_sheet(const std::string& name,
                                            u32 cols, u32 rows,
                                            u32 frame_count, float fps,
                                            bool looping) {
    SpriteAnimation anim;
    anim.name = name;
    anim.looping = looping;

    float fw = 1.0f / static_cast<float>(cols);
    float fh = 1.0f / static_cast<float>(rows);
    float duration = 1.0f / fps;

    for (u32 i = 0; i < frame_count; ++i) {
        u32 col = i % cols;
        u32 row = i / cols;
        SpriteFrame frame;
        frame.uv_min = {static_cast<float>(col) * fw, static_cast<float>(row) * fh};
        frame.uv_max = {static_cast<float>(col + 1) * fw, static_cast<float>(row + 1) * fh};
        frame.duration = duration;
        anim.frames.push_back(frame);
    }
    return anim;
}

} // namespace nexus::anim
