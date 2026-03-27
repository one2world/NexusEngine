#include "nexus/animation/blend_tree.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// ClipNode
// ─────────────────────────────────────────────────────────────────────────────

void ClipNode::evaluate(float dt, const Skeleton& skeleton,
                         std::vector<BonePose>& out_pose) {
    (void)skeleton;
    if (!clip_) return;

    time_ += dt * speed_;
    if (looping_ && clip_->duration() > 0.0f) {
        time_ = std::fmod(time_, clip_->duration());
        if (time_ < 0.0f) time_ += clip_->duration();
    } else {
        time_ = math::clamp(time_, 0.0f, clip_->duration());
    }

    clip_->sample(time_, out_pose);
}

// ─────────────────────────────────────────────────────────────────────────────
// Blend1DNode
// ─────────────────────────────────────────────────────────────────────────────

void Blend1DNode::add_child(float threshold, BlendNodePtr node) {
    entries_.push_back({threshold, std::move(node)});
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.threshold < b.threshold; });
}

void Blend1DNode::evaluate(float dt, const Skeleton& skeleton,
                            std::vector<BonePose>& out_pose) {
    if (entries_.empty()) return;
    if (entries_.size() == 1) {
        entries_[0].node->evaluate(dt, skeleton, out_pose);
        return;
    }

    // Find the two entries to blend between
    float p = parameter_;

    // Clamp to range
    if (p <= entries_.front().threshold) {
        entries_.front().node->evaluate(dt, skeleton, out_pose);
        // Still tick other nodes to keep them in sync
        std::vector<BonePose> scratch;
        for (size_t i = 1; i < entries_.size(); ++i) {
            entries_[i].node->evaluate(dt, skeleton, scratch);
        }
        return;
    }
    if (p >= entries_.back().threshold) {
        entries_.back().node->evaluate(dt, skeleton, out_pose);
        std::vector<BonePose> scratch;
        for (size_t i = 0; i + 1 < entries_.size(); ++i) {
            entries_[i].node->evaluate(dt, skeleton, scratch);
        }
        return;
    }

    // Find the interval
    size_t lower = 0;
    for (size_t i = 0; i + 1 < entries_.size(); ++i) {
        if (p >= entries_[i].threshold && p <= entries_[i + 1].threshold) {
            lower = i;
            break;
        }
    }
    size_t upper = lower + 1;

    float range = entries_[upper].threshold - entries_[lower].threshold;
    float t = (range > 0.0f) ? (p - entries_[lower].threshold) / range : 0.0f;

    // Evaluate both
    std::vector<BonePose> pose_a, pose_b;
    entries_[lower].node->evaluate(dt, skeleton, pose_a);
    entries_[upper].node->evaluate(dt, skeleton, pose_b);

    // Tick remaining nodes
    std::vector<BonePose> scratch;
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (i != lower && i != upper) {
            entries_[i].node->evaluate(dt, skeleton, scratch);
        }
    }

    AnimationClip::blend(pose_a, pose_b, t, out_pose);
}

float Blend1DNode::duration() const {
    if (entries_.empty()) return 0.0f;
    float total = 0.0f;
    for (auto& e : entries_) total += e.node->duration();
    return total / static_cast<float>(entries_.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Blend2DNode
// ─────────────────────────────────────────────────────────────────────────────

void Blend2DNode::add_child(Vec2 position, BlendNodePtr node) {
    entries_.push_back({position, std::move(node)});
}

std::vector<float> Blend2DNode::compute_weights() const {
    std::vector<float> weights(entries_.size(), 0.0f);

    // Inverse distance weighting
    float total = 0.0f;
    bool exact_match = false;

    for (size_t i = 0; i < entries_.size(); ++i) {
        float dist = glm::length(entries_[i].position - parameter_);
        if (dist < 0.0001f) {
            weights.assign(entries_.size(), 0.0f);
            weights[i] = 1.0f;
            exact_match = true;
            break;
        }
        // Use squared inverse distance for sharper falloff
        float w = 1.0f / (dist * dist);
        weights[i] = w;
        total += w;
    }

    if (!exact_match && total > 0.0f) {
        for (auto& w : weights) w /= total;
    }

    return weights;
}

void Blend2DNode::evaluate(float dt, const Skeleton& skeleton,
                            std::vector<BonePose>& out_pose) {
    if (entries_.empty()) return;
    if (entries_.size() == 1) {
        entries_[0].node->evaluate(dt, skeleton, out_pose);
        return;
    }

    auto weights = compute_weights();

    // Evaluate all children
    std::vector<std::vector<BonePose>> poses(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) {
        entries_[i].node->evaluate(dt, skeleton, poses[i]);
    }

    // Multi-way weighted blend: start from first non-zero weight
    bool started = false;
    float accumulated_weight = 0.0f;

    for (size_t i = 0; i < entries_.size(); ++i) {
        if (weights[i] < 0.0001f) continue;

        if (!started) {
            out_pose = poses[i];
            accumulated_weight = weights[i];
            started = true;
        } else {
            float blend_factor = weights[i] / (accumulated_weight + weights[i]);
            AnimationClip::blend(out_pose, poses[i], blend_factor, out_pose);
            accumulated_weight += weights[i];
        }
    }
}

float Blend2DNode::duration() const {
    if (entries_.empty()) return 0.0f;
    float total = 0.0f;
    for (auto& e : entries_) total += e.node->duration();
    return total / static_cast<float>(entries_.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// AdditiveNode
// ─────────────────────────────────────────────────────────────────────────────

void AdditiveNode::evaluate(float dt, const Skeleton& skeleton,
                             std::vector<BonePose>& out_pose) {
    if (!base_) return;

    base_->evaluate(dt, skeleton, out_pose);

    if (!additive_ || weight_ <= 0.0f) return;

    std::vector<BonePose> additive_pose;
    additive_->evaluate(dt, skeleton, additive_pose);

    // Build reference pose from skeleton bind pose if not cached
    if (reference_pose_.size() != skeleton.bone_count()) {
        reference_pose_ = skeleton.get_bind_pose();
    }

    AnimationClip::blend_additive(out_pose, additive_pose, reference_pose_,
                                   weight_, out_pose);
}

// ─────────────────────────────────────────────────────────────────────────────
// BlendTree
// ─────────────────────────────────────────────────────────────────────────────

void BlendTree::evaluate(float dt, const Skeleton& skeleton,
                          std::vector<BonePose>& out_pose) {
    if (root_) {
        root_->evaluate(dt, skeleton, out_pose);
    }
}

void BlendTree::set_float(const std::string& name, float value) {
    parameters_[name] = value;
}

float BlendTree::get_float(const std::string& name) const {
    auto it = parameters_.find(name);
    return (it != parameters_.end()) ? it->second : 0.0f;
}

} // namespace nexus::anim
