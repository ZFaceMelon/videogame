#include "fbx_asset.h"

#include "gl_loader.h"
#include "gltf_asset.h"

#include "ufbx.h"
#include "stb_image.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <vector>

namespace engine {
namespace {

constexpr unsigned int kGlArrayBuffer = 0x8892;
constexpr unsigned int kGlElementArrayBuffer = 0x8893;
constexpr unsigned int kGlStaticDraw = 0x88E4;
constexpr unsigned int kGlFloat = 0x1406;
constexpr unsigned int kGlFalse = 0;
constexpr unsigned int kGlUnsignedInt = 0x1405;
constexpr unsigned int kGlTriangles = 0x0004;
constexpr unsigned int kGlTexture2D = 0x0DE1;
constexpr unsigned int kGlRgba = 0x1908;
constexpr unsigned int kGlUnsignedByteType = 0x1401;
constexpr unsigned int kGlLinear = 0x2601;
constexpr unsigned int kGlTextureWrapS = 0x2802;
constexpr unsigned int kGlTextureWrapT = 0x2803;
constexpr unsigned int kGlRepeat = 0x2901;

struct GpuVertex {
  float px, py, pz;
  float nx, ny, nz;
  float u, v;
};

Mat4 mat4_from_ufbx(const ufbx_matrix &m) {
  Mat4 out{};
  out.m[0] = static_cast<float>(m.m00);
  out.m[1] = static_cast<float>(m.m10);
  out.m[2] = static_cast<float>(m.m20);
  out.m[4] = static_cast<float>(m.m01);
  out.m[5] = static_cast<float>(m.m11);
  out.m[6] = static_cast<float>(m.m21);
  out.m[8] = static_cast<float>(m.m02);
  out.m[9] = static_cast<float>(m.m12);
  out.m[10] = static_cast<float>(m.m22);
  out.m[12] = static_cast<float>(m.m03);
  out.m[13] = static_cast<float>(m.m13);
  out.m[14] = static_cast<float>(m.m23);
  out.m[15] = 1.0f;
  return out;
}

void transform_point_mat(const Mat4 &m, float x, float y, float z, float &ox, float &oy,
                         float &oz) {
  ox = m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12];
  oy = m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13];
  oz = m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14];
}

} // namespace

std::string resolve_fbx_path(const char *filename) {
  return resolve_asset_path(filename);
}

