#include "nexus/renderer/lightmap_baker.h"
#include "nexus/core/log.h"
#include <cmath>
#include <algorithm>

namespace nexus {

// ── RNG helpers ───────────────────────────────────────────────────────────────

static u32 xorshift32(u32 state) {
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

static float rng_float(u32& state) {
    state = xorshift32(state);
    return static_cast<float>(state) / static_cast<float>(0xFFFFFFFFu);
}

// ── LightmapResult ────────────────────────────────────────────────────────────

void LightmapResult::tonemap(float exposure) {
    u32 texel_count = width * height;
    pixels_ldr.resize(static_cast<size_t>(texel_count) * 4u);

    for (u32 i = 0; i < texel_count; ++i) {
        Vec3 hdr = pixels[i] * exposure;
        // Reinhard tone mapping per channel
        Vec3 mapped = hdr / (Vec3(1.0f) + hdr);

        u32 base = i * 4u;
        pixels_ldr[base + 0u] = static_cast<u8>(std::min(mapped.x * 255.0f, 255.0f));
        pixels_ldr[base + 1u] = static_cast<u8>(std::min(mapped.y * 255.0f, 255.0f));
        pixels_ldr[base + 2u] = static_cast<u8>(std::min(mapped.z * 255.0f, 255.0f));
        pixels_ldr[base + 3u] = 255u;
    }
}

// ── LightmapBaker public ─────────────────────────────────────────────────────

void LightmapBaker::add_mesh(const BakeMesh& mesh) {
    meshes_.push_back(mesh);
}

void LightmapBaker::add_light(const BakeLight& light) {
    lights_.push_back(light);
}

void LightmapBaker::clear() {
    meshes_.clear();
    lights_.clear();
}

LightmapResult LightmapBaker::bake(u32 mesh_index) {
    if (mesh_index >= static_cast<u32>(meshes_.size())) {
        NX_ERROR("LightmapBaker::bake — mesh_index {} out of range ({})",
                 mesh_index, meshes_.size());
        return {};
    }

    const BakeMesh& mesh = meshes_[mesh_index];
    u32 res = config_.resolution;

    NX_INFO("LightmapBaker: baking mesh {} ({}x{}, {} triangles, {} spp, {} bounces)",
            mesh_index, res, res,
            static_cast<u32>(mesh.triangles.size()),
            config_.samples_per_texel, config_.bounces);

    // Build texel grid
    u32 texel_count = res * res;
    std::vector<LightmapTexel> texels(texel_count);

    // Rasterize all triangles of this mesh into the texel grid
    for (u32 t = 0; t < static_cast<u32>(mesh.triangles.size()); ++t) {
        rasterize_triangle(mesh.triangles[t], mesh.transform, texels, res);
    }

    // Count valid texels for progress reporting
    u32 valid_count = 0;
    for (u32 i = 0; i < texel_count; ++i) {
        if (texels[i].valid) {
            ++valid_count;
        }
    }

    NX_INFO("LightmapBaker: {} valid texels out of {}", valid_count, texel_count);

    // Prepare result
    LightmapResult result;
    result.width = res;
    result.height = res;
    result.pixels.resize(texel_count, Vec3(0.0f));

    // Bake each valid texel
    u32 progress = 0;
    for (u32 i = 0; i < texel_count; ++i) {
        const LightmapTexel& texel = texels[i];
        if (!texel.valid) {
            continue;
        }

        Vec3 position = texel.position;
        Vec3 normal = glm::normalize(texel.normal);

        // Direct lighting
        Vec3 color = compute_direct(position, normal);

        // Indirect lighting via hemisphere sampling
        if (config_.samples_per_texel > 0 && config_.bounces > 0) {
            Vec3 indirect(0.0f);
            // Seed RNG per texel for deterministic results
            u32 rng_state = i * 2654435761u + 1u;

            for (u32 s = 0; s < config_.samples_per_texel; ++s) {
                float u1 = rng_float(rng_state);
                float u2 = rng_float(rng_state);

                Vec3 sample_dir = cosine_weighted_hemisphere(normal, u1, u2);
                Vec3 ray_origin = position + normal * config_.bias;

                if (!trace_ray(ray_origin, sample_dir, 1000.0f)) {
                    // No occlusion — this sample sees the sky / environment.
                    // For simplicity, treat unoccluded hemisphere samples as
                    // a small ambient contribution modulated by the direct lights.
                    // A full path tracer would recurse here.
                    float ndot = std::max(glm::dot(normal, sample_dir), 0.0f);
                    indirect += Vec3(0.1f) * ndot;
                }
            }
            indirect /= static_cast<float>(config_.samples_per_texel);
            color += indirect;
        }

        result.pixels[i] = color * config_.intensity;

        ++progress;
        if (progress_cb_) {
            progress_cb_(progress, valid_count);
        }
    }

    NX_INFO("LightmapBaker: bake complete for mesh {}", mesh_index);
    return result;
}

std::vector<LightmapResult> LightmapBaker::bake_all() {
    std::vector<LightmapResult> results;
    results.reserve(meshes_.size());
    for (u32 i = 0; i < static_cast<u32>(meshes_.size()); ++i) {
        results.push_back(bake(i));
    }
    return results;
}

// ── LightmapBaker private ────────────────────────────────────────────────────

void LightmapBaker::rasterize_triangle(const BakeTriangle& tri, const Mat4& transform,
                                        std::vector<LightmapTexel>& texels, u32 resolution) {
    // Transform positions and normals to world space
    Vec3 p0 = Vec3(transform * Vec4(tri.v0, 1.0f));
    Vec3 p1 = Vec3(transform * Vec4(tri.v1, 1.0f));
    Vec3 p2 = Vec3(transform * Vec4(tri.v2, 1.0f));

    Mat4 normal_mat = glm::transpose(glm::inverse(transform));
    Vec3 wn0 = glm::normalize(Vec3(normal_mat * Vec4(tri.n0, 0.0f)));
    Vec3 wn1 = glm::normalize(Vec3(normal_mat * Vec4(tri.n1, 0.0f)));
    Vec3 wn2 = glm::normalize(Vec3(normal_mat * Vec4(tri.n2, 0.0f)));

    // UV2 coordinates scaled to texel space
    float fres = static_cast<float>(resolution);
    Vec2 t0 = tri.uv0 * fres;
    Vec2 t1 = tri.uv1 * fres;
    Vec2 t2 = tri.uv2 * fres;

    // Bounding box in texel space
    float min_x = std::max(std::min({t0.x, t1.x, t2.x}), 0.0f);
    float min_y = std::max(std::min({t0.y, t1.y, t2.y}), 0.0f);
    float max_x = std::min(std::max({t0.x, t1.x, t2.x}), fres - 1.0f);
    float max_y = std::min(std::max({t0.y, t1.y, t2.y}), fres - 1.0f);

    u32 ix_min = static_cast<u32>(min_x);
    u32 iy_min = static_cast<u32>(min_y);
    u32 ix_max = static_cast<u32>(max_x);
    u32 iy_max = static_cast<u32>(max_y);

    // Edge function for barycentric coordinates
    auto edge = [](Vec2 a, Vec2 b, Vec2 p) -> float {
        return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
    };

    float area = edge(t0, t1, t2);
    if (std::abs(area) < 1e-8f) {
        return; // Degenerate triangle in UV space
    }

    float inv_area = 1.0f / area;

    for (u32 y = iy_min; y <= iy_max; ++y) {
        for (u32 x = ix_min; x <= ix_max; ++x) {
            Vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);

            float w0 = edge(t1, t2, p) * inv_area;
            float w1 = edge(t2, t0, p) * inv_area;
            float w2 = edge(t0, t1, p) * inv_area;

            // Check if point is inside the triangle
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                u32 idx = y * resolution + x;
                if (idx < resolution * resolution) {
                    texels[idx].position = p0 * w0 + p1 * w1 + p2 * w2;
                    texels[idx].normal = glm::normalize(wn0 * w0 + wn1 * w1 + wn2 * w2);
                    texels[idx].valid = true;
                }
            }
        }
    }
}

