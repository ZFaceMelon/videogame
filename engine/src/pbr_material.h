#pragma once

namespace engine {

struct PbrMaterial {
  float metallic = 0.0f;
  float roughness = 0.8f;
  float emissive_r = 0.0f;
  float emissive_g = 0.0f;
  float emissive_b = 0.0f;
  float emissive_strength = 0.0f;
  float diffuse_bounce = 0.3f;
};

inline PbrMaterial mat_sand() {
  return {0.0f, 0.96f, 0.0f, 0.0f, 0.0f, 0.0f, 0.48f};
}

inline PbrMaterial mat_stone() {
  return {0.04f, 0.82f, 0.0f, 0.0f, 0.0f, 0.0f, 0.22f};
}

inline PbrMaterial mat_rock() {
  return {0.02f, 0.88f, 0.0f, 0.0f, 0.0f, 0.0f, 0.18f};
}

inline PbrMaterial mat_wood() {
  return {0.0f, 0.72f, 0.0f, 0.0f, 0.0f, 0.0f, 0.35f};
}

inline PbrMaterial mat_straw() {
  return {0.0f, 0.92f, 0.0f, 0.0f, 0.0f, 0.0f, 0.42f};
}

inline PbrMaterial mat_skin() {
  return {0.0f, 0.68f, 0.0f, 0.0f, 0.0f, 0.0f, 0.38f};
}

inline PbrMaterial mat_fabric() {
  return {0.0f, 0.78f, 0.0f, 0.0f, 0.0f, 0.0f, 0.40f};
}

inline PbrMaterial mat_sword_blade() {
  return {0.88f, 0.18f, 0.12f, 0.55f, 1.0f, 3.0f, 0.06f};
}

inline PbrMaterial mat_sword_handle() {
  return {0.0f, 0.65f, 0.0f, 0.0f, 0.0f, 0.0f, 0.28f};
}

} // namespace engine
