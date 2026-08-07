#include "engine/engine.h"

#include "arena_mesh.h"
#include "dummy_mesh.h"
#include "lan_network.h"
#include "renderer.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>

namespace engine {
namespace {

constexpr float kMoveSpeed = 6.0f;
constexpr float kSprintSpeedMultiplier = 1.5f;
constexpr float kMouseSensitivity = 0.0028f;
constexpr float kMinCameraPitch = -1.52f;
constexpr float kMaxCameraPitch = 1.52f;
constexpr float kMaxSwingAngle = 1.6f;
constexpr float kPi = 3.1415926535f;
constexpr float kWalkAnimSpeed = 12.0f;
constexpr float kDummyHitDamage = 34.0f;
constexpr float kDummyHitLean = 0.55f;
constexpr float kDummyHitRecoverSpeed = 2.5f;
constexpr float kDummyRespawnDelay = 3.0f;
constexpr float kPlayerRespawnDelay = 3.0f;
constexpr float kPlayerHitDamage = 34.0f;
constexpr float kFingerProjectileSpeed = 45.0f;
constexpr float kFingerProjectileLifetime = 1.5f;
constexpr float kFingerProjectileSweepStep = 0.025f;
constexpr float kFingerShotReleaseProgress = 0.20f;
constexpr float kFingerShotSpawnClearance = 0.08f;
constexpr float kFlyVerticalSpeed = 6.0f;
constexpr float kMaxFlyHeight = 12.0f;
// Detonate on the animation's ending pose, after the visible power-up has
// finished building to its climax.
constexpr float kUltimateBlastProgress = 0.92f;
constexpr float kUltimateWaveMaxRadius = 22.0f;
constexpr float kUltimateWaveSpeed = 34.0f;
constexpr float kUltimateDamage = 100.0f;
constexpr float kUltimateReturnDelay = 2.0f;
constexpr float kUltimateReformDuration = 1.5f;
constexpr float kUltimateDissolveDuration = 0.32f;

constexpr float kCharacterHeightUnits = 1.54f;
constexpr float kCharacterHeightFeet = 6.0f;
constexpr float kJumpHeightFeet = 3.0f;
constexpr float kGravityEarthFtPerSecSq = 32.2f;
constexpr float kUnitsPerFoot = kCharacterHeightUnits / kCharacterHeightFeet;
constexpr float kJumpHeightUnits = kJumpHeightFeet * kUnitsPerFoot;
constexpr float kGravity = kGravityEarthFtPerSecSq * kUnitsPerFoot * 2.0f;
constexpr float kJumpVelocity = 5.7f;

struct AppState {
  Renderer renderer;
  LanNetwork network;
  int framebuffer_width = 1280;
  int framebuffer_height = 720;
  float player_x = 0.0f;
  float player_y = 0.0f;
  float player_z = 0.0f;
  float player_vy = 0.0f;
  float player_yaw = 0.0f;
  float camera_yaw = 0.0f;
  float camera_pitch = -0.25f;
  float camera_side = 1.0f;
  float swing_progress = 0.0f;
  float walk_phase = 0.0f;
  float dummy_health = 100.0f;
  float dummy_hit_lean = 0.0f;
  float dummy_respawn_timer = 0.0f;
  bool swinging = false;
  bool swing_hit_landed = false;
  bool ultimate_active = false;
  bool ultimate_damage_landed = false;
  bool ultimate_blast_this_frame = false;
  bool ultimate_blast_triggered = false;
  bool ultimate_wave_active = false;
  bool ultimate_vanished = false;
  bool ultimate_returning = false;
  float ultimate_progress = 0.0f;
  float ultimate_return_timer = 0.0f;
  float ultimate_return_progress = 0.0f;
  float ultimate_wave_radius = 0.0f;
  float ultimate_dissolve_progress = 0.0f;
  bool grounded = true;
  bool flying = false;
  bool moving = false;
  bool sprinting = false;
  bool toggle_equip = false;
  bool dummy_alive = true;
  bool e_was_pressed = false;
  bool raytracing = false;
  bool toggle_character = false;
  bool r_was_pressed = false;
  bool c_was_pressed = false;
  bool f_was_pressed = false;
  bool v_was_pressed = false;
  bool q_was_pressed = false;
  bool space_was_pressed = false;
  int jump_press_count = 0;
  bool finger_projectile_active = false;
  bool finger_shooting = false;
  bool finger_shot_spawned = false;
  float finger_shoot_progress = 0.0f;
  float finger_projectile_x = 0.0f;
  float finger_projectile_y = 0.0f;
  float finger_projectile_z = 0.0f;
  float finger_projectile_dx = 0.0f;
  float finger_projectile_dy = 0.0f;
  float finger_projectile_dz = 1.0f;
  float finger_projectile_yaw = 0.0f;
  float finger_projectile_pitch = 0.0f;
  float finger_projectile_age = 0.0f;
  float finger_aim_x = 0.0f;
  float finger_aim_y = 0.0f;
  float finger_aim_z = 100.0f;
  float move_forward = 0.0f;
  float move_right = 0.0f;
  double last_mouse_x = 0.0;
  double last_mouse_y = 0.0;
  bool first_mouse_sample = true;
  float host_health = 100.0f;
  float client_health = 100.0f;
  float host_respawn_timer = 0.0f;
  float client_respawn_timer = 0.0f;
  bool host_alive = true;
  bool client_alive = true;
  bool last_received_client_alive = true;
  std::uint32_t swing_id = 0;
  std::uint32_t shot_id = 0;
  std::uint32_t ultimate_id = 0;
  std::uint32_t host_hit_by_swing_id = 0;
  std::uint32_t client_hit_by_swing_id = 0;
  std::uint32_t host_hit_by_shot_id = 0;
  std::uint32_t client_hit_by_shot_id = 0;
  std::uint32_t host_hit_by_ultimate_id = 0;
  std::uint32_t client_hit_by_ultimate_id = 0;
};

bool local_player_alive(const AppState &state) {
  if (state.network.mode() == LanNetwork::Mode::Host) {
    return state.host_alive;
  }
  if (state.network.mode() == LanNetwork::Mode::Client) {
    return state.client_alive;
  }
  return true;
}

void cancel_pharaoh_abilities(AppState &state) {
  state.swinging = false;
  state.swing_progress = 0.0f;
  state.swing_hit_landed = false;
  state.finger_shooting = false;
  state.finger_shot_spawned = false;
  state.finger_shoot_progress = 0.0f;
  state.finger_projectile_active = false;
  state.ultimate_active = false;
  state.ultimate_damage_landed = false;
  state.ultimate_blast_this_frame = false;
  state.ultimate_blast_triggered = false;
  state.ultimate_wave_active = false;
  state.ultimate_vanished = false;
  state.ultimate_returning = false;
  state.ultimate_progress = 0.0f;
  state.ultimate_wave_radius = 0.0f;
  state.ultimate_dissolve_progress = 0.0f;
  state.ultimate_return_progress = 0.0f;
}

void rotate_x_local(float &x, float &y, float &z, float angle) {
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  const float new_y = y * c - z * s;
  const float new_z = y * s + z * c;
  y = new_y;
  z = new_z;
  (void)x;
}

void rotate_y_local(float &x, float &y, float &z, float angle) {
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  const float new_x = x * c + z * s;
  const float new_z = -x * s + z * c;
  x = new_x;
  z = new_z;
  (void)y;
}

void sword_tip_world(const AppState &state, const SceneDrawParams &scene, float &world_x,
                     float &world_y, float &world_z) {
  state.renderer.sword_tip_world(scene, world_x, world_y, world_z);
}

bool point_in_dummy_hit(float local_x, float local_y, float local_z) {
  return local_x >= kDummyHitMinX && local_x <= kDummyHitMaxX &&
         local_y >= kDummyHitMinY && local_y <= kDummyHitMaxY &&
         local_z >= kDummyHitMinZ && local_z <= kDummyHitMaxZ;
}

bool point_in_dummy_projectile_hit(float local_x, float local_y, float local_z) {
  return local_x >= -0.40f && local_x <= 0.40f &&
         local_y >= 0.0f && local_y <= 1.45f &&
         local_z >= -0.22f && local_z <= 0.22f;
}

bool ray_hits_aabb(float origin_x, float origin_y, float origin_z,
                   float direction_x, float direction_y, float direction_z,
                   float min_x, float max_x, float min_y, float max_y,
                   float min_z, float max_z, float &hit_distance) {
  float near_t = 0.0f;
  float far_t = 1000.0f;
  const auto test_slab = [&](float origin, float direction, float min_value,
                             float max_value) {
    if (std::abs(direction) < 1e-6f) {
      return origin >= min_value && origin <= max_value;
    }
    float first = (min_value - origin) / direction;
    float second = (max_value - origin) / direction;
    if (first > second) {
      std::swap(first, second);
    }
    near_t = std::max(near_t, first);
    far_t = std::min(far_t, second);
    return near_t <= far_t;
  };

  if (!test_slab(origin_x, direction_x, min_x, max_x) ||
      !test_slab(origin_y, direction_y, min_y, max_y) ||
      !test_slab(origin_z, direction_z, min_z, max_z) || far_t <= 0.0f) {
    return false;
  }
  hit_distance = near_t > 0.0f ? near_t : far_t;
  return hit_distance > 0.0f;
}

bool dummy_exposed_to_ultimate(const AppState &state) {
  const float origin_x = state.player_x;
  const float origin_y = state.player_y + 0.78f;
  const float origin_z = state.player_z;
  const float target_x = kDummySpawnX;
  const float target_y = kDummySpawnY + 0.78f;
  const float target_z = kDummySpawnZ;
  float direction_x = target_x - origin_x;
  float direction_y = target_y - origin_y;
  float direction_z = target_z - origin_z;
  const float distance = std::sqrt(direction_x * direction_x +
                                   direction_y * direction_y +
                                   direction_z * direction_z);
  if (distance > kUltimateWaveMaxRadius || distance < 1e-4f) {
    return distance <= kUltimateWaveMaxRadius;
  }
  direction_x /= distance;
  direction_y /= distance;
  direction_z /= distance;

  for (const SolidBox &box : solid_boxes()) {
    float hit_distance = 0.0f;
    if (ray_hits_aabb(origin_x, origin_y, origin_z,
                      direction_x, direction_y, direction_z,
                      box.min_x, box.max_x, box.min_y, box.max_y,
                      box.min_z, box.max_z, hit_distance) &&
        hit_distance < distance - 0.15f) {
      return false;
    }
  }
  return true;
}

void crosshair_world_target(const OrbitCamera &camera, float vertical_ndc,
                            float &target_x, float &target_y, float &target_z) {
  float ray_x = 0.0f;
  float ray_y = 0.0f;
  float ray_z = 1.0f;
  crosshair_ray_direction(camera, vertical_ndc, ray_x, ray_y, ray_z);
  float closest = 100.0f;
  const auto test_box = [&](float min_x, float max_x, float min_y, float max_y,
                            float min_z, float max_z) {
    float distance = 0.0f;
    if (ray_hits_aabb(camera.x, camera.y, camera.z,
                      ray_x, ray_y, ray_z,
                      min_x, max_x, min_y, max_y, min_z, max_z, distance) &&
        distance < closest) {
      closest = distance;
    }
  };

  for (const SolidBox &box : solid_boxes()) {
    test_box(box.min_x, box.max_x, box.min_y, box.max_y, box.min_z, box.max_z);
  }

  // The dummy is rendered separately from the arena collision boxes.
  test_box(kDummySpawnX - 0.40f, kDummySpawnX + 0.40f,
           kDummySpawnY, kDummySpawnY + 1.45f,
           kDummySpawnZ - 0.22f, kDummySpawnZ + 0.22f);

  // Match the visible perimeter walls so the reticle can target them too.
  test_box(-kWorldHalfExtent, kWorldHalfExtent, 0.0f, 0.90f,
           -kWorldHalfExtent, -kWorldHalfExtent + 0.30f);
  test_box(-kWorldHalfExtent, kWorldHalfExtent, 0.0f, 0.90f,
           kWorldHalfExtent - 0.30f, kWorldHalfExtent);
  test_box(-kWorldHalfExtent, -kWorldHalfExtent + 0.30f, 0.0f, 0.90f,
           -kWorldHalfExtent, kWorldHalfExtent);
  test_box(kWorldHalfExtent - 0.30f, kWorldHalfExtent, 0.0f, 0.90f,
           -kWorldHalfExtent, kWorldHalfExtent);

  // The base arena floor is a plane rather than a collision box.
  if (ray_y < -1e-6f) {
    const float floor_distance = -camera.y / ray_y;
    if (floor_distance > 0.0f && floor_distance < closest) {
      closest = floor_distance;
    }
  }

  target_x = camera.x + ray_x * closest;
  target_y = camera.y + ray_y * closest;
  target_z = camera.z + ray_z * closest;
}

bool player_overlaps_dummy(float x, float y, float z) {
  constexpr float dummy_half_x = 0.40f;
  constexpr float dummy_half_z = 0.22f;
  constexpr float dummy_height = 1.45f;
  return aabb_overlaps(
      x - kPlayerRadius, x + kPlayerRadius, y, y + kPlayerHeight,
      z - kPlayerRadius, z + kPlayerRadius,
      kDummySpawnX - dummy_half_x, kDummySpawnX + dummy_half_x,
      kDummySpawnY, kDummySpawnY + dummy_height,
      kDummySpawnZ - dummy_half_z, kDummySpawnZ + dummy_half_z);
}

void depenetrate_dummy(float &x, float y, float &z) {
  if (!player_overlaps_dummy(x, y, z)) {
    return;
  }
  constexpr float dummy_half_x = 0.40f;
  constexpr float dummy_half_z = 0.22f;
  constexpr float skin = 0.01f;
  const float overlap_x = std::min(x + kPlayerRadius, kDummySpawnX + dummy_half_x) -
                          std::max(x - kPlayerRadius, kDummySpawnX - dummy_half_x);
  const float overlap_z = std::min(z + kPlayerRadius, kDummySpawnZ + dummy_half_z) -
                          std::max(z - kPlayerRadius, kDummySpawnZ - dummy_half_z);
  if (overlap_x < overlap_z) {
    x += x < kDummySpawnX ? -(overlap_x + skin) : overlap_x + skin;
  } else {
    z += z < kDummySpawnZ ? -(overlap_z + skin) : overlap_z + skin;
  }
}

bool sword_hits_dummy(const AppState &state, const SceneDrawParams &scene) {
  float staff_start_x = 0.0f;
  float staff_start_y = 0.0f;
  float staff_start_z = 0.0f;
  float staff_end_x = 0.0f;
  float staff_end_y = 0.0f;
  float staff_end_z = 0.0f;
  if (state.renderer.staff_segment_world(
          scene, staff_start_x, staff_start_y, staff_start_z,
          staff_end_x, staff_end_y, staff_end_z)) {
    // Convert the animated staff shaft into dummy-local space, then test the
    // entire segment against an expanded target box. Expanding the target is
    // equivalent to giving the staff a thick attack box instead of treating
    // its infinitely thin center line as the weapon.
    float start_x = staff_start_x - kDummySpawnX;
    float start_y = staff_start_y - kDummySpawnY;
    float start_z = staff_start_z - kDummySpawnZ;
    float end_x = staff_end_x - kDummySpawnX;
    float end_y = staff_end_y - kDummySpawnY;
    float end_z = staff_end_z - kDummySpawnZ;
    rotate_y_local(start_x, start_y, start_z, -kDummySpawnYaw);
    rotate_y_local(end_x, end_y, end_z, -kDummySpawnYaw);

    constexpr float attack_box_radius = 0.24f;
    const float direction_x = end_x - start_x;
    const float direction_y = end_y - start_y;
    const float direction_z = end_z - start_z;
    float near_t = 0.0f;
    float far_t = 1.0f;
    const auto test_axis = [&](float origin, float direction,
                               float min_value, float max_value) {
      if (std::abs(direction) < 1e-6f) {
        return origin >= min_value && origin <= max_value;
      }
      float first = (min_value - origin) / direction;
      float second = (max_value - origin) / direction;
      if (first > second) {
        std::swap(first, second);
      }
      near_t = std::max(near_t, first);
      far_t = std::min(far_t, second);
      return near_t <= far_t;
    };

    return test_axis(start_x, direction_x,
                     kDummyHitMinX - attack_box_radius,
                     kDummyHitMaxX + attack_box_radius) &&
           test_axis(start_y, direction_y,
                     kDummyHitMinY - attack_box_radius,
                     kDummyHitMaxY + attack_box_radius) &&
           test_axis(start_z, direction_z,
                     kDummyHitMinZ - attack_box_radius,
                     kDummyHitMaxZ + attack_box_radius);
  }

  // The samurai keeps the existing sword-tip collision behavior.
  float tip_x = 0.0f;
  float tip_y = 0.0f;
  float tip_z = 0.0f;
  sword_tip_world(state, scene, tip_x, tip_y, tip_z);

  float local_x = tip_x - kDummySpawnX;
  float local_y = tip_y - kDummySpawnY;
  float local_z = tip_z - kDummySpawnZ;
  rotate_y_local(local_x, local_y, local_z, -kDummySpawnYaw);
  return point_in_dummy_hit(local_x, local_y, local_z);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  auto *state = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  state->framebuffer_width = width;
  state->framebuffer_height = height;
  state->renderer.resize(width, height);
}

float clamp(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
  auto *state = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (state->first_mouse_sample) {
    state->last_mouse_x = xpos;
    state->last_mouse_y = ypos;
    state->first_mouse_sample = false;
    return;
  }

  const double delta_x = xpos - state->last_mouse_x;
  const double delta_y = ypos - state->last_mouse_y;
  state->last_mouse_x = xpos;
  state->last_mouse_y = ypos;

  state->camera_yaw -= static_cast<float>(delta_x) * kMouseSensitivity;
  state->camera_pitch = clamp(
      state->camera_pitch + static_cast<float>(delta_y) * kMouseSensitivity,
      kMinCameraPitch, kMaxCameraPitch);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int /*mods*/) {
  if (action != GLFW_PRESS) {
    return;
  }

  auto *state = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (!local_player_alive(*state)) {
    return;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    if (!state->renderer.using_pharaoh()) {
      return;
    }
    if (state->finger_shooting || state->ultimate_active ||
        state->ultimate_vanished || state->ultimate_returning || state->swinging) {
      return;
    }
    // Aim the Pharaoh where the camera is looking at the moment the shot is
    // requested. Movement remains available, but this facing is held through
    // the projectile's release frame.
    state->player_yaw = state->camera_yaw;
    const float forward_x = std::sin(state->player_yaw);
    const float forward_z = std::cos(state->player_yaw);
    state->finger_shooting = true;
    ++state->shot_id;
    state->finger_shot_spawned = false;
    state->finger_shoot_progress = 0.0f;
    state->finger_projectile_dx = forward_x;
    state->finger_projectile_dz = forward_z;
    state->finger_projectile_yaw = state->player_yaw;
    const OrbitCamera camera = compute_orbit_camera(
        state->player_x, state->player_y, state->player_z,
        state->camera_yaw, state->camera_pitch, state->camera_side);
    const float crosshair_ndc_y = head_level_crosshair_ndc_y(
        camera, state->player_x, state->player_y, state->player_z);
    crosshair_world_target(camera, crosshair_ndc_y, state->finger_aim_x,
                           state->finger_aim_y, state->finger_aim_z);
    return;
  }
  if (button != GLFW_MOUSE_BUTTON_LEFT) {
    return;
  }
  state->sprinting = false;
  if (state->ultimate_active || state->ultimate_vanished ||
      state->ultimate_returning || state->finger_shooting) {
    return;
  }
  if (!state->renderer.can_swing()) {
    return;
  }
  if (!state->swinging) {
    state->swinging = true;
    ++state->swing_id;
    state->swing_progress = 0.0f;
    state->swing_hit_landed = false;
  }
}

void update_finger_shot(AppState &state, float clip_duration, float delta_time) {
  if (!state.finger_shooting) {
    return;
  }
  const float duration = std::max(clip_duration, 0.001f);
  state.finger_shoot_progress += delta_time / duration;

  if (state.finger_shoot_progress >= 1.0f && state.finger_shot_spawned) {
    state.finger_shooting = false;
    state.finger_shoot_progress = 0.0f;
  }
}

bool release_finger_projectile(AppState &state, const SceneDrawParams &scene) {
  if (!state.finger_shooting || state.finger_shot_spawned ||
      state.finger_shoot_progress < kFingerShotReleaseProgress) {
    return false;
  }

  state.finger_shot_spawned = true;
  state.finger_projectile_active = true;
  state.finger_projectile_age = 0.0f;

  // Use the character's facing at the exact release frame. This keeps the
  // shot aligned even if movement turns the Pharaoh while his hand rises.
  state.finger_projectile_yaw = state.player_yaw;

  if (!state.renderer.finger_tip_world(
          scene, state.finger_projectile_x, state.finger_projectile_y,
          state.finger_projectile_z)) {
    state.finger_projectile_x = state.player_x + state.finger_projectile_dx * 0.42f;
    state.finger_projectile_y = state.player_y + 1.02f;
    state.finger_projectile_z = state.player_z + state.finger_projectile_dz * 0.42f;
  }
  float aim_dx = state.finger_aim_x - state.finger_projectile_x;
  float aim_dy = state.finger_aim_y - state.finger_projectile_y;
  float aim_dz = state.finger_aim_z - state.finger_projectile_z;
  const float aim_length = std::max(
      std::sqrt(aim_dx * aim_dx + aim_dy * aim_dy + aim_dz * aim_dz), 0.001f);
  state.finger_projectile_dx = aim_dx / aim_length;
  state.finger_projectile_dy = aim_dy / aim_length;
  state.finger_projectile_dz = aim_dz / aim_length;
  state.finger_projectile_yaw =
      std::atan2(state.finger_projectile_dx, state.finger_projectile_dz);
  state.finger_projectile_pitch = std::asin(
      std::clamp(state.finger_projectile_dy, -1.0f, 1.0f));
  // Start just beyond the fingertip so the centered projectile mesh cannot
  // overlap the hand or appear on the Pharaoh's back side.
  state.finger_projectile_x += state.finger_projectile_dx * kFingerShotSpawnClearance;
  state.finger_projectile_y += state.finger_projectile_dy * kFingerShotSpawnClearance;
  state.finger_projectile_z += state.finger_projectile_dz * kFingerShotSpawnClearance;
  return true;
}

void update_finger_projectile(AppState &state, float delta_time) {
  if (!state.finger_projectile_active) {
    return;
  }

  state.finger_projectile_age += delta_time;
  if (state.finger_projectile_age >= kFingerProjectileLifetime) {
    state.finger_projectile_active = false;
    return;
  }

  // Sweep the whole distance travelled this frame in bullet-sized increments.
  // A single endpoint test would tunnel through thin targets at this speed.
  const float travel = kFingerProjectileSpeed * delta_time;
  const int steps = std::max(1, static_cast<int>(std::ceil(travel / kFingerProjectileSweepStep)));
  const float step_distance = travel / static_cast<float>(steps);
  for (int step = 0; step < steps; ++step) {
    state.finger_projectile_x += state.finger_projectile_dx * step_distance;
    state.finger_projectile_y += state.finger_projectile_dy * step_distance;
    state.finger_projectile_z += state.finger_projectile_dz * step_distance;

    if (is_projectile_blocked(state.finger_projectile_x, state.finger_projectile_y,
                              state.finger_projectile_z)) {
      state.finger_projectile_active = false;
      return;
    }

    float local_x = state.finger_projectile_x - kDummySpawnX;
    float local_y = state.finger_projectile_y - kDummySpawnY;
    float local_z = state.finger_projectile_z - kDummySpawnZ;
    rotate_y_local(local_x, local_y, local_z, -kDummySpawnYaw);
    if (state.dummy_alive &&
        point_in_dummy_projectile_hit(local_x, local_y, local_z)) {
      state.finger_projectile_active = false;
      state.dummy_health -= kDummyHitDamage;
      state.dummy_hit_lean = kDummyHitLean;
      if (state.dummy_health <= 0.0f) {
        state.dummy_alive = false;
        state.dummy_hit_lean = 1.2f;
      }
      return;
    }
  }
}

void update_swing(AppState &state, float clip_duration, float delta_time) {
  if (!state.swinging) {
    return;
  }

  const float duration = std::max(clip_duration, 0.001f);
  state.swing_progress += delta_time / duration;
  if (state.swing_progress >= 1.0f) {
    state.swing_progress = 0.0f;
    state.swinging = false;
    state.swing_hit_landed = false;
  }
}

float current_swing_angle(const AppState &state) {
  if (!state.swinging) {
    return 0.0f;
  }
  return std::sin(state.swing_progress * kPi) * kMaxSwingAngle;
}

void update_combat(AppState &state, const SceneDrawParams &scene) {
  if (!state.dummy_alive || !state.swinging || state.swing_hit_landed) {
    return;
  }

  if (state.swing_progress < 0.22f || state.swing_progress > 0.78f) {
    return;
  }

  if (!sword_hits_dummy(state, scene)) {
    return;
  }

  state.swing_hit_landed = true;
  state.dummy_health -= kDummyHitDamage;
  state.dummy_hit_lean = kDummyHitLean;
  if (state.dummy_health <= 0.0f) {
    state.dummy_alive = false;
    state.dummy_hit_lean = 1.2f;
  }
}

void update_dummy_reaction(AppState &state, float delta_time) {
  if (!state.dummy_alive) {
    state.dummy_respawn_timer += delta_time;
    if (state.dummy_respawn_timer >= kDummyRespawnDelay) {
      state.dummy_alive = true;
      state.dummy_health = 100.0f;
      state.dummy_hit_lean = 0.0f;
      state.dummy_respawn_timer = 0.0f;
    }
    return;
  }
  if (state.dummy_hit_lean <= 0.0f) {
    return;
  }
  state.dummy_hit_lean = std::max(0.0f, state.dummy_hit_lean - kDummyHitRecoverSpeed * delta_time);
}

void update_vertical_motion(GLFWwindow *window, AppState &state, float delta_time) {
  if (!local_player_alive(state)) {
    state.player_vy = 0.0f;
    return;
  }
  const bool space_pressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
  const bool space_just_pressed = space_pressed && !state.space_was_pressed;
  const bool ultimate_lock = state.ultimate_active || state.ultimate_vanished ||
                             state.ultimate_returning;

  if (!ultimate_lock && !state.flying && space_just_pressed) {
    if (state.grounded) {
      state.player_vy = kJumpVelocity;
      state.grounded = false;
      state.jump_press_count = 1;
    } else if (state.jump_press_count == 1) {
      // A deliberate second Space press during the same jump enters flight.
      state.flying = true;
      state.player_vy = 0.0f;
      state.jump_press_count = 2;
    }
  }
  state.space_was_pressed = space_pressed;

  if (state.flying) {
    float vertical_input = 0.0f;
    if (!ultimate_lock && space_pressed) {
      vertical_input += 1.0f;
    }
    if (!ultimate_lock &&
        (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
         glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)) {
      vertical_input -= 1.0f;
    }
    const float fly_multiplier = state.sprinting ? kSprintSpeedMultiplier : 1.0f;
    state.player_y += vertical_input * kFlyVerticalSpeed * fly_multiplier * delta_time;
    const float support = resolve_support_height(
        state.player_x, state.player_z, state.player_y, 0.0f);
    state.player_y = std::clamp(state.player_y, support + 0.05f, kMaxFlyHeight);
    state.player_vy = 0.0f;
    state.grounded = false;
    depenetrate(state.player_x, state.player_y, state.player_z);
    depenetrate_dummy(state.player_x, state.player_y, state.player_z);
    return;
  }

  state.player_vy -= kGravity * delta_time;
  state.player_y += state.player_vy * delta_time;

  const float support =
      resolve_support_height(state.player_x, state.player_z, state.player_y, state.player_vy);
  if (state.player_y <= support) {
    state.player_y = support;
    state.player_vy = 0.0f;
    state.grounded = true;
    state.jump_press_count = 0;
  } else {
    state.grounded = false;
  }

  depenetrate(state.player_x, state.player_y, state.player_z);
  depenetrate_dummy(state.player_x, state.player_y, state.player_z);
}

void update_ultimate(AppState &state, float clip_duration, float delta_time) {
  state.ultimate_blast_this_frame = false;

  // The damaging volume follows the visible leading edge of the sand blast
  // instead of affecting the entire radius on its first frame.
  if (state.ultimate_wave_active) {
    state.ultimate_wave_radius = std::min(
        kUltimateWaveMaxRadius,
        state.ultimate_wave_radius + kUltimateWaveSpeed * delta_time);
    const float dummy_dx = kDummySpawnX - state.player_x;
    const float dummy_dz = kDummySpawnZ - state.player_z;
    const float dummy_distance = std::sqrt(dummy_dx * dummy_dx + dummy_dz * dummy_dz);
    if (!state.ultimate_damage_landed && state.dummy_alive &&
        dummy_distance <= state.ultimate_wave_radius &&
        dummy_exposed_to_ultimate(state)) {
      state.ultimate_damage_landed = true;
      state.dummy_health -= kUltimateDamage;
      state.dummy_hit_lean = 1.15f;
      if (state.dummy_health <= 0.0f) {
        state.dummy_alive = false;
        state.dummy_hit_lean = 1.2f;
      }
    }
    if (state.ultimate_wave_radius >= kUltimateWaveMaxRadius) {
      state.ultimate_wave_active = false;
    }
  }

  if (state.ultimate_returning) {
    state.ultimate_return_progress += delta_time / kUltimateReformDuration;
    if (state.ultimate_return_progress >= 1.0f) {
      state.ultimate_return_progress = 0.0f;
      state.ultimate_returning = false;
      state.ultimate_damage_landed = false;
      state.ultimate_blast_triggered = false;
      state.ultimate_dissolve_progress = 0.0f;
    }
    return;
  }
  if (!state.ultimate_active) {
    if (state.ultimate_vanished) {
      state.ultimate_return_timer =
          std::max(0.0f, state.ultimate_return_timer - delta_time);
      if (state.ultimate_return_timer <= 0.0f) {
        state.ultimate_vanished = false;
        state.ultimate_returning = true;
        state.ultimate_return_progress = 0.0f;
      }
    }
    return;
  }

  const float duration = std::max(clip_duration, 0.001f);
  state.ultimate_progress = std::min(
      1.0f, state.ultimate_progress + delta_time / duration);
  if (!state.ultimate_blast_triggered &&
      state.ultimate_progress >= kUltimateBlastProgress) {
    state.ultimate_blast_triggered = true;
    state.ultimate_blast_this_frame = true;
    state.ultimate_wave_active = true;
    state.ultimate_wave_radius = 0.0f;
  }

  if (state.ultimate_blast_triggered) {
    state.ultimate_dissolve_progress = std::min(
        1.0f,
        state.ultimate_dissolve_progress + delta_time / kUltimateDissolveDuration);
  }

  if (state.ultimate_progress >= 1.0f &&
      state.ultimate_dissolve_progress >= 1.0f) {
    state.ultimate_active = false;
    state.ultimate_progress = 0.0f;
    state.ultimate_vanished = true;
    state.ultimate_return_timer = kUltimateReturnDelay;
  }
}

void try_move_axis(AppState &state, float new_x, float new_z) {
  if (!is_position_blocked(new_x, state.player_y, new_z) &&
      !player_overlaps_dummy(new_x, state.player_y, new_z)) {
    state.player_x = new_x;
    state.player_z = new_z;
    return;
  }
  if (!is_position_blocked(new_x, state.player_y, state.player_z) &&
      !player_overlaps_dummy(new_x, state.player_y, state.player_z)) {
    state.player_x = new_x;
    return;
  }
  if (!is_position_blocked(state.player_x, state.player_y, new_z) &&
      !player_overlaps_dummy(state.player_x, state.player_y, new_z)) {
    state.player_z = new_z;
  }
}

void update_movement(GLFWwindow *window, AppState &state, float delta_time) {
  state.toggle_equip = false;
  const bool action_lock = state.renderer.action_overrides_sprint();

  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
  if (!local_player_alive(state)) {
    state.moving = false;
    state.sprinting = false;
    state.move_forward = 0.0f;
    state.move_right = 0.0f;
    return;
  }

  float input_forward = 0.0f;
  float input_right = 0.0f;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    input_forward += 1.0f;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    input_forward -= 1.0f;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    input_right += 1.0f;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    input_right -= 1.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }

