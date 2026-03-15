#include "nexus/animation/particle_system.h"
#include <cmath>
#include <algorithm>

namespace nexus::anim {

ParticleSystem::ParticleSystem(const EmitterConfig& config)
    : config_(config) {
    particles_.reserve(config.max_particles);
}

void ParticleSystem::update(float dt) {
    // Emit new particles
    if (emitting_ && config_.emit_rate > 0.0f) {
        emit_accumulator_ += config_.emit_rate * dt;
        while (emit_accumulator_ >= 1.0f &&
               particles_.size() < config_.max_particles) {
            emit_particle();
            emit_accumulator_ -= 1.0f;
        }
    }

    // Update existing particles
    for (auto& p : particles_) {
        if (!p.alive) continue;

        p.age += dt;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }

        // Apply affectors
        for (const auto& affector : affectors_) {
            affector(p, dt);
        }

        // Integrate
        p.position += p.velocity * dt;
        p.rotation += p.angular_velocity * dt;

        // Interpolate color and size over lifetime
        float life_t = p.age / p.lifetime;
        p.color = p.start_color + (p.end_color - p.start_color) * life_t;
        p.size = math::lerp(p.start_size, p.end_size, life_t);
    }

    // Remove dead particles (swap-and-pop)
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
            [](const Particle& p) { return !p.alive; }),
        particles_.end());
}

void ParticleSystem::burst(u32 count) {
    for (u32 i = 0; i < count && particles_.size() < config_.max_particles; ++i) {
        emit_particle();
    }
}

void ParticleSystem::add_affector(Affector affector) {
    affectors_.push_back(std::move(affector));
}

void ParticleSystem::clear() {
    particles_.clear();
}

u32 ParticleSystem::alive_count() const {
    u32 count = 0;
    for (const auto& p : particles_) {
        if (p.alive) ++count;
    }
    return count;
}

void ParticleSystem::emit_particle() {
    Particle p;
    p.alive = true;
    p.age = 0.0f;
    p.lifetime = random_range(config_.lifetime_min, config_.lifetime_max);
    p.start_size = random_range(config_.size_min, config_.size_max);
    p.end_size = random_range(config_.end_size_min, config_.end_size_max);
    p.size = p.start_size;
    p.start_color = config_.color_start;
    p.end_color = config_.color_end;
    p.color = p.start_color;
    p.rotation = random_range(config_.rotation_min, config_.rotation_max);
    p.angular_velocity = random_range(config_.angular_velocity_min,
                                       config_.angular_velocity_max);

    // Position based on emitter shape
    switch (config_.shape) {
        case EmitterShape::Point:
            p.position = config_.position;
            break;
        case EmitterShape::Box:
            p.position = config_.position + Vec3(
                random_range(-config_.box_extents.x, config_.box_extents.x),
                random_range(-config_.box_extents.y, config_.box_extents.y),
                random_range(-config_.box_extents.z, config_.box_extents.z));
            break;
        case EmitterShape::Sphere: {
            Vec3 dir = random_direction();
            float r = random_range(0.0f, config_.sphere_radius);
            p.position = config_.position + dir * r;
            break;
        }
        case EmitterShape::Circle: {
            float angle = random_range(0.0f, math::TWO_PI);
            float r = random_range(0.0f, config_.sphere_radius);
            p.position = config_.position + Vec3(std::cos(angle) * r, 0.0f, std::sin(angle) * r);
            break;
        }
        case EmitterShape::Cone: {
            float half_angle = config_.cone_angle * math::DEG2RAD * 0.5f;
            float phi = random_range(0.0f, math::TWO_PI);
            float theta = random_range(0.0f, half_angle);
            Vec3 dir;
            dir.x = std::sin(theta) * std::cos(phi);
            dir.y = std::cos(theta);
            dir.z = std::sin(theta) * std::sin(phi);
            p.position = config_.position;
            float speed = random_range(config_.speed_min, config_.speed_max);
            p.velocity = dir * speed;
            particles_.push_back(p);
            return; // velocity already set
        }
    }

    // Velocity
    Vec3 dir = glm::length(config_.direction) > math::EPSILON
               ? glm::normalize(config_.direction) : random_direction();
    float speed = random_range(config_.speed_min, config_.speed_max);
    p.velocity = dir * speed;

    particles_.push_back(p);
}

Vec3 ParticleSystem::random_direction() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    Vec3 v;
    do {
        v = {dist(rng_), dist(rng_), dist(rng_)};
    } while (glm::dot(v, v) < math::EPSILON);
    return glm::normalize(v);
}

float ParticleSystem::random_range(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

} // namespace nexus::anim
