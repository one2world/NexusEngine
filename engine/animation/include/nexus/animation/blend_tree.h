#pragma once

#include "nexus/core/types.h"
#include "nexus/animation/animation_clip.h"
#include "nexus/animation/skeleton.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace nexus::anim {

// ─────────────────────────────────────────────────────────────────────────────
// BlendNode — base class for nodes in a blend tree
// ─────────────────────────────────────────────────────────────────────────────

class BlendNode {
public:
    virtual ~BlendNode() = default;

    /// Evaluate this node, producing an output pose.
    virtual void evaluate(float dt, const Skeleton& skeleton,
                          std::vector<BonePose>& out_pose) = 0;

    /// Get the effective duration of this node.
    virtual float duration() const = 0;
};

using BlendNodePtr = std::unique_ptr<BlendNode>;

// ─────────────────────────────────────────────────────────────────────────────
// ClipNode — leaf node that plays a single animation clip
// ─────────────────────────────────────────────────────────────────────────────

class ClipNode : public BlendNode {
public:
    explicit ClipNode(AnimationClip* clip, float speed = 1.0f, bool looping = true)
        : clip_(clip), speed_(speed), looping_(looping) {}

    void evaluate(float dt, const Skeleton& skeleton,
                  std::vector<BonePose>& out_pose) override;

    float duration() const override {
        return clip_ ? clip_->duration() : 0.0f;
    }

    float time() const { return time_; }
    void set_time(float t) { time_ = t; }
    AnimationClip* clip() const { return clip_; }

private:
    AnimationClip* clip_;
    float speed_;
    bool looping_;
    float time_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// Blend1DNode — blends between children based on a 1D parameter
// Like Unity's 1D blend tree: walk ↔ run based on speed parameter
// ─────────────────────────────────────────────────────────────────────────────

class Blend1DNode : public BlendNode {
public:
    struct Entry {
        float threshold;   // parameter value at which this child is fully active
        BlendNodePtr node;
    };

    void add_child(float threshold, BlendNodePtr node);
    void set_parameter(float value) { parameter_ = value; }
    float parameter() const { return parameter_; }

    void evaluate(float dt, const Skeleton& skeleton,
                  std::vector<BonePose>& out_pose) override;

    float duration() const override;

    const std::vector<Entry>& entries() const { return entries_; }

private:
    std::vector<Entry> entries_;
    float parameter_{0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// Blend2DNode — blends between children based on 2D parameter (x, y)
// Like Unity's Freeform 2D blend tree: directional movement blending
// ─────────────────────────────────────────────────────────────────────────────

class Blend2DNode : public BlendNode {
public:
    struct Entry {
        Vec2 position;     // 2D parameter position for this child
        BlendNodePtr node;
    };

    void add_child(Vec2 position, BlendNodePtr node);
    void set_parameter(Vec2 value) { parameter_ = value; }
    Vec2 parameter() const { return parameter_; }

    void evaluate(float dt, const Skeleton& skeleton,
                  std::vector<BonePose>& out_pose) override;

    float duration() const override;

    const std::vector<Entry>& entries() const { return entries_; }

private:
    /// Compute blend weights using inverse distance weighting.
    std::vector<float> compute_weights() const;

    std::vector<Entry> entries_;
    Vec2 parameter_{0.0f, 0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// AdditiveNode — applies one animation additively on top of a base
// ─────────────────────────────────────────────────────────────────────────────

class AdditiveNode : public BlendNode {
public:
    AdditiveNode(BlendNodePtr base, BlendNodePtr additive, float weight = 1.0f)
        : base_(std::move(base)), additive_(std::move(additive)), weight_(weight) {}

    void set_weight(float w) { weight_ = w; }
    float weight() const { return weight_; }

    void evaluate(float dt, const Skeleton& skeleton,
                  std::vector<BonePose>& out_pose) override;

    float duration() const override {
        return base_ ? base_->duration() : 0.0f;
    }

private:
    BlendNodePtr base_;
    BlendNodePtr additive_;
    float weight_;
    std::vector<BonePose> reference_pose_;
};

// ─────────────────────────────────────────────────────────────────────────────
// BlendTree — top-level blend tree with named parameter access
// ─────────────────────────────────────────────────────────────────────────────

class BlendTree {
public:
    void set_root(BlendNodePtr root) { root_ = std::move(root); }
    BlendNode* root() const { return root_.get(); }

    /// Evaluate the tree, producing an output pose.
    void evaluate(float dt, const Skeleton& skeleton,
                  std::vector<BonePose>& out_pose);

    /// Named parameter management (convenience for connecting to game logic).
    void set_float(const std::string& name, float value);
    float get_float(const std::string& name) const;

private:
    BlendNodePtr root_;
    std::unordered_map<std::string, float> parameters_;
};

} // namespace nexus::anim