  const bool e_pressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
  if (e_pressed && !state.e_was_pressed) {
    state.sprinting = false;
    state.toggle_equip = true;
  }
  state.e_was_pressed = e_pressed;

  const bool r_pressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
  if (r_pressed && !state.r_was_pressed) {
    state.raytracing = !state.raytracing;
  }

  const bool q_pressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
  if (q_pressed && !state.q_was_pressed && !state.ultimate_active &&
      !state.ultimate_vanished &&
      !state.ultimate_returning &&
      !state.swinging && !state.finger_shooting &&
      state.renderer.can_ultimate()) {
    state.ultimate_active = true;
    ++state.ultimate_id;
    state.ultimate_progress = 0.0f;
    state.ultimate_damage_landed = false;
    state.ultimate_blast_this_frame = false;
    state.ultimate_blast_triggered = false;
    state.ultimate_wave_active = false;
    state.ultimate_wave_radius = 0.0f;
    state.ultimate_dissolve_progress = 0.0f;
    state.ultimate_vanished = false;
    state.ultimate_return_timer = 0.0f;
    state.ultimate_returning = false;
    state.ultimate_return_progress = 0.0f;
    state.sprinting = false;
  }
  state.q_was_pressed = q_pressed;
  state.r_was_pressed = r_pressed;

  const bool c_pressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
  state.toggle_character = !state.network.online() &&
                           c_pressed && !state.c_was_pressed;
  if (state.toggle_character && state.renderer.using_pharaoh()) {
    cancel_pharaoh_abilities(state);
  }
  state.c_was_pressed = c_pressed;