Vec3 LightmapBaker::cosine_weighted_hemisphere(Vec3 normal, float u1, float u2) {
    float r = std::sqrt(u1);
    float theta = math::TWO_PI * u2;

    float x = r * std::cos(theta);
    float y = r * std::sin(theta);
    float z = std::sqrt(std::max(0.0f, 1.0f - u1));

    // Build orthonormal basis (TBN) from normal
    Vec3 up = (std::abs(normal.y) < 0.999f) ? Vec3(0.0f, 1.0f, 0.0f)
                                             : Vec3(1.0f, 0.0f, 0.0f);
    Vec3 tangent = glm::normalize(glm::cross(up, normal));
    Vec3 bitangent = glm::cross(normal, tangent);

    return glm::normalize(tangent * x + bitangent * y + normal * z);
}

bool LightmapBaker::trace_ray(Vec3 origin, Vec3 direction, float max_dist) const {
    // Moller-Trumbore ray-triangle intersection
    for (u32 m = 0; m < static_cast<u32>(meshes_.size()); ++m) {
        const BakeMesh& mesh = meshes_[m];
        const Mat4& xform = mesh.transform;

        for (u32 t = 0; t < static_cast<u32>(mesh.triangles.size()); ++t) {
            const BakeTriangle& tri = mesh.triangles[t];

            Vec3 v0 = Vec3(xform * Vec4(tri.v0, 1.0f));
            Vec3 v1 = Vec3(xform * Vec4(tri.v1, 1.0f));
            Vec3 v2 = Vec3(xform * Vec4(tri.v2, 1.0f));

            Vec3 e1 = v1 - v0;
            Vec3 e2 = v2 - v0;
            Vec3 h = glm::cross(direction, e2);
            float a = glm::dot(e1, h);

            if (std::abs(a) < math::EPSILON) {
                continue;
            }

            float f = 1.0f / a;
            Vec3 s = origin - v0;
            float u = f * glm::dot(s, h);

            if (u < 0.0f || u > 1.0f) {
                continue;
            }

            Vec3 q = glm::cross(s, e1);
            float v = f * glm::dot(direction, q);

            if (v < 0.0f || u + v > 1.0f) {
                continue;
            }

            float dist = f * glm::dot(e2, q);
            if (dist > math::EPSILON && dist < max_dist) {
                return true;
            }
        }
    }
    return false;
}

Vec3 LightmapBaker::compute_direct(Vec3 position, Vec3 normal) const {
    Vec3 color(0.0f);

    for (u32 i = 0; i < static_cast<u32>(lights_.size()); ++i) {
        const BakeLight& light = lights_[i];
        Vec3 light_dir = glm::normalize(-light.direction);
        float ndotl = std::max(glm::dot(normal, light_dir), 0.0f);

        if (ndotl <= 0.0f) {
            continue;
        }

        // Shadow ray
        Vec3 ray_origin = position + normal * config_.bias;
        if (!trace_ray(ray_origin, light_dir, 1000.0f)) {
            color += light.color * light.intensity * ndotl;
        }
    }

    return color;
}

} // namespace nexus
