#pragma once

#include "mesh_builder.h"

#include <vector>

namespace engine {

// Weapon-side arm in mesh space (-0.34 is the player's right hand on screen).
constexpr float kWeaponArmCenterX = -0.34f;
constexpr float kWeaponArmCenterY = 0.88f;
constexpr float kWeaponArmCenterZ = 0.0f;
constexpr float kArmHalfX = 0.08f;
constexpr float kArmHalfY = 0.28f;
constexpr float kArmHalfZ = 0.08f;

constexpr float kWeaponShoulderX = kWeaponArmCenterX;
constexpr float kWeaponShoulderY = kWeaponArmCenterY + kArmHalfY;
constexpr float kWeaponShoulderZ = kWeaponArmCenterZ;

constexpr float kWeaponHandX = kWeaponArmCenterX - kArmHalfX + 0.02f;
constexpr float kWeaponHandY = kWeaponArmCenterY - kArmHalfY + 0.02f;
constexpr float kWeaponHandZ = kWeaponArmCenterZ + kArmHalfZ + 0.02f;

constexpr float kCharacterHeightUnits = 1.54f;

constexpr float kLeftLegX = -0.12f;
constexpr float kRightLegX = 0.12f;
constexpr float kLegCenterY = 0.30f;
constexpr float kLegHalfX = 0.10f;
constexpr float kLegHalfY = 0.30f;
constexpr float kLegHalfZ = 0.10f;

inline std::vector<float> build_left_leg() {
  std::vector<float> vertices;
  vertices.reserve(36 * 6);
  constexpr float pants[] = {0.40f, 0.42f, 0.45f};
  append_box(vertices, 0.0f, 0.0f, 0.0f, kLegHalfX, kLegHalfY, kLegHalfZ,
             pants[0], pants[1], pants[2]);
  return vertices;
}

inline std::vector<float> build_right_leg() {
  return build_left_leg();
}

inline std::vector<float> build_character_torso() {
  std::vector<float> vertices;
  vertices.reserve(3 * 36 * 6);

  constexpr float skin[] = {0.72f, 0.70f, 0.68f};
  constexpr float shirt[] = {0.48f, 0.50f, 0.53f};

  append_box(vertices, 0.0f, 0.90f, 0.0f, 0.22f, 0.30f, 0.12f, shirt[0], shirt[1], shirt[2]);
  append_box(vertices, 0.34f, 0.88f, 0.0f, kArmHalfX, kArmHalfY, kArmHalfZ, skin[0], skin[1], skin[2]);
  append_box(vertices, 0.0f, 1.34f, 0.0f, 0.20f, 0.20f, 0.20f, skin[0], skin[1], skin[2]);

  return vertices;
}

inline std::vector<float> build_weapon_arm() {
  std::vector<float> vertices;
  vertices.reserve(3 * 36 * 6);

  constexpr float skin[] = {0.72f, 0.70f, 0.68f};

  append_box(vertices, 0.0f, -kArmHalfY, 0.0f, kArmHalfX, kArmHalfY, kArmHalfZ,
             skin[0], skin[1], skin[2]);

  return vertices;
}

inline std::vector<float> build_sword_mesh() {
  std::vector<float> vertices;
  vertices.reserve(2 * 36 * 6);

  constexpr float brown[] = {0.45f, 0.30f, 0.15f};
  constexpr float blade[] = {0.15f, 0.55f, 1.0f};

  const float hand_x = kWeaponHandX - kWeaponShoulderX;
  const float hand_y = kWeaponHandY - kWeaponShoulderY;
  const float hand_z = kWeaponHandZ - kWeaponShoulderZ;

  append_box(vertices, hand_x, hand_y, hand_z + 0.06f, 0.014f, 0.014f, 0.06f,
             brown[0], brown[1], brown[2]);
  append_box(vertices, hand_x, hand_y, hand_z + 0.30f, 0.018f, 0.055f, 0.20f,
             blade[0], blade[1], blade[2]);

  return vertices;
}

} // namespace engine
