#pragma once

#include "mesh_builder.h"

#include <vector>

namespace engine {

constexpr float kDummySpawnX = 0.0f;
constexpr float kDummySpawnY = 0.16f;
constexpr float kDummySpawnZ = 7.0f;
constexpr float kDummySpawnYaw = 3.1415926535f;

// Torso hit volume in dummy-local space (origin at feet).
constexpr float kDummyHitMinX = -0.28f;
constexpr float kDummyHitMaxX = 0.28f;
constexpr float kDummyHitMinY = 0.45f;
constexpr float kDummyHitMaxY = 1.25f;
constexpr float kDummyHitMinZ = -0.22f;
constexpr float kDummyHitMaxZ = 0.22f;

inline std::vector<float> build_dummy_post() {
  std::vector<float> vertices;
  vertices.reserve(2 * 36 * 6);

  constexpr float wood[] = {0.42f, 0.28f, 0.14f};
  append_box(vertices, 0.0f, 0.55f, 0.0f, 0.12f, 0.55f, 0.12f, wood[0], wood[1], wood[2]);
  append_box(vertices, 0.0f, 1.15f, 0.0f, 0.35f, 0.08f, 0.12f, wood[0], wood[1], wood[2]);

  return vertices;
}

inline std::vector<float> build_dummy_body() {
  std::vector<float> vertices;
  vertices.reserve(5 * 36 * 6);

  constexpr float straw[] = {0.82f, 0.72f, 0.38f};
  constexpr float straw_dark[] = {0.68f, 0.58f, 0.30f};
  constexpr float target[] = {0.85f, 0.15f, 0.12f};

  append_box(vertices, -0.14f, 0.28f, 0.0f, 0.08f, 0.28f, 0.08f, straw_dark[0], straw_dark[1], straw_dark[2]);
  append_box(vertices, 0.14f, 0.28f, 0.0f, 0.08f, 0.28f, 0.08f, straw_dark[0], straw_dark[1], straw_dark[2]);
  append_box(vertices, 0.0f, 0.82f, 0.0f, 0.24f, 0.32f, 0.14f, straw[0], straw[1], straw[2]);
  append_box(vertices, -0.30f, 0.80f, 0.0f, 0.07f, 0.26f, 0.07f, straw[0], straw[1], straw[2]);
  append_box(vertices, 0.30f, 0.80f, 0.0f, 0.07f, 0.26f, 0.07f, straw[0], straw[1], straw[2]);
  append_box(vertices, 0.0f, 1.22f, 0.0f, 0.18f, 0.18f, 0.18f, straw[0], straw[1], straw[2]);
  append_box(vertices, 0.0f, 0.88f, 0.15f, 0.12f, 0.12f, 0.02f, target[0], target[1], target[2]);

  return vertices;
}

} // namespace engine
