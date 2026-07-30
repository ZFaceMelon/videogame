#pragma once

#include "camera.h"
#include "gltf_asset.h"
#include "math.h"

namespace engine {

struct SceneDrawParams {
  float player_x = 0.0f;
  float player_y = 0.0f;
  float player_z = 0.0f;
  float player_yaw = 0.0f;
  float camera_yaw = 0.0f;
  float camera_pitch = 0.0f;
  float move_forward = 0.0f;
  float move_right = 0.0f;
  float swing_angle = 0.0f;
  float walk_phase = 0.0f;
  float dummy_hit_lean = 0.0f;
  float delta_time = 0.0f;
  bool moving = false;
  bool sprinting = false;
  bool swinging = false;
  bool dodging = false;
  bool toggle_equip = false;
  bool dummy_alive = true;
  float time = 0.0f;
};

struct SceneTransforms {
  Mat4 dummy_model{};
  Mat4 dummy_body_local{};
};

class Renderer {
public:
  bool init();
  void resize(int width, int height);
  void update_samurai(const SceneDrawParams &scene);
  float swing_clip_duration() const;
  bool sword_drawn() const;
  bool can_swing() const;
  bool action_overrides_sprint() const;
  bool equip_active() const;
  bool sword_tip_world(const SceneDrawParams &scene, float &x, float &y, float &z) const;
  void draw(const SceneDrawParams &scene);
  void shutdown();

private:
  unsigned int mesh_program_ = 0;
  unsigned int skinned_program_ = 0;
  unsigned int rigid_program_ = 0;
  unsigned int sky_program_ = 0;
  unsigned int raytrace_program_ = 0;
  unsigned int vao_ = 0;
  unsigned int sky_vao_ = 0;
  unsigned int arena_vbo_ = 0;
  unsigned int dummy_post_vbo_ = 0;
  unsigned int dummy_body_vbo_ = 0;
  unsigned int sky_vbo_ = 0;
  int arena_vertex_count_ = 0;
  int dummy_post_vertex_count_ = 0;
  int dummy_body_vertex_count_ = 0;
  int width_ = 1280;
  int height_ = 720;

  SamuraiAsset samurai_;

  int mesh_mvp_location_ = -1;
  int mesh_model_location_ = -1;
  int mesh_light_location_ = -1;

  int sky_camera_pos_location_ = -1;
  int sky_camera_right_location_ = -1;
  int sky_camera_up_location_ = -1;
  int sky_camera_forward_location_ = -1;
  int sky_tan_half_fov_location_ = -1;
  int sky_aspect_location_ = -1;
  int sky_sun_dir_location_ = -1;
  int sky_time_location_ = -1;
  int ray_aspect_location_ = -1;

  SceneTransforms compute_scene_transforms(const SceneDrawParams &scene) const;
  Mat4 player_model_matrix(const SceneDrawParams &scene) const;
  void bind_mesh_vbo(unsigned int vbo);
  void draw_lit_mesh(const Mat4 &mvp, const Mat4 &model, unsigned int vbo, int vertex_count);
  void draw_scene_meshes(const Mat4 &projection, const Mat4 &view,
                         const SceneDrawParams &scene, const SceneTransforms &transforms);
  void draw_sky(const OrbitCamera &camera, float aspect, float time);
  void draw_raytrace(float aspect);
  unsigned int compile_shader(unsigned int type, const char *source);
  unsigned int link_program(unsigned int vertex_shader, unsigned int fragment_shader);
};

} // namespace engine
