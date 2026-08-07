#pragma once

#include "camera.h"
#include "fbx_asset.h"
#include "gltf_asset.h"
#include "math.h"

#include <cstdint>
#include <vector>

namespace engine {

struct SceneDrawParams {
  float player_x = 0.0f;
  float player_y = 0.0f;
  float player_z = 0.0f;
  float player_yaw = 0.0f;
  float camera_yaw = 0.0f;
  float camera_pitch = 0.0f;
  float camera_side = 1.0f;
  float move_forward = 0.0f;
  float move_right = 0.0f;
  float swing_angle = 0.0f;
  float walk_phase = 0.0f;
  float dummy_hit_lean = 0.0f;
  float delta_time = 0.0f;
  bool moving = false;
  bool sprinting = false;
  bool airborne = false;
  bool flying = false;
  bool swinging = false;
  bool finger_shooting = false;
  bool finger_hidden = false;
  bool ultimate = false;
  bool ultimate_blast = false;
  bool player_hidden = false;
  bool ultimate_returning = false;
  bool dodging = false;
  bool toggle_equip = false;
  bool dummy_alive = true;
  bool raytracing = false;
  bool toggle_character = false;
  bool finger_projectile_active = false;
  float time = 0.0f;
  float attack_progress = 0.0f;
  float ultimate_progress = 0.0f;
  float ultimate_return_progress = 0.0f;
  float player_opacity = 1.0f;
  float player_dissolve = 0.0f;
  float player_reveal_progress = 1.0f;
  float finger_projectile_x = 0.0f;
  float finger_projectile_y = 0.0f;
  float finger_projectile_z = 0.0f;
  float finger_projectile_yaw = 0.0f;
  float finger_projectile_pitch = 0.0f;
  bool remote_active = false;
  bool remote_hidden = false;
  bool remote_moving = false;
  bool remote_sprinting = false;
  bool remote_airborne = false;
  bool remote_flying = false;
  bool remote_swinging = false;
  bool remote_shooting = false;
  bool remote_ultimate = false;
  bool remote_projectile_active = false;
  float remote_x = 0.0f;
  float remote_y = 0.0f;
  float remote_z = 0.0f;
  float remote_yaw = 0.0f;
  float remote_move_forward = 0.0f;
  float remote_move_right = 0.0f;
  float remote_projectile_x = 0.0f;
  float remote_projectile_y = 0.0f;
  float remote_projectile_z = 0.0f;
  float remote_projectile_yaw = 0.0f;
  float remote_projectile_pitch = 0.0f;
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
  float shoot_clip_duration() const;
  float ultimate_clip_duration() const;
  bool sword_drawn() const;
  bool can_swing() const;
  bool can_ultimate() const;
  bool using_pharaoh() const;
  bool action_overrides_sprint() const;
  bool equip_active() const;
  bool sword_tip_world(const SceneDrawParams &scene, float &x, float &y, float &z) const;
  bool staff_segment_world(const SceneDrawParams &scene,
                           float &start_x, float &start_y, float &start_z,
                           float &end_x, float &end_y, float &end_z) const;
  bool finger_tip_world(const SceneDrawParams &scene, float &x, float &y, float &z) const;
  void draw(const SceneDrawParams &scene);
  void shutdown();

private:
  struct SandParticle {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float age = 0.0f;
    float lifetime = 0.6f;
    float size = 0.025f;
    float tint_r = 1.0f;
    float tint_g = 1.0f;
    float tint_b = 1.0f;
    bool cloud = false;
    float cloud_seed = 0.0f;
    // 0 = free/falling, 1 = ground-to-staff formation,
    // 2 = staff-to-ground dismissal.
    int behavior = 0;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    float origin_z = 0.0f;
    float destination_x = 0.0f;
    float destination_y = 0.0f;
    float destination_z = 0.0f;
    float staff_along = 0.0f;
    float staff_offset_x = 0.0f;
    float staff_offset_y = 0.0f;
    float staff_offset_z = 0.0f;
    float travel_time = 0.2f;
  };

  unsigned int mesh_program_ = 0;
  unsigned int skinned_program_ = 0;
  unsigned int rigid_program_ = 0;
  unsigned int sky_program_ = 0;
  unsigned int raytrace_program_ = 0;
  unsigned int crosshair_program_ = 0;
  unsigned int sand_cloud_program_ = 0;
  unsigned int sand_ribbon_program_ = 0;
  unsigned int vao_ = 0;
  unsigned int sky_vao_ = 0;
  unsigned int sand_cloud_vao_ = 0;
  unsigned int sand_ribbon_vao_ = 0;
  unsigned int arena_vbo_ = 0;
  unsigned int dummy_post_vbo_ = 0;
  unsigned int dummy_body_vbo_ = 0;
  unsigned int sand_particle_vbo_ = 0;
  unsigned int sky_vbo_ = 0;
  unsigned int sand_cloud_vbo_ = 0;
  unsigned int sand_ribbon_vbo_ = 0;
  int arena_vertex_count_ = 0;
  int dummy_post_vertex_count_ = 0;
  int dummy_body_vertex_count_ = 0;
  int sand_particle_vertex_count_ = 0;
  int width_ = 1280;
  int height_ = 720;