  const bool f_pressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
  if (f_pressed && !state.f_was_pressed) {
    state.flying = !state.flying;
    state.player_vy = 0.0f;
    if (state.flying) {
      state.grounded = false;
    }
  }
  state.f_was_pressed = f_pressed;

  const bool v_pressed = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
  if (v_pressed && !state.v_was_pressed) {
    state.camera_side = -state.camera_side;
  }
  state.v_was_pressed = v_pressed;

  if (state.toggle_equip || action_lock || state.ultimate_active ||
      state.ultimate_vanished || state.ultimate_returning) {
    state.moving = false;
    state.sprinting = false;
    state.move_forward = 0.0f;
    state.move_right = 0.0f;
    return;
  }

  state.moving = input_forward != 0.0f || input_right != 0.0f;
  state.move_forward = input_forward;
  state.move_right = input_right;

  const bool shift_held = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  if (state.flying) {
    state.sprinting = shift_held && state.moving && !state.swinging;
  } else {
    const bool carrying_sprint_through_jump = !state.grounded && state.sprinting;
    state.sprinting =
        shift_held && input_forward > 0.0f && state.moving && !state.swinging &&
        (state.grounded || carrying_sprint_through_jump);
  }

  if (!state.moving) {
    return;
  }

  const float input_length =
      std::sqrt(input_forward * input_forward + input_right * input_right);
  input_forward /= input_length;
  input_right /= input_length;

