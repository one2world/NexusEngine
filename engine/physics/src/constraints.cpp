#include "nexus/physics/constraints.h"
#include "nexus/physics/physics_world_3d.h"
#include <cmath>

namespace nexus::physics {

// ── Helpers ─────────────────────────────────────────────────────────────────

static Vec3 world_anchor(const Body3D& b, Vec3 local) {
    return b.position + glm::mat3_cast(b.rotation) * local;
}

// ── DistanceJoint ───────────────────────────────────────────────────────────

void DistanceJoint::prepare(float dt) {
    dt_ = dt;
    if (!body_a || !body_b) return;

    Vec3 wa = world_anchor(*body_a, anchor_a);
    Vec3 wb = world_anchor(*body_b, anchor_b);
    Vec3 diff = wb - wa;
    float len = glm::length(diff);

    axis_ = (len > math::EPSILON) ? diff / len : Vec3(0.0f, 1.0f, 0.0f);
    float error = len - distance;

    float inv_mass = body_a->inv_mass + body_b->inv_mass;
    effective_mass_ = (inv_mass > 0.0f) ? 1.0f / inv_mass : 0.0f;

    float beta = stiffness * 0.2f;
    bias_ = -beta / dt * error;
    impulse_ = 0.0f;
}

void DistanceJoint::solve() {
    if (!body_a || !body_b) return;

    float rel_v = glm::dot(body_b->velocity - body_a->velocity, axis_);
    float lambda = effective_mass_ * (bias_ - rel_v - damping * rel_v);
    impulse_ += lambda;

    Vec3 p = axis_ * lambda;
    body_a->velocity -= p * body_a->inv_mass;
    body_b->velocity += p * body_b->inv_mass;
}

// ── HingeJoint ──────────────────────────────────────────────────────────────

void HingeJoint::prepare(float dt) {
    dt_ = dt;
    if (!body_a || !body_b) return;

    r_a_ = world_anchor(*body_a, anchor_a) - body_a->position;
    r_b_ = world_anchor(*body_b, anchor_b) - body_b->position;

    float inv_mass = body_a->inv_mass + body_b->inv_mass;
    effective_mass_ = (inv_mass > 0.0f) ? 1.0f / inv_mass : 0.0f;

    Vec3 wa = body_a->position + r_a_;
    Vec3 wb = body_b->position + r_b_;
    Vec3 error = wb - wa;
    float beta = 0.2f;
    bias_ = -beta / dt * glm::length(error);
    impulse_ = Vec3(0.0f);
}

void HingeJoint::solve() {
    if (!body_a || !body_b) return;

    // Point-to-point constraint along non-axis directions
    Vec3 wa = body_a->position + r_a_;
    Vec3 wb = body_b->position + r_b_;
    Vec3 diff = wb - wa;

    // Project out axis component to only constrain perpendicular
    Vec3 perp = diff - axis * glm::dot(diff, axis);
    float len = glm::length(perp);
    if (len < math::EPSILON) return;

    Vec3 n = perp / len;
    float rel_v = glm::dot(body_b->velocity - body_a->velocity, n);
    float lambda = effective_mass_ * (-rel_v + bias_);

    Vec3 p = n * lambda;
    body_a->velocity -= p * body_a->inv_mass;
    body_b->velocity += p * body_b->inv_mass;
}

// ── BallJoint ───────────────────────────────────────────────────────────────

void BallJoint::prepare(float dt) {
    dt_ = dt;
    if (!body_a || !body_b) return;

    r_a_ = world_anchor(*body_a, anchor_a) - body_a->position;
    r_b_ = world_anchor(*body_b, anchor_b) - body_b->position;

    float inv_mass = body_a->inv_mass + body_b->inv_mass;
    if (inv_mass > 0.0f) {
        effective_mass_ = Mat3(1.0f / inv_mass);
    } else {
        effective_mass_ = Mat3(0.0f);
    }

    Vec3 wa = body_a->position + r_a_;
    Vec3 wb = body_b->position + r_b_;
    float beta = 0.2f;
    bias_ = -(beta / dt) * (wb - wa);
    impulse_ = Vec3(0.0f);
}

void BallJoint::solve() {
    if (!body_a || !body_b) return;

    Vec3 rel_v = body_b->velocity - body_a->velocity;
    Vec3 lambda = effective_mass_ * (bias_ - rel_v);
    impulse_ += lambda;

    body_a->velocity -= lambda * body_a->inv_mass;
    body_b->velocity += lambda * body_b->inv_mass;
}

// ── SpringJoint ─────────────────────────────────────────────────────────────

void SpringJoint::prepare(float dt) {
    dt_ = dt;
}

void SpringJoint::solve() {
    if (!body_a || !body_b) return;

    Vec3 wa = world_anchor(*body_a, anchor_a);
    Vec3 wb = world_anchor(*body_b, anchor_b);
    Vec3 diff = wb - wa;
    float len = glm::length(diff);

    if (len < math::EPSILON) return;

    Vec3 n = diff / len;
    float stretch = len - rest_length;

    // Spring force: F = -k * x - c * v_rel
    float rel_v = glm::dot(body_b->velocity - body_a->velocity, n);
    float force_mag = stiffness * stretch + damping * rel_v;

    Vec3 impulse = n * force_mag * dt_;
    body_a->velocity += impulse * body_a->inv_mass;
    body_b->velocity -= impulse * body_b->inv_mass;
}

} // namespace nexus::physics
