#pragma once

#include "mesh_builder.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace engine {

struct GroundSurface {
  float min_x;
  float max_x;
  float min_z;
  float max_z;
  float height;
};

struct SolidBox {
  float min_x;
  float max_x;
  float min_y;
  float max_y;
  float min_z;
  float max_z;
};

// ~1 ft step at 6 ft character scale.
constexpr float kMaxStepHeight = 0.26f;
constexpr float kSupportEpsilon = 0.06f;
constexpr float kPlayerRadius = 0.18f;
constexpr float kPlayerHeight = 1.50f;

inline void append_rock_cluster(std::vector<float> &out,
                                float cx, float cz,
                                float base_r, float base_g, float base_b) {
  const float base_top = 0.40f;
  append_box(out, cx, 0.20f, cz, 0.50f, 0.20f, 0.40f, base_r, base_g, base_b);
  append_box(out, cx + 0.18f, base_top + 0.14f, cz + 0.10f, 0.28f, 0.14f, 0.22f,
             base_r * 0.92f, base_g * 0.92f, base_b * 0.92f);
  append_box(out, cx - 0.15f, base_top + 0.12f, cz - 0.08f, 0.22f, 0.12f, 0.18f,
             base_r * 1.05f, base_g * 1.05f, base_b * 1.05f);
}

inline void append_uv_sphere(std::vector<float> &out,
                             float cx, float cy, float cz, float radius,
                             int stacks, int sectors,
                             float r, float g, float b) {
  constexpr float kPi = 3.1415926535f;
  for (int i = 0; i < stacks; ++i) {
    const float phi0 = kPi * static_cast<float>(i) / static_cast<float>(stacks);
    const float phi1 = kPi * static_cast<float>(i + 1) / static_cast<float>(stacks);
    for (int j = 0; j < sectors; ++j) {
      const float theta0 = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
      const float theta1 = 2.0f * kPi * static_cast<float>(j + 1) / static_cast<float>(sectors);

      const float x00 = cx + radius * std::sin(phi0) * std::cos(theta0);
      const float y00 = cy + radius * std::cos(phi0);
      const float z00 = cz + radius * std::sin(phi0) * std::sin(theta0);

      const float x10 = cx + radius * std::sin(phi0) * std::cos(theta1);
      const float y10 = cy + radius * std::cos(phi0);
      const float z10 = cz + radius * std::sin(phi0) * std::sin(theta1);

      const float x11 = cx + radius * std::sin(phi1) * std::cos(theta1);
      const float y11 = cy + radius * std::cos(phi1);
      const float z11 = cz + radius * std::sin(phi1) * std::sin(theta1);

      const float x01 = cx + radius * std::sin(phi1) * std::cos(theta0);
      const float y01 = cy + radius * std::cos(phi1);
      const float z01 = cz + radius * std::sin(phi1) * std::sin(theta0);

      append_quad_vnorm(out,
                        x00, y00, z00, (x00 - cx) / radius, (y00 - cy) / radius, (z00 - cz) / radius,
                        x10, y10, z10, (x10 - cx) / radius, (y10 - cy) / radius, (z10 - cz) / radius,
                        x11, y11, z11, (x11 - cx) / radius, (y11 - cy) / radius, (z11 - cz) / radius,
                        x01, y01, z01, (x01 - cx) / radius, (y01 - cy) / radius, (z01 - cz) / radius,
                        r, g, b);
    }
  }
}

inline void append_rock_cluster_collision(std::vector<GroundSurface> &walk,
                                          std::vector<SolidBox> &solids,
                                          float cx, float cz) {
  const float base_top = 0.40f;
  const float cluster_top = 0.68f;
  walk.push_back({cx - 0.50f, cx + 0.50f, cz - 0.40f, cz + 0.40f, cluster_top});
  solids.push_back({cx - 0.50f, cx + 0.50f, 0.0f, base_top, cz - 0.40f, cz + 0.40f});
  solids.push_back({cx + 0.18f - 0.28f, cx + 0.18f + 0.28f, base_top, cluster_top,
                    cz + 0.10f - 0.22f, cz + 0.10f + 0.22f});
  solids.push_back({cx - 0.15f - 0.22f, cx - 0.15f + 0.22f, base_top, base_top + 0.24f,
                    cz - 0.08f - 0.18f, cz - 0.08f + 0.18f});
}