  // Forward movement is the deliberate turn/aim input. It snaps the Pharaoh
  // toward the camera's forward direction, while idle camera orbiting and
  // pure strafing leave his aim unchanged.
  if (input_forward > 0.0f && !state.finger_shooting) {
    state.player_yaw = state.camera_yaw;
  } else if (input_forward < 0.0f && !state.finger_shooting) {
    // Retreating (straight or diagonally) snaps the Pharaoh's aim to the
    // camera direction while leaving the player's camera untouched.
    state.player_yaw = state.camera_yaw;
  }

  // Move in character space so W follows his facing, S retreats, and A/D
  // strafe across his aim line instead of rotating his whole body.
  const float forward_x = std::sin(state.player_yaw);
  const float forward_z = std::cos(state.player_yaw);
  const float right_x = std::cos(state.player_yaw);
  const float right_z = -std::sin(state.player_yaw);

  const float move_x = -input_right * right_x + input_forward * forward_x;
  const float move_z = -input_right * right_z + input_forward * forward_z;

  const float speed =
      state.sprinting ? kMoveSpeed * kSprintSpeedMultiplier : kMoveSpeed;
  const float delta_x = move_x * speed * delta_time;
  const float delta_z = move_z * speed * delta_time;
  try_move_axis(state, state.player_x + delta_x, state.player_z + delta_z);
  depenetrate(state.player_x, state.player_y, state.player_z);
  depenetrate_dummy(state.player_x, state.player_y, state.player_z);