bool FbxMeshAsset::load(const char *filename, float target_height) {
  shutdown();

  const std::string path = resolve_fbx_path(filename);
  ufbx_load_opts opts{};
  opts.generate_missing_normals = true;
  opts.target_axes = ufbx_axes_right_handed_y_up;
  opts.target_unit_meters = 1.0f;
  opts.load_external_files = true;

  ufbx_error error{};
  ufbx_scene *scene = ufbx_load_file(path.c_str(), &opts, &error);
  if (!scene) {
    char buffer[512]{};
    ufbx_format_error(buffer, sizeof(buffer), &error);
    std::fprintf(stderr, "Failed to load FBX %s: %s\n", path.c_str(), buffer);
    return false;
  }

  struct Bounds {
    float min_x = 1e9f;
    float min_y = 1e9f;
    float min_z = 1e9f;
    float max_x = -1e9f;
    float max_y = -1e9f;
    float max_z = -1e9f;
    bool valid = false;

    void include(float x, float y, float z) {
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      min_z = std::min(min_z, z);
      max_x = std::max(max_x, x);
      max_y = std::max(max_y, y);
      max_z = std::max(max_z, z);
      valid = true;
    }
  } bounds;

  textures_.assign(scene->materials.count, 0);
  for (size_t mi = 0; mi < scene->materials.count; ++mi) {
    const ufbx_material *material = scene->materials.data[mi];
    const ufbx_material_map &color_map = material->pbr.base_color;
    const ufbx_material_map &legacy_map = material->fbx.diffuse_color;
    const ufbx_texture *source = color_map.texture_enabled && color_map.texture
                                     ? color_map.texture
                                     : (legacy_map.texture_enabled ? legacy_map.texture : nullptr);

    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = nullptr;
    if (source && source->content.data && source->content.size > 0) {
      pixels = stbi_load_from_memory(static_cast<const unsigned char *>(source->content.data),
                                     static_cast<int>(source->content.size), &width, &height,
                                     &channels, 4);
    } else if (source && source->filename.length > 0) {
      const std::string texture_path(source->filename.data, source->filename.length);
      pixels = stbi_load(texture_path.c_str(), &width, &height, &channels, 4);
      if (!pixels) {
        const size_t slash = texture_path.find_last_of("/\\");
        const std::string filename = slash == std::string::npos
                                         ? texture_path
                                         : texture_path.substr(slash + 1);
        const std::string local_path = resolve_asset_path(filename.c_str());
        pixels = stbi_load(local_path.c_str(), &width, &height, &channels, 4);
      }
    }

    unsigned int texture = 0;
    gl::gen_textures(1, &texture);
    gl::bind_texture(kGlTexture2D, texture);
    if (pixels) {
      gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), width, height, 0, kGlRgba,
                      kGlUnsignedByteType, pixels);
      stbi_image_free(pixels);
    } else {
      const ufbx_vec4 color = color_map.has_value ? color_map.value_vec4
                                                  : ufbx_vec4{legacy_map.value_vec3.x,
                                                              legacy_map.value_vec3.y,
                                                              legacy_map.value_vec3.z, 1.0};
      const unsigned char rgba[] = {
          static_cast<unsigned char>(std::clamp(color.x, 0.0, 1.0) * 255.0),
          static_cast<unsigned char>(std::clamp(color.y, 0.0, 1.0) * 255.0),
          static_cast<unsigned char>(std::clamp(color.z, 0.0, 1.0) * 255.0), 255};
      gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), 1, 1, 0, kGlRgba,
                      kGlUnsignedByteType, rgba);
    }
    gl::texParameteri(kGlTexture2D, kGlTextureWrapS, kGlRepeat);
    gl::texParameteri(kGlTexture2D, kGlTextureWrapT, kGlRepeat);
    gl::texParameteri(kGlTexture2D, 0x2801, kGlLinear);
    gl::texParameteri(kGlTexture2D, 0x2800, kGlLinear);
    textures_[mi] = texture;
  }

  for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
    ufbx_node *node = scene->nodes.data[ni];
    if (!node->mesh) {
      continue;
    }

    ufbx_mesh *mesh = node->mesh;
    const Mat4 node_matrix = mat4_from_ufbx(node->geometry_to_world);

    const size_t material_count = std::max<size_t>(node->materials.count, 1);
    std::vector<std::vector<GpuVertex>> material_vertices(material_count);

    std::vector<uint32_t> tri_indices(256);
    for (size_t fi = 0; fi < mesh->faces.count; ++fi) {
      const ufbx_face face = mesh->faces.data[fi];
      const size_t material_index = mesh->face_material.count > fi
                                        ? std::min<size_t>(mesh->face_material.data[fi], material_count - 1)
                                        : 0;
      std::vector<GpuVertex> &vertices = material_vertices[material_index];
      if (tri_indices.size() < face.num_indices * 2) {
        tri_indices.resize(face.num_indices * 2);
      }
      const uint32_t num_tris =
          ufbx_triangulate_face(tri_indices.data(), tri_indices.size(), mesh, face);
      for (uint32_t ti = 0; ti < num_tris * 3; ++ti) {
        const uint32_t ix = tri_indices[ti];
        const ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
        const ufbx_vec3 norm = mesh->vertex_normal.exists
                                   ? ufbx_get_vertex_vec3(&mesh->vertex_normal, ix)
                                   : ufbx_vec3{0.0, 1.0, 0.0};
        const ufbx_vec2 uv = mesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&mesh->vertex_uv, ix)
                                                    : ufbx_vec2{0.0, 0.0};

        float wx = static_cast<float>(pos.x);
        float wy = static_cast<float>(pos.y);
        float wz = static_cast<float>(pos.z);
        transform_point_mat(node_matrix, wx, wy, wz, wx, wy, wz);
        bounds.include(wx, wy, wz);

        GpuVertex vert{};
        vert.px = wx;
        vert.py = wy;
        vert.pz = wz;
        vert.nx = static_cast<float>(norm.x);
        vert.ny = static_cast<float>(norm.y);
        vert.nz = static_cast<float>(norm.z);
        vert.u = static_cast<float>(uv.x);
        // FBX/Blender UVs use a bottom-left image origin while stb_image uploads
        // rows top-first. Flip V here so the color atlas lands on the intended
        // parts of the character instead of sampling unrelated colored regions.
        vert.v = 1.0f - static_cast<float>(uv.y);
        vertices.push_back(vert);
      }
    }

    for (size_t material_index = 0; material_index < material_vertices.size(); ++material_index) {
      std::vector<GpuVertex> &vertices = material_vertices[material_index];
      if (vertices.empty()) continue;
      std::vector<unsigned int> indices(vertices.size());
      for (size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<unsigned int>(i);

      FbxMeshPartGpu part{};
      part.local_transform = Mat4::identity();
      part.index_count = static_cast<int>(indices.size());
      if (material_index < node->materials.count) {
        const size_t scene_material = node->materials.data[material_index]->typed_id;
        if (scene_material < textures_.size()) part.texture = textures_[scene_material];
      }

    gl::gen_buffers(1, &part.vbo);
    gl::gen_buffers(1, &part.ebo);
    part.vao = gl::create_vertex_array();
    gl::bind_vertex_array(part.vao);
    gl::bind_buffer(kGlArrayBuffer, part.vbo);
    gl::buffer_data(kGlArrayBuffer, static_cast<long long>(vertices.size() * sizeof(GpuVertex)),
                    vertices.data(), kGlStaticDraw);
    gl::bind_buffer(kGlElementArrayBuffer, part.ebo);
    gl::buffer_data(kGlElementArrayBuffer,
                    static_cast<long long>(indices.size() * sizeof(unsigned int)),
                    indices.data(), kGlStaticDraw);

    constexpr int stride = static_cast<int>(sizeof(GpuVertex));
    gl::vertex_attrib_pointer(0, 3, kGlFloat, kGlFalse, stride, nullptr);
    gl::enable_vertex_attrib_array(0);
    gl::vertex_attrib_pointer(1, 3, kGlFloat, kGlFalse, stride,
                              reinterpret_cast<const void *>(offsetof(GpuVertex, nx)));
    gl::enable_vertex_attrib_array(1);
    gl::vertex_attrib_pointer(2, 2, kGlFloat, kGlFalse, stride,
                              reinterpret_cast<const void *>(offsetof(GpuVertex, u)));
    gl::enable_vertex_attrib_array(2);

      parts_.push_back(part);
    }
  }

  ufbx_free_scene(scene);

  if (parts_.empty()) {
    std::fprintf(stderr, "No mesh geometry found in FBX: %s\n", path.c_str());
    return false;
  }

  if (bounds.valid) {
    height_ = bounds.max_y - bounds.min_y;
    max_extent_z_ = std::max(bounds.max_z - bounds.min_z, 0.1f);
    if (target_height > 0.0f && height_ > 1e-4f) {
      const float scale = target_height / height_;
      const float center_x = (bounds.min_x + bounds.max_x) * 0.5f;
      const float center_z = (bounds.min_z + bounds.max_z) * 0.5f;
      fixup_ = Mat4::scale(scale, scale, scale) *
               Mat4::translate(-center_x, -bounds.min_y, -center_z);
      height_ = target_height;
      max_extent_z_ *= scale;
    } else {
      fixup_ = Mat4::translate(0.0f, -bounds.min_y, 0.0f);
    }
  }

  gl::gen_textures(1, &fallback_texture_);
  gl::bind_texture(kGlTexture2D, fallback_texture_);
  const unsigned char white[] = {220, 210, 190, 255};
  gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), 1, 1, 0, kGlRgba,
                  kGlUnsignedByteType, white);
  gl::texParameteri(kGlTexture2D, 0x2801, kGlLinear);
  gl::texParameteri(kGlTexture2D, 0x2800, kGlLinear);

  std::fprintf(stderr, "Loaded FBX %s (%zu parts, height %.3f)\n", path.c_str(), parts_.size(),
               height_);
  return true;
}