  SamuraiAsset samurai_;
  SamuraiAsset pharaoh_;
  SamuraiAsset remote_pharaoh_;
  FbxMeshAsset finger_projectile_;
  bool use_pharaoh_ = false;
  bool pharaoh_loaded_ = false;
  bool remote_pharaoh_loaded_ = false;
  bool samurai_loaded_ = false;
  bool finger_projectile_loaded_ = false;
  std::vector<SandParticle> sand_particles_;
  float sand_emit_accumulator_ = 0.0f;
  float staff_sand_emit_accumulator_ = 0.0f;
  float staff_sand_dismiss_time_ = 0.0f;
  float last_projectile_x_ = 0.0f;
  float last_projectile_y_ = 0.0f;
  float last_projectile_z_ = 0.0f;
  bool had_active_projectile_ = false;
  bool staff_was_swinging_ = false;
  bool ultimate_was_returning_ = false;
  float ultimate_return_emit_accumulator_ = 0.0f;
  float ultimate_rush_time_ = 0.0f;
  float ultimate_rush_emit_accumulator_ = 0.0f;
  std::uint32_t sand_random_state_ = 0x51A7D00Du;

  int mesh_mvp_location_ = -1;
  int mesh_model_location_ = -1;
  int mesh_light_location_ = -1;
  int mesh_color_tint_location_ = -1;

  int sky_camera_pos_location_ = -1;
  int sky_camera_right_location_ = -1;
  int sky_camera_up_location_ = -1;
  int sky_camera_forward_location_ = -1;
  int sky_tan_half_fov_location_ = -1;
  int sky_aspect_location_ = -1;
  int sky_sun_dir_location_ = -1;
  int sky_time_location_ = -1;
  int ray_aspect_location_ = -1;
  int ray_camera_pos_location_ = -1;
  int ray_camera_right_location_ = -1;
  int ray_camera_up_location_ = -1;
  int ray_camera_forward_location_ = -1;
  int ray_time_location_ = -1;
  int crosshair_inv_viewport_location_ = -1;
  int crosshair_scale_location_ = -1;
  int crosshair_color_location_ = -1;
  int crosshair_vertical_offset_location_ = -1;
  int sand_cloud_vp_location_ = -1;
  int sand_cloud_center_location_ = -1;
  int sand_cloud_right_location_ = -1;
  int sand_cloud_up_location_ = -1;
  int sand_cloud_size_location_ = -1;
  int sand_cloud_color_location_ = -1;
  int sand_cloud_opacity_location_ = -1;
  int sand_cloud_seed_location_ = -1;
  int sand_ribbon_vp_location_ = -1;
  int sand_ribbon_color_location_ = -1;
  int sand_ribbon_opacity_location_ = -1;
  int sand_ribbon_time_location_ = -1;

  SceneTransforms compute_scene_transforms(const SceneDrawParams &scene) const;
  Mat4 player_model_matrix(const SceneDrawParams &scene) const;
  void bind_mesh_vbo(unsigned int vbo);
  void draw_lit_mesh(const Mat4 &mvp, const Mat4 &model, unsigned int vbo,
                     int vertex_count, float tint_r = 1.0f,
                     float tint_g = 1.0f, float tint_b = 1.0f);
  void draw_scene_meshes(const Mat4 &projection, const Mat4 &view,
                         const SceneDrawParams &scene, const SceneTransforms &transforms);
  void draw_sky(const OrbitCamera &camera, float aspect, float time);
  void draw_raytrace(const OrbitCamera &camera, float aspect, float time);
  void draw_crosshair(float vertical_ndc);
  void update_sand_trail(const SceneDrawParams &scene);
  void update_staff_summon(const SceneDrawParams &scene);
  void update_ultimate_sand(const SceneDrawParams &scene);
  void draw_sand_trail(const Mat4 &projection, const Mat4 &view,
                       const OrbitCamera &camera);
  void draw_sand_cloud_ribbons(const Mat4 &projection, const Mat4 &view,
                               const OrbitCamera &camera,
                               const SceneDrawParams &scene);
  float next_sand_random();
  void assign_sand_shade(SandParticle &particle);
  unsigned int compile_shader(unsigned int type, const char *source);
  unsigned int link_program(unsigned int vertex_shader, unsigned int fragment_shader);
};

} // namespace engine
