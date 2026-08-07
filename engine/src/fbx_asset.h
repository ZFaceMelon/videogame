#pragma once

#include "math.h"

#include <string>
#include <vector>

namespace engine {

struct FbxMeshPartGpu {
  unsigned int vao = 0;
  unsigned int vbo = 0;
  unsigned int ebo = 0;
  int index_count = 0;
  unsigned int texture = 0;
  Mat4 local_transform = Mat4::identity();
};

class FbxMeshAsset {
public:
  static constexpr float kDefaultTargetHeight = 1.54f;

  bool load(const char *filename, float target_height = kDefaultTargetHeight);
  void shutdown();
  void draw(unsigned int program, const Mat4 &vp, const Mat4 &model,
            unsigned int fallback_texture) const;

  Mat4 fixup() const { return fixup_; }
  float height() const { return height_; }
  float max_extent_z() const { return max_extent_z_; }

private:
  std::vector<FbxMeshPartGpu> parts_;
  std::vector<unsigned int> textures_;
  Mat4 fixup_ = Mat4::identity();
  float height_ = 1.0f;
  float max_extent_z_ = 0.5f;
  unsigned int fallback_texture_ = 0;

  void compute_fixup(float target_height);
};

std::string resolve_fbx_path(const char *filename);

} // namespace engine
