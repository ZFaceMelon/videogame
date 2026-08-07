#pragma once

#include "arena_mesh.h"

#include <algorithm>
#include <cmath>

namespace engine {

constexpr float kCameraDistance = 5.0f;
constexpr float kCameraHeightOffset = 1.5f;
constexpr float kCameraTargetHeight = 0.95f;
constexpr float kCameraFloorClearance = 0.45f;
constexpr float kCameraFovY = 0.785398f;
constexpr float kCameraShoulderOffset = 0.72f;
constexpr float kCharacterAimHeadHeight = 1.46f;

struct OrbitCamera {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float target_x = 0.0f;
  float target_y = 0.0f;
  float target_z = 0.0f;
  float forward_x = 0.0f;
  float forward_y = 0.0f;
  float forward_z = 1.0f;
  float right_x = 1.0f;
  float right_y = 0.0f;
  float right_z = 0.0f;
  float up_x = 0.0f;
  float up_y = 1.0f;
  float up_z = 0.0f;
};

inline void cross3(float ax, float ay, float az, float bx, float by, float bz,
                   float &out_x, float &out_y, float &out_z) {
  out_x = ay * bz - az * by;
  out_y = az * bx - ax * bz;
  out_z = ax * by - ay * bx;
}

inline OrbitCamera compute_orbit_camera(float player_x, float player_y, float player_z,
                                        float camera_yaw, float camera_pitch,
                                        float shoulder_side = 0.0f) {
  OrbitCamera camera{};
  const float sin_yaw = std::sin(camera_yaw);
  const float cos_yaw = std::cos(camera_yaw);
  const float shoulder_x = -cos_yaw * kCameraShoulderOffset * shoulder_side;
  const float shoulder_z = sin_yaw * kCameraShoulderOffset * shoulder_side;

  camera.target_x = player_x + shoulder_x;
  camera.target_y = kCameraTargetHeight + player_y;
  camera.target_z = player_z + shoulder_z;

  const float cos_pitch = std::cos(camera_pitch);
  const float sin_pitch = std::sin(camera_pitch);

  camera.x = player_x + shoulder_x - sin_yaw * cos_pitch * kCameraDistance;
  camera.y = camera.target_y + kCameraHeightOffset + sin_pitch * kCameraDistance;
  camera.z = player_z + shoulder_z - cos_yaw * cos_pitch * kCameraDistance;

  const float floor_y =
      resolve_support_height(camera.x, camera.z, camera.y, -1.0f);
  const float min_camera_y = floor_y + kCameraFloorClearance;
  if (camera.y < min_camera_y) {
    camera.y = min_camera_y;
  }

  camera.forward_x = camera.target_x - camera.x;
  camera.forward_y = camera.target_y - camera.y;
  camera.forward_z = camera.target_z - camera.z;
  normalize3(camera.forward_x, camera.forward_y, camera.forward_z);

  constexpr float world_up_x = 0.0f;
  constexpr float world_up_y = 1.0f;
  constexpr float world_up_z = 0.0f;
  cross3(camera.forward_x, camera.forward_y, camera.forward_z,
         world_up_x, world_up_y, world_up_z,
         camera.right_x, camera.right_y, camera.right_z);
  normalize3(camera.right_x, camera.right_y, camera.right_z);

  cross3(camera.right_x, camera.right_y, camera.right_z,
         camera.forward_x, camera.forward_y, camera.forward_z,
         camera.up_x, camera.up_y, camera.up_z);
  normalize3(camera.up_x, camera.up_y, camera.up_z);

  return camera;
}

inline float head_level_crosshair_ndc_y(const OrbitCamera &camera,
                                        float player_x, float player_y,
                                        float player_z) {
  const float relative_x = player_x - camera.x;
  const float relative_y = player_y + kCharacterAimHeadHeight - camera.y;
  const float relative_z = player_z - camera.z;
  const float view_y = relative_x * camera.up_x + relative_y * camera.up_y +
                       relative_z * camera.up_z;
  const float view_depth = relative_x * camera.forward_x +
                           relative_y * camera.forward_y +
                           relative_z * camera.forward_z;
  const float tan_half_fov = std::tan(kCameraFovY * 0.5f);
  if (view_depth <= 0.01f || tan_half_fov <= 0.001f) {
    return 0.0f;
  }
  return std::clamp(view_y / (view_depth * tan_half_fov), -0.82f, 0.82f);
}

inline void crosshair_ray_direction(const OrbitCamera &camera, float vertical_ndc,
                                    float &x, float &y, float &z) {
  const float vertical_scale = vertical_ndc * std::tan(kCameraFovY * 0.5f);
  x = camera.forward_x + camera.up_x * vertical_scale;
  y = camera.forward_y + camera.up_y * vertical_scale;
  z = camera.forward_z + camera.up_z * vertical_scale;
  normalize3(x, y, z);
}

} // namespace engine
