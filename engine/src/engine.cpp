#include "engine/engine.h"

#include "arena_mesh.h"
#include "dummy_mesh.h"
#include "renderer.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>

namespace engine {
namespace {

constexpr float kMoveSpeed = 4.0f;
constexpr float kSprintSpeedMultiplier = 1.2f;
constexpr float kMouseSensitivity = 0.0025f;
constexpr float kMinCameraPitch = -1.52f;
constexpr float kMaxCameraPitch = 1.52f;
constexpr float kMaxSwingAngle = 1.6f;
constexpr float kPi = 3.1415926535f;
constexpr float kWalkAnimSpeed = 9.0f;
constexpr float kDummyHitDamage = 34.0f;
constexpr float kDummyHitLean = 0.55f;
constexpr float kDummyHitRecoverSpeed = 2.5f;

constexpr float kCharacterHeightUnits = 1.54f;
constexpr float kCharacterHeightFeet = 6.0f;
constexpr float kJumpHeightFeet = 3.0f;
constexpr float kGravityEarthFtPerSecSq = 32.2f;
constexpr float kUnitsPerFoot = kCharacterHeightUnits / kCharacterHeightFeet;
constexpr float kJumpHeightUnits = kJumpHeightFeet * kUnitsPerFoot;
constexpr float kGravity = kGravityEarthFtPerSecSq * kUnitsPerFoot;
constexpr float kJumpVelocity = 3.564f;

struct AppState {
  Renderer renderer;
  int framebuffer_width = 1280;
  int framebuffer_height = 720;
  float player_x = 0.0f;
  float player_y = 0.0f;
  float player_z = 0.0f;
  float player_vy = 0.0f;
  float player_yaw = 0.0f;
  float camera_yaw = 0.0f;
  float camera_pitch = -0.25f;
  float swing_progress = 0.0f;
  float walk_phase = 0.0f;
  float dummy_health = 100.0f;
  float dummy_hit_lean = 0.0f;
  bool swinging = false;
  bool swing_hit_landed = false;
  bool grounded = true;
  bool moving = false;
  bool sprinting = false;
  bool toggle_equip = false;
  bool dummy_alive = true;
  bool e_was_pressed = false;
  float move_forward = 0.0f;
  float move_right = 0.0f;
  double last_mouse_x = 0.0;
  double last_mouse_y = 0.0;
  bool first_mouse_sample = true;
};

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

bool sword_hits_dummy(const AppState &state, const SceneDrawParams &scene) {
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
  state->player_yaw = state->camera_yaw;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int /*mods*/) {
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) {
    return;
  }

  auto *state = static_cast<AppState *>(glfwGetWindowUserPointer(window));
  state->sprinting = false;
  if (!state->renderer.can_swing()) {
    return;
  }
  if (!state->swinging) {
    state->swinging = true;
    state->swing_progress = 0.0f;
    state->swing_hit_landed = false;
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
  if (!state.dummy_alive || state.dummy_hit_lean <= 0.0f) {
    return;
  }
  state.dummy_hit_lean = std::max(0.0f, state.dummy_hit_lean - kDummyHitRecoverSpeed * delta_time);
}

void update_vertical_motion(GLFWwindow *window, AppState &state, float delta_time) {
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && state.grounded) {
    state.player_vy = kJumpVelocity;
    state.grounded = false;
  }

  state.player_vy -= kGravity * delta_time;
  state.player_y += state.player_vy * delta_time;

  const float support =
      resolve_support_height(state.player_x, state.player_z, state.player_y, state.player_vy);
  if (state.player_y <= support) {
    state.player_y = support;
    state.player_vy = 0.0f;
    state.grounded = true;
  } else {
    state.grounded = false;
  }

  depenetrate(state.player_x, state.player_y, state.player_z);
}

void try_move_axis(AppState &state, float new_x, float new_z) {
  if (!is_position_blocked(new_x, state.player_y, new_z)) {
    state.player_x = new_x;
    state.player_z = new_z;
    return;
  }
  if (!is_position_blocked(new_x, state.player_y, state.player_z)) {
    state.player_x = new_x;
    return;
  }
  if (!is_position_blocked(state.player_x, state.player_y, new_z)) {
    state.player_z = new_z;
  }
}

void update_movement(GLFWwindow *window, AppState &state, float delta_time) {
  state.toggle_equip = false;
  state.player_yaw = state.camera_yaw;
  const bool action_lock = state.renderer.action_overrides_sprint();

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

  if (state.toggle_equip || action_lock) {
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
  state.sprinting =
      shift_held && input_forward > 0.0f && state.moving && state.grounded &&
      !state.swinging;

  if (!state.moving) {
    return;
  }

  const float input_length =
      std::sqrt(input_forward * input_forward + input_right * input_right);
  input_forward /= input_length;
  input_right /= input_length;

  const float forward_x = std::sin(state.camera_yaw);
  const float forward_z = std::cos(state.camera_yaw);
  const float right_x = std::cos(state.camera_yaw);
  const float right_z = -std::sin(state.camera_yaw);

  const float move_x = -input_right * right_x + input_forward * forward_x;
  const float move_z = -input_right * right_z + input_forward * forward_z;

  const float speed =
      state.sprinting ? kMoveSpeed * kSprintSpeedMultiplier : kMoveSpeed;
  const float delta_x = move_x * speed * delta_time;
  const float delta_z = move_z * speed * delta_time;
  try_move_axis(state, state.player_x + delta_x, state.player_z + delta_z);
  depenetrate(state.player_x, state.player_y, state.player_z);

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

SceneDrawParams build_scene(const AppState &state, double current_time, float delta_time) {
  SceneDrawParams scene{};
  scene.player_x = state.player_x;
  scene.player_y = state.player_y;
  scene.player_z = state.player_z;
  scene.player_yaw = state.camera_yaw;
  scene.camera_yaw = state.camera_yaw;
  scene.camera_pitch = state.camera_pitch;
  scene.move_forward = state.move_forward;
  scene.move_right = state.move_right;
  scene.swing_angle = current_swing_angle(state);
  scene.walk_phase = state.walk_phase;
  scene.dummy_hit_lean = state.dummy_hit_lean;
  scene.dummy_alive = state.dummy_alive;
  scene.moving = state.moving;
  scene.sprinting = state.sprinting;
  scene.swinging = state.swinging;
  scene.dodging = false;
  scene.toggle_equip = state.toggle_equip;
  scene.delta_time = delta_time;
  scene.time = static_cast<float>(current_time);
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

  state.renderer.resize(state.framebuffer_width, state.framebuffer_height);
  double previous_time = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    const double current_time = glfwGetTime();
    const float delta_time = static_cast<float>(current_time - previous_time);
    previous_time = current_time;

    update_movement(window, state, delta_time);
    update_walk_phase(state, delta_time);
    update_vertical_motion(window, state, delta_time);
    update_swing(state, state.renderer.swing_clip_duration(), delta_time);
    const SceneDrawParams scene = build_scene(state, current_time, delta_time);
    state.renderer.update_samurai(scene);
    update_combat(state, scene);
    update_dummy_reaction(state, delta_time);
    state.renderer.draw(scene);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  state.renderer.shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

} // namespace engine
