#pragma once

#include "fbx_asset.h"
#include "math.h"

namespace engine {

class PharaohCharacter {
public:
  static constexpr float kSwingDuration = 0.45f;

  bool load();
  void shutdown();
  void update(float delta_time, bool moving, bool swinging);
  void draw(unsigned int program, const Mat4 &projection, const Mat4 &view,
            const Mat4 &player_model);

  float swing_clip_duration() const { return kSwingDuration; }
  bool can_swing() const { return loaded_; }
  bool sword_drawn() const { return true; }
  bool action_overrides_sprint() const { return false; }
  bool weapon_tip_world(const Mat4 &player_model, float &x, float &y, float &z) const;

private:
  FbxMeshAsset body_;
  FbxMeshAsset staff_;
  FbxMeshAsset finger_bullet_;
  bool loaded_ = false;

  Mat4 staff_local_{};
  float swing_progress_ = 0.0f;
  bool swinging_ = false;

};

} // namespace engine
