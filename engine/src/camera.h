#pragma once

#include "arena_mesh.h"

#include <cmath>

namespace engine {

constexpr float kCameraDistance = 5.0f;
constexpr float kCameraHeightOffset = 1.5f;
constexpr float kCameraTargetHeight = 0.95f;
constexpr float kCameraFloorClearance = 0.45f;
constexpr float kCameraFovY = 0.785398f;

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
                                        float camera_yaw, float camera_pitch) {
  OrbitCamera camera{};
  camera.target_x = player_x;
  camera.target_y = kCameraTargetHeight + player_y;
  camera.target_z = player_z;

  const float cos_pitch = std::cos(camera_pitch);
  const float sin_pitch = std::sin(camera_pitch);
  const float sin_yaw = std::sin(camera_yaw);
  const float cos_yaw = std::cos(camera_yaw);

  camera.x = player_x - sin_yaw * cos_pitch * kCameraDistance;
  camera.y = camera.target_y + kCameraHeightOffset + sin_pitch * kCameraDistance;
  camera.z = player_z - cos_yaw * cos_pitch * kCameraDistance;

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

} // namespace engine