void FbxMeshAsset::shutdown() {
  for (FbxMeshPartGpu &part : parts_) {
    if (part.vbo != 0) {
      gl::delete_buffer(1, &part.vbo);
    }
    if (part.ebo != 0) {
      gl::delete_buffer(1, &part.ebo);
    }
    part = {};
  }
  parts_.clear();
  for (unsigned int texture : textures_) {
    if (texture != 0) gl::delete_textures(1, &texture);
  }
  textures_.clear();
  if (fallback_texture_ != 0) {
    gl::delete_textures(1, &fallback_texture_);
    fallback_texture_ = 0;
  }
  fixup_ = Mat4::identity();
  height_ = 1.0f;
  max_extent_z_ = 0.5f;
}

void FbxMeshAsset::draw(unsigned int program, const Mat4 &vp, const Mat4 &model,
                        unsigned int fallback_texture) const {
  const int mvp_loc = gl::get_uniform_location(program, "uMVP");
  const int model_loc = gl::get_uniform_location(program, "uModel");
  const int tex_loc = gl::get_uniform_location(program, "uDiffuse");
  const int light_loc = gl::get_uniform_location(program, "uLightDir");

  constexpr float kSunDirX = 0.551f;
  constexpr float kSunDirY = 0.701f;
  constexpr float kSunDirZ = -0.451f;
  gl::uniform3f(light_loc, kSunDirX, kSunDirY, kSunDirZ);

  for (const FbxMeshPartGpu &part : parts_) {
    const unsigned int tex = part.texture != 0
                                 ? part.texture
                                 : (fallback_texture != 0 ? fallback_texture : fallback_texture_);
    const Mat4 world = model * fixup_ * part.local_transform;
    const Mat4 mvp = vp * world;
    gl::uniform_matrix4fv(mvp_loc, 1, kGlFalse, mvp.m);
    gl::uniform_matrix4fv(model_loc, 1, kGlFalse, world.m);
    gl::active_texture(0x84C0);
    gl::bind_texture(kGlTexture2D, tex);
    gl::uniform1i(tex_loc, 0);
    gl::bind_vertex_array(part.vao);
    gl::draw_elements(kGlTriangles, part.index_count, kGlUnsignedInt, nullptr);
  }
}

} // namespace engine