inline const std::vector<GroundSurface> &ground_surfaces() {
  static const std::vector<GroundSurface> surfaces = [] {
    std::vector<GroundSurface> out = {
        {-18.0f, 18.0f, -18.0f, 18.0f, 0.0f},
        {-4.0f, 4.0f, -4.0f, 4.0f, 0.16f},
        {-7.8f, -4.2f, 3.6f, 6.4f, 0.70f},
        {4.9f, 8.1f, -5.6f, -2.4f, 1.10f},
        {-6.2f, -3.8f, -7.2f, -4.8f, 1.70f},
        {4.0f, 6.0f, 5.0f, 7.0f, 1.50f},
        {-2.5f, 2.5f, -9.0f, -7.0f, 0.90f},
        {-9.0f, -7.0f, -2.0f, 2.0f, 0.90f},
    };

    const float rock_positions[][2] = {
        {-14.0f, -14.0f}, {14.0f, -14.0f}, {-14.0f, 14.0f}, {14.0f, 14.0f},
        {0.0f, -15.0f}, {0.0f, 15.0f}, {-15.0f, 0.0f}, {15.0f, 0.0f},
        {-3.0f, 3.0f}, {4.0f, 2.0f}, {-2.0f, -5.0f}, {7.0f, 1.0f},
        {-7.0f, -2.0f}, {2.0f, -3.0f},
    };

    std::vector<SolidBox> unused;
    for (const auto &rock : rock_positions) {
      append_rock_cluster_collision(out, unused, rock[0], rock[1]);
    }
    return out;
  }();
  return surfaces;
}

inline const std::vector<SolidBox> &solid_boxes() {
  static const std::vector<SolidBox> boxes = [] {
    std::vector<SolidBox> out = {
        {-4.0f, 4.0f, 0.0f, 0.16f, -4.0f, 4.0f},
        {-7.8f, -4.2f, 0.0f, 0.70f, 3.6f, 6.4f},
        {4.9f, 8.1f, 0.0f, 1.10f, -5.6f, -2.4f},
        {-6.2f, -3.8f, 0.0f, 1.70f, -7.2f, -4.8f},
        {4.0f, 6.0f, 0.0f, 1.50f, 5.0f, 7.0f},
        {-2.5f, 2.5f, 0.0f, 0.90f, -9.0f, -7.0f},
        {-9.0f, -7.0f, 0.0f, 0.90f, -2.0f, 2.0f},
    };

    const float rock_positions[][2] = {
        {-14.0f, -14.0f}, {14.0f, -14.0f}, {-14.0f, 14.0f}, {14.0f, 14.0f},
        {0.0f, -15.0f}, {0.0f, 15.0f}, {-15.0f, 0.0f}, {15.0f, 0.0f},
        {-3.0f, 3.0f}, {4.0f, 2.0f}, {-2.0f, -5.0f}, {7.0f, 1.0f},
        {-7.0f, -2.0f}, {2.0f, -3.0f},
    };

    std::vector<GroundSurface> unused;
    for (const auto &rock : rock_positions) {
      append_rock_cluster_collision(unused, out, rock[0], rock[1]);
    }
    return out;
  }();
  return boxes;
}

inline std::vector<float> build_arena() {
  std::vector<float> vertices;
  vertices.reserve(200 * 36 * 6);

  constexpr float sand[] = {0.78f, 0.68f, 0.46f};
  constexpr float rock[] = {0.48f, 0.44f, 0.40f};
  constexpr float rock_dark[] = {0.38f, 0.35f, 0.32f};
  constexpr float stone[] = {0.58f, 0.55f, 0.50f};

  append_box(vertices, 0.0f, -0.05f, 0.0f, 18.0f, 0.05f, 18.0f, sand[0], sand[1], sand[2]);

  append_box(vertices, 0.0f, 0.08f, 0.0f, 4.0f, 0.08f, 4.0f, stone[0], stone[1], stone[2]);
  append_box(vertices, -6.0f, 0.35f, 5.0f, 1.8f, 0.35f, 1.4f, stone[0], stone[1], stone[2]);
  append_box(vertices, 6.5f, 0.55f, -4.0f, 1.6f, 0.55f, 1.6f, stone[0], stone[1], stone[2]);
  append_box(vertices, -5.0f, 0.85f, -6.0f, 1.2f, 0.85f, 1.2f, stone[0], stone[1], stone[2]);
  append_box(vertices, 5.0f, 1.15f, 6.0f, 1.0f, 0.35f, 1.0f, stone[0], stone[1], stone[2]);
  append_box(vertices, 0.0f, 0.65f, -8.0f, 2.5f, 0.25f, 1.0f, stone[0], stone[1], stone[2]);
  append_box(vertices, -8.0f, 0.45f, 0.0f, 1.0f, 0.45f, 2.0f, stone[0], stone[1], stone[2]);

  append_rock_cluster(vertices, -14.0f, -14.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, 14.0f, -14.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, -14.0f, 14.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, 14.0f, 14.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, 0.0f, -15.0f, rock_dark[0], rock_dark[1], rock_dark[2]);
  append_rock_cluster(vertices, 0.0f, 15.0f, rock_dark[0], rock_dark[1], rock_dark[2]);
  append_rock_cluster(vertices, -15.0f, 0.0f, rock_dark[0], rock_dark[1], rock_dark[2]);
  append_rock_cluster(vertices, 15.0f, 0.0f, rock_dark[0], rock_dark[1], rock_dark[2]);
  append_rock_cluster(vertices, -3.0f, 3.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, 4.0f, 2.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, -2.0f, -5.0f, rock_dark[0], rock_dark[1], rock_dark[2]);
  append_rock_cluster(vertices, 7.0f, 1.0f, rock[0], rock[1], rock[2]);
  append_rock_cluster(vertices, -7.0f, -2.0f, rock_dark[0], rock_dark[1], rock_dark[2]);
  append_rock_cluster(vertices, 2.0f, -3.0f, rock[0], rock[1], rock[2]);

  return vertices;
}

