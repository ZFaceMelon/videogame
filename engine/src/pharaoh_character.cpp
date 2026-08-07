#include "pharaoh_character.h"

#include "gl_loader.h"

#include <cmath>
#include <cstdio>

namespace engine {

bool PharaohCharacter::load() {
  shutdown();

  if (!body_.load("pharoh_person.fbx", FbxMeshAsset::kDefaultTargetHeight)) {
    return false;
  }
  if (!staff_.load("staff.fbx", -1.0f)) {
    std::fprintf(stderr, "Warning: staff.fbx failed to load\n");
  }
  if (!finger_bullet_.load("finger_bullet.fbx", -1.0f)) {
    std::fprintf(stderr, "Warning: finger_bullet.fbx failed to load\n");
  }

  const float body_h = body_.height();
  const float staff_scale = body_h > 0.0f ? (body_h * 0.85f) / std::max(staff_.height(), 0.01f) : 1.0f;
  staff_local_ = Mat4::translate(0.28f, body_h * 0.55f, 0.05f) * Mat4::rotate_x(-0.35f) *
                 Mat4::rotate_z(-0.15f) * Mat4::scale(staff_scale, staff_scale, staff_scale);

  loaded_ = true;
  std::fprintf(stderr, "Pharaoh character ready (body + staff + finger bullet)\n");
  return true;
}

void PharaohCharacter::shutdown() {
  body_.shutdown();
  staff_.shutdown();
  finger_bullet_.shutdown();
  loaded_ = false;
  swing_progress_ = 0.0f;
  swinging_ = false;
}

void PharaohCharacter::update(float delta_time, bool moving, bool swinging) {
  (void)moving;
  if (swinging && !swinging_) {
    swing_progress_ = 0.0f;
  }
  swinging_ = swinging;
  if (swinging_) {
    swing_progress_ = std::min(1.0f, swing_progress_ + delta_time / kSwingDuration);
  } else {
    swing_progress_ = 0.0f;
  }
}

void PharaohCharacter::draw(unsigned int program, const Mat4 &projection, const Mat4 &view,
                            const Mat4 &player_model) {
  if (!loaded_) {
    return;
  }

  const Mat4 vp = projection * view;
  const Mat4 body_model = player_model;

  body_.draw(program, vp, body_model, 0);

  if (staff_.height() > 0.0f) {
    staff_.draw(program, vp, body_model * staff_local_, 0);
  }

  if (swinging_ && finger_bullet_.height() > 0.0f) {
    const float t = std::sin(swing_progress_ * 3.1415926535f);
    const float body_h = body_.height();
    const float bullet_scale = body_h * 0.08f / std::max(finger_bullet_.height(), 0.01f);
    Mat4 bullet_local = Mat4::translate(0.32f, body_h * 0.62f, 0.15f + t * 1.4f) *
                        Mat4::rotate_x(-0.2f) * Mat4::scale(bullet_scale, bullet_scale, bullet_scale);
    finger_bullet_.draw(program, vp, body_model * bullet_local, 0);
  }
}

bool PharaohCharacter::weapon_tip_world(const Mat4 &player_model, float &x, float &y,
                                        float &z) const {
  if (!loaded_) {
    return false;
  }

  const float body_h = body_.height();
  const float staff_len = staff_.height() > 0.0f ? staff_.max_extent_z() * (body_h * 0.85f / std::max(staff_.height(), 0.01f))
                                                 : body_h * 0.5f;

  x = 0.28f;
  y = body_h * 0.55f;
  z = staff_len;

  if (swinging_ && finger_bullet_.height() > 0.0f) {
    const float t = std::sin(swing_progress_ * 3.1415926535f);
    x = 0.32f;
    y = body_h * 0.62f;
    z = 0.15f + t * 1.4f + body_h * 0.08f;
  }

  // The attachment coordinates above are already expressed in normalized player-space.
  // Applying the body's import fixup again would scale them a second time and put the
  // combat hit point far away from the weapon that is actually drawn.
  transform_point(player_model, x, y, z);
  return true;
}

} // namespace engine