  const float walk_rate = state.sprinting ? kWalkAnimSpeed * 1.35f : kWalkAnimSpeed;
  state.walk_phase += delta_time * walk_rate;
}

void update_walk_phase(AppState &state, float delta_time) {
  if (!state.moving) {
    state.walk_phase = 0.0f;
    return;
  }
  (void)delta_time;
}

NetworkPlayerState network_player_state(const AppState &state) {
  NetworkPlayerState player{};
  player.x = state.player_x;
  player.y = state.player_y;
  player.z = state.player_z;
  player.yaw = state.player_yaw;
  player.move_forward = state.move_forward;
  player.move_right = state.move_right;
  player.attack_progress = state.swing_progress;
  player.shoot_progress = state.finger_shoot_progress;
  player.ultimate_progress = state.ultimate_progress;
  player.ultimate_wave_radius = state.ultimate_wave_radius;
  player.projectile_x = state.finger_projectile_x;
  player.projectile_y = state.finger_projectile_y;
  player.projectile_z = state.finger_projectile_z;
  player.projectile_yaw = state.finger_projectile_yaw;
  player.projectile_pitch = state.finger_projectile_pitch;
  player.swing_id = state.swing_id;
  player.shot_id = state.shot_id;
  player.ultimate_id = state.ultimate_id;
  if (state.moving) player.flags |= kNetMoving;
  if (state.sprinting) player.flags |= kNetSprinting;
  if (!state.grounded) player.flags |= kNetAirborne;
  if (state.flying) player.flags |= kNetFlying;
  if (state.swinging) player.flags |= kNetSwinging;
  if (state.finger_shooting) player.flags |= kNetShooting;
  if (state.ultimate_active) player.flags |= kNetUltimate;
  if (state.finger_projectile_active) player.flags |= kNetProjectile;
  if (state.ultimate_vanished || !local_player_alive(state)) player.flags |= kNetHidden;
  return player;
}

void stop_local_actions(AppState &state) {
  state.moving = false;
  state.sprinting = false;
  state.swinging = false;
  state.finger_shooting = false;
  state.finger_projectile_active = false;
  state.ultimate_active = false;
  state.ultimate_vanished = false;
  state.ultimate_returning = false;
  state.ultimate_wave_active = false;
  state.move_forward = 0.0f;
  state.move_right = 0.0f;
}

void respawn_local_player(AppState &state, float spawn_x) {
  state.player_x = spawn_x;
  state.player_y = 0.0f;
  state.player_z = 0.0f;
  state.player_vy = 0.0f;
  state.grounded = true;
  state.flying = false;
  stop_local_actions(state);
}

bool staff_hits_player(const NetworkPlayerState &attacker,
                       const NetworkPlayerState &target) {
  if (!(attacker.flags & kNetSwinging) || attacker.attack_progress < 0.22f ||
      attacker.attack_progress > 0.78f) {
    return false;
  }
  const float dx = target.x - attacker.x;
  const float dy = target.y - attacker.y;
  const float dz = target.z - attacker.z;
  const float forward = dx * std::sin(attacker.yaw) + dz * std::cos(attacker.yaw);
  const float sideways = dx * std::cos(attacker.yaw) - dz * std::sin(attacker.yaw);
  return forward >= -0.30f && forward <= 2.0f && std::abs(sideways) <= 1.05f &&
         std::abs(dy) <= 1.55f;
}

bool projectile_hits_player(const NetworkPlayerState &attacker,
                            const NetworkPlayerState &target) {
  if (!(attacker.flags & kNetProjectile)) {
    return false;
  }
  const float cp = std::cos(attacker.projectile_pitch);
  const float direction_x = std::sin(attacker.projectile_yaw) * cp;
  const float direction_y = std::sin(attacker.projectile_pitch);
  const float direction_z = std::cos(attacker.projectile_yaw) * cp;
  const float target_x = target.x;
  const float target_y = target.y + 0.78f;
  const float target_z = target.z;
  const float to_target_x = target_x - attacker.projectile_x;
  const float to_target_y = target_y - attacker.projectile_y;
  const float to_target_z = target_z - attacker.projectile_z;
  const float along = std::clamp(
      -(to_target_x * direction_x + to_target_y * direction_y +
        to_target_z * direction_z), 0.0f, 1.15f);
  const float closest_x = attacker.projectile_x - direction_x * along;
  const float closest_y = attacker.projectile_y - direction_y * along;
  const float closest_z = attacker.projectile_z - direction_z * along;
  const float dx = target_x - closest_x;
  const float dy = target_y - closest_y;
  const float dz = target_z - closest_z;
  return dx * dx + dy * dy + dz * dz <= 0.48f * 0.48f;
}

bool player_exposed_to_ultimate(const NetworkPlayerState &attacker,
                                const NetworkPlayerState &target) {
  float dx = target.x - attacker.x;
  float dy = target.y - attacker.y;
  float dz = target.z - attacker.z;
  const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (distance < 1e-4f) {
    return true;
  }
  dx /= distance;
  dy /= distance;
  dz /= distance;
  for (const SolidBox &box : solid_boxes()) {
    float hit_distance = 0.0f;
    if (ray_hits_aabb(attacker.x, attacker.y + 0.78f, attacker.z,
                      dx, dy, dz, box.min_x, box.max_x, box.min_y, box.max_y,
                      box.min_z, box.max_z, hit_distance) &&
        hit_distance < distance - 0.15f) {
      return false;
    }
  }
  return true;
}

void damage_player(float &health, bool &alive, float damage) {
  if (!alive) {
    return;
  }
  health = std::max(0.0f, health - damage);
  if (health <= 0.0f) {
    alive = false;
  }
}

void resolve_lan_attack(const NetworkPlayerState &attacker,
                        const NetworkPlayerState &target, bool target_alive,
                        float &target_health, bool &target_alive_ref,
                        std::uint32_t &last_swing, std::uint32_t &last_shot,
                        std::uint32_t &last_ultimate) {
  if (!target_alive) {
    return;
  }
  if (attacker.swing_id != 0 && attacker.swing_id != last_swing &&
      staff_hits_player(attacker, target)) {
    last_swing = attacker.swing_id;
    damage_player(target_health, target_alive_ref, kPlayerHitDamage);
  }
  if (attacker.shot_id != 0 && attacker.shot_id != last_shot &&
      projectile_hits_player(attacker, target)) {
    last_shot = attacker.shot_id;
    damage_player(target_health, target_alive_ref, kPlayerHitDamage);
  }
  const float dx = target.x - attacker.x;
  const float dz = target.z - attacker.z;
  const float distance = std::sqrt(dx * dx + dz * dz);
  if (attacker.ultimate_id != 0 && attacker.ultimate_id != last_ultimate &&
      attacker.ultimate_wave_radius > 0.0f &&
      distance <= attacker.ultimate_wave_radius &&
      player_exposed_to_ultimate(attacker, target)) {
    last_ultimate = attacker.ultimate_id;
    damage_player(target_health, target_alive_ref, kUltimateDamage);
  }
}

void update_lan_multiplayer(AppState &state, float delta_time) {
  if (!state.network.online()) {
    return;
  }

  if (state.network.mode() == LanNetwork::Mode::Host) {
    if (!state.host_alive) {
      state.host_respawn_timer += delta_time;
      if (state.host_respawn_timer >= kPlayerRespawnDelay) {
        state.host_health = 100.0f;
        state.host_alive = true;
        state.host_respawn_timer = 0.0f;
        respawn_local_player(state, -2.0f);
      }
    }
    if (!state.client_alive) {
      state.client_respawn_timer += delta_time;
      if (state.client_respawn_timer >= kPlayerRespawnDelay) {
        state.client_health = 100.0f;
        state.client_alive = true;
        state.client_respawn_timer = 0.0f;
      }
    }
  }

  NetworkPlayerState local = network_player_state(state);
  state.network.tick(local, state.host_health, state.client_health,
                     state.host_alive, state.client_alive);

  if (state.network.mode() == LanNetwork::Mode::Client) {
    const bool was_alive = state.client_alive;
    state.host_health = state.network.received_host_health();
    state.client_health = state.network.received_client_health();
    state.host_alive = state.network.received_host_alive();
    state.client_alive = state.network.received_client_alive();
    if (was_alive && !state.client_alive) {
      stop_local_actions(state);
    } else if (!was_alive && state.client_alive) {
      respawn_local_player(state, 2.0f);
    }
    return;
  }

  if (!state.network.has_remote_state()) {
    return;
  }
  const NetworkPlayerState &remote = state.network.remote_state();
  local = network_player_state(state);
  resolve_lan_attack(local, remote, state.client_alive, state.client_health,
                     state.client_alive, state.client_hit_by_swing_id,
                     state.client_hit_by_shot_id, state.client_hit_by_ultimate_id);
  resolve_lan_attack(remote, local, state.host_alive, state.host_health,
                     state.host_alive, state.host_hit_by_swing_id,
                     state.host_hit_by_shot_id, state.host_hit_by_ultimate_id);
  if (!state.host_alive) {
    stop_local_actions(state);
  }
}

SceneDrawParams build_scene(const AppState &state, double current_time, float delta_time) {
  SceneDrawParams scene{};
  scene.player_x = state.player_x;
  scene.player_y = state.player_y;
  scene.player_z = state.player_z;
  scene.player_yaw = state.player_yaw;
  scene.camera_yaw = state.camera_yaw;
  scene.camera_pitch = state.camera_pitch;
  scene.camera_side = state.camera_side;
  scene.move_forward = state.move_forward;
  scene.move_right = state.move_right;
  scene.swing_angle = current_swing_angle(state);
  scene.attack_progress = state.swing_progress;
  scene.walk_phase = state.walk_phase;
  scene.dummy_hit_lean = state.dummy_hit_lean;
  scene.dummy_alive = state.dummy_alive;
  scene.moving = state.moving;
  scene.sprinting = state.sprinting;
  scene.airborne = !state.grounded;
  scene.flying = state.flying;
  scene.swinging = state.swinging;
  scene.dodging = false;
  scene.toggle_equip = state.toggle_equip;
  scene.raytracing = state.raytracing;
  scene.toggle_character = state.toggle_character && !state.network.online();
  scene.finger_projectile_active = state.finger_projectile_active;
  scene.finger_projectile_x = state.finger_projectile_x;
  scene.finger_projectile_y = state.finger_projectile_y;
  scene.finger_projectile_z = state.finger_projectile_z;
  scene.finger_projectile_yaw = state.finger_projectile_yaw;
  scene.finger_projectile_pitch = state.finger_projectile_pitch;
  scene.finger_shooting = state.finger_shooting;
  scene.finger_hidden = state.finger_shooting && state.finger_shot_spawned;
  scene.ultimate = state.ultimate_active;
  scene.ultimate_blast = state.ultimate_blast_this_frame;
  scene.ultimate_progress = state.ultimate_progress;
  scene.player_hidden = state.ultimate_vanished || !local_player_alive(state);
  scene.ultimate_returning = state.ultimate_returning;
  scene.ultimate_return_progress = state.ultimate_return_progress;
  if (state.ultimate_returning) {
    const float reveal = std::clamp(state.ultimate_return_progress, 0.0f, 1.0f);
    const float eased_reveal = reveal * reveal * (3.0f - 2.0f * reveal);
    scene.player_dissolve = 0.0f;
    scene.player_reveal_progress = eased_reveal;
    scene.player_opacity = 1.0f;
  } else {
    const float dissolve = std::clamp(state.ultimate_dissolve_progress, 0.0f, 1.0f);
    scene.player_dissolve = dissolve * dissolve * (3.0f - 2.0f * dissolve);
  }
  scene.delta_time = delta_time;
  scene.time = static_cast<float>(current_time);
  if (state.network.has_remote_state()) {
    const NetworkPlayerState &remote = state.network.remote_state();
    const bool remote_alive = state.network.mode() == LanNetwork::Mode::Host
                                  ? state.client_alive
                                  : state.host_alive;
    scene.remote_active = true;
    scene.remote_hidden = !remote_alive || (remote.flags & kNetHidden) != 0;
    scene.remote_moving = (remote.flags & kNetMoving) != 0;
    scene.remote_sprinting = (remote.flags & kNetSprinting) != 0;
    scene.remote_airborne = (remote.flags & kNetAirborne) != 0;
    scene.remote_flying = (remote.flags & kNetFlying) != 0;
    scene.remote_swinging = (remote.flags & kNetSwinging) != 0;
    scene.remote_shooting = (remote.flags & kNetShooting) != 0;
    scene.remote_ultimate = (remote.flags & kNetUltimate) != 0;
    scene.remote_projectile_active = remote_alive &&
                                     (remote.flags & kNetProjectile) != 0;
    scene.remote_x = remote.x;
    scene.remote_y = remote.y;
    scene.remote_z = remote.z;
    scene.remote_yaw = remote.yaw;
    scene.remote_move_forward = remote.move_forward;
    scene.remote_move_right = remote.move_right;
    scene.remote_projectile_x = remote.projectile_x;
    scene.remote_projectile_y = remote.projectile_y;
    scene.remote_projectile_z = remote.projectile_z;
    scene.remote_projectile_yaw = remote.projectile_yaw;
    scene.remote_projectile_pitch = remote.projectile_pitch;
  }
  return scene;
}

} // namespace

