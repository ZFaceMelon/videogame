#pragma once

#include "math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace engine {

struct SkinnedMeshGpu {
  unsigned int vao = 0;
  unsigned int vbo = 0;
  unsigned int ebo = 0;
  int index_count = 0;
  unsigned int index_type = 0;
  int material_index = -1;
};

struct RigidMeshGpu {
  unsigned int vao = 0;
  unsigned int vbo = 0;
  unsigned int ebo = 0;
  int index_count = 0;
  unsigned int index_type = 0;
  int material_index = -1;
  int node_index = -1;
};

class SamuraiAsset {
public:
  static constexpr int kMaxBones = 33;
  static constexpr float kBlendDuration = 0.2f;

  bool load();
  void shutdown();

  void update(float delta_time, bool moving, bool sprinting, bool swinging, bool dodging,
              bool toggle_equip, float move_forward, float move_right);
  void draw(unsigned int skinned_program, unsigned int rigid_program,
            const Mat4 &projection, const Mat4 &view, const Mat4 &player_model);

  bool sword_drawn() const { return sword_drawn_; }
  bool can_swing() const { return sword_drawn_ && !equip_active_; }
  bool action_overrides_sprint() const { return equip_active_; }
  bool equip_active() const { return equip_active_; }
  bool sword_tip_world(const Mat4 &player_model, float &x, float &y, float &z) const;
  float swing_clip_duration() const { return clip_swing_.duration; }
  void log_animation_catalog() const;

  const std::vector<unsigned int> &textures() const { return textures_; }

private:
  struct NodePose {
    float translation[3]{0.0f, 0.0f, 0.0f};
    Quat rotation{};
    float scale[3]{1.0f, 1.0f, 1.0f};
  };

  struct AnimationClip {
    int animation_index = -1;
    float start_time = 0.0f;
    float end_time = 1.0f;
    float duration = 1.0f;
    int keyframes = 0;
    bool loop = true;
  };

  void *data_ = nullptr;
  std::vector<unsigned char> file_bytes_;
  std::string asset_path_;
  std::vector<NodePose> rest_poses_;
  std::vector<NodePose> current_poses_;
  std::vector<NodePose> previous_poses_;
  std::vector<Mat4> global_matrices_;
  std::vector<Mat4> skin_matrices_;
  std::vector<int> node_by_name_;

  SkinnedMeshGpu samurai_mesh_{};
  std::vector<RigidMeshGpu> rigid_meshes_;

  std::vector<unsigned int> textures_;
  unsigned int fallback_texture_ = 0;
  mutable std::vector<int> bone_uniform_locs_;
  mutable unsigned int cached_bone_program_ = 0;
  std::vector<Mat4> inverse_bind_;

  int samurai_skin_index_ = -1;
  int node_samurai_ = -1;
  int node_sheath_ = -1;
  int node_katana_ = -1;
  int node_sockethand_ = -1;
  int node_socketsheath_ = -1;

  Mat4 model_fixup_{};

  float scale_ = 1.0f;
  float idle_time_ = 0.0f;
  float run_forward_time_ = 0.0f;
  float run_back_time_ = 0.0f;
  float run_left_time_ = 0.0f;
  float run_right_time_ = 0.0f;
  float sprint_time_ = 0.0f;
  float swing_time_ = 0.0f;
  float equip_time_ = 0.0f;
  float blend_time_ = 1.0f;
  bool swing_active_ = false;
  bool equip_active_ = false;
  bool equip_drawing_ = false;
  float equip_swap_time_ = 0.5f;

  enum class LocomotionState {
    Idle,
    RunForward,
    RunBack,
    RunLeft,
    RunRight,
    Sprint,
    Swing,
    Equip
  };
  LocomotionState locomotion_ = LocomotionState::Idle;
  LocomotionState previous_locomotion_ = LocomotionState::Idle;

  AnimationClip clip_idle_{};
  AnimationClip clip_run_forward_{};
  AnimationClip clip_run_back_{};
  AnimationClip clip_run_left_{};
  AnimationClip clip_run_right_{};
  AnimationClip clip_sprint_{};
  AnimationClip clip_swing_{};
  AnimationClip clip_equip_{};

  bool sword_drawn_ = true;

  int find_node(const char *name) const;
  AnimationClip find_clip(const char *name, bool loop) const;
  void cache_rest_poses();
  void sample_animation(int animation_index, float time, std::vector<NodePose> &out) const;
  void update_global_matrices();
  void update_skin_matrices();
  void compute_model_fixup();
  Mat4 node_local_matrix(int node_index, const std::vector<NodePose> &poses) const;
  Mat4 node_global_matrix(int node_index) const;
  Mat4 character_root_matrix() const;
  Mat4 katana_world_matrix() const;
  bool katana_attached_to_hand() const;
  LocomotionState choose_locomotion_state(bool moving, bool sprinting, bool swinging,
                                          bool dodging,
                                          float move_forward, float move_right) const;
  const AnimationClip &clip_for_state(LocomotionState state) const;
  float clip_playback_time(LocomotionState state) const;
  float clip_sample_time(const AnimationClip &clip, float playback_time) const;
  void advance_clip_time(LocomotionState state, float delta_time, bool loop);
  unsigned int texture_for_material(int material_index) const;
  void upload_bone_uniforms(unsigned int program) const;
};

bool create_samurai_programs(unsigned int &skinned_program, unsigned int &rigid_program);
std::string resolve_asset_path(const char *filename);

} // namespace engine