inline bool aabb_overlaps(float ax0, float ax1, float ay0, float ay1, float az0, float az1,
                          float bx0, float bx1, float by0, float by1, float bz0, float bz1) {
  return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0 && az0 < bz1 && az1 > bz0;
}

inline bool is_position_blocked(float x, float y, float z) {
  const float px0 = x - kPlayerRadius;
  const float px1 = x + kPlayerRadius;
  const float py0 = y;
  const float py1 = y + kPlayerHeight;
  const float pz0 = z - kPlayerRadius;
  const float pz1 = z + kPlayerRadius;

  for (const SolidBox &box : solid_boxes()) {
    if (aabb_overlaps(px0, px1, py0, py1, pz0, pz1,
                      box.min_x, box.max_x, box.min_y, box.max_y, box.min_z, box.max_z)) {
      return true;
    }
  }
  return false;
}

inline bool player_overlaps_solid_xz(float x, float z, const SolidBox &box) {
  return (x - kPlayerRadius) < box.max_x && (x + kPlayerRadius) > box.min_x &&
         (z - kPlayerRadius) < box.max_z && (z + kPlayerRadius) > box.min_z;
}

inline float resolve_solid_top_support(float x, float z, float y, float vy) {
  if (vy > 0.05f) {
    return -1.0f;
  }

  float best = -1.0f;
  for (const SolidBox &box : solid_boxes()) {
    if (!player_overlaps_solid_xz(x, z, box)) {
      continue;
    }
    if (y <= box.max_y + kSupportEpsilon && y >= box.max_y - 0.75f) {
      best = std::max(best, box.max_y);
    }
  }
  return best;
}

inline float resolve_support_height(float x, float z, float y, float vy) {
  float floor_at = 0.0f;
  for (const GroundSurface &surface : ground_surfaces()) {
    if (x >= surface.min_x && x <= surface.max_x &&
        z >= surface.min_z && z <= surface.max_z &&
        surface.height <= y + kSupportEpsilon) {
      floor_at = std::max(floor_at, surface.height);
    }
  }

  float support = floor_at;
  for (const GroundSurface &surface : ground_surfaces()) {
    if (x >= surface.min_x && x <= surface.max_x &&
        z >= surface.min_z && z <= surface.max_z &&
        surface.height > floor_at &&
        surface.height <= floor_at + kMaxStepHeight &&
        y <= floor_at + kSupportEpsilon) {
      support = std::max(support, surface.height);
    }
  }

  const float solid_top = resolve_solid_top_support(x, z, y, vy);
  if (solid_top >= 0.0f) {
    support = std::max(support, solid_top);
  }

  return support;
}

inline void depenetrate(float &x, float &y, float &z) {
  constexpr float kSkin = 0.01f;

  for (int iteration = 0; iteration < 8; ++iteration) {
    if (!is_position_blocked(x, y, z)) {
      return;
    }

    float lift_to = y;
    for (const SolidBox &box : solid_boxes()) {
      if (player_overlaps_solid_xz(x, z, box)) {
        lift_to = std::max(lift_to, box.max_y);
      }
    }
    if (lift_to > y + 1e-4f) {
      y = lift_to + kSkin;
      continue;
    }

    const float px0 = x - kPlayerRadius;
    const float px1 = x + kPlayerRadius;
    const float py0 = y;
    const float py1 = y + kPlayerHeight;
    const float pz0 = z - kPlayerRadius;
    const float pz1 = z + kPlayerRadius;

    bool pushed = false;
    for (const SolidBox &box : solid_boxes()) {
      if (!aabb_overlaps(px0, px1, py0, py1, pz0, pz1,
                         box.min_x, box.max_x, box.min_y, box.max_y, box.min_z, box.max_z)) {
        continue;
      }

      const float overlap_x = std::min(px1, box.max_x) - std::max(px0, box.min_x);
      const float overlap_z = std::min(pz1, box.max_z) - std::max(pz0, box.min_z);
      if (overlap_x <= 0.0f || overlap_z <= 0.0f) {
        continue;
      }

      if (overlap_x < overlap_z) {
        const float box_center_x = (box.min_x + box.max_x) * 0.5f;
        x += (x < box_center_x ? -(overlap_x + kSkin) : (overlap_x + kSkin));
      } else {
        const float box_center_z = (box.min_z + box.max_z) * 0.5f;
        z += (z < box_center_z ? -(overlap_z + kSkin) : (overlap_z + kSkin));
      }
      pushed = true;
      break;
    }

    if (!pushed) {
      return;
    }
  }
}

} // namespace engine