int run() {
  if (!glfwInit()) {
    std::fprintf(stderr, "Failed to initialize GLFW\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

  AppState state{};
  GLFWwindow *window = glfwCreateWindow(
      state.framebuffer_width, state.framebuffer_height, "videogame", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return 1;
  }

  glfwSetWindowUserPointer(window, &state);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, cursor_position_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (!state.renderer.init()) {
    std::fprintf(stderr, "Failed to initialize renderer\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  if (!state.network.init_from_environment()) {
    std::fprintf(stderr, "Failed to initialize LAN multiplayer\n");
    state.renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
  if (state.network.mode() == LanNetwork::Mode::Host) {
    state.player_x = -2.0f;
    glfwSetWindowTitle(window, "videogame - LAN HOST");
  } else if (state.network.mode() == LanNetwork::Mode::Client) {
    state.player_x = 2.0f;
    glfwSetWindowTitle(window, "videogame - LAN JOINER");
  }

  state.renderer.resize(state.framebuffer_width, state.framebuffer_height);
  double previous_time = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    const double current_time = glfwGetTime();
    const float delta_time = static_cast<float>(current_time - previous_time);
    previous_time = current_time;

    update_movement(window, state, delta_time);
    update_walk_phase(state, delta_time);
    update_vertical_motion(window, state, delta_time);
    update_finger_shot(state, state.renderer.shoot_clip_duration(), delta_time);
    update_finger_projectile(state, delta_time);
    update_swing(state, state.renderer.swing_clip_duration(), delta_time);
    update_ultimate(state, state.renderer.ultimate_clip_duration(), delta_time);
    update_lan_multiplayer(state, delta_time);
    if (state.network.online()) {
      char title[128]{};
      const bool hosting = state.network.mode() == LanNetwork::Mode::Host;
      const float your_health = hosting ? state.host_health : state.client_health;
      const float opponent_health = hosting ? state.client_health : state.host_health;
      std::snprintf(title, sizeof(title), "videogame - %s  YOU %d HP  OPPONENT %d HP%s",
                    hosting ? "LAN HOST" : "LAN JOINER",
                    static_cast<int>(your_health), static_cast<int>(opponent_health),
                    state.network.connected() ? "" : "  (waiting for player)");
      glfwSetWindowTitle(window, title);
    }
    SceneDrawParams scene = build_scene(state, current_time, delta_time);
    state.renderer.update_samurai(scene);
    if (release_finger_projectile(state, scene)) {
      // Make the newly released projectile visible at the fingertip this frame.
      scene = build_scene(state, current_time, delta_time);
    }
    update_combat(state, scene);
    update_dummy_reaction(state, delta_time);
    state.renderer.draw(scene);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  state.network.shutdown();
  state.renderer.shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

} // namespace engine
