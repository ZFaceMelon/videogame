#include "gltf_asset.h"

#include "gl_loader.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace engine {
namespace {

constexpr unsigned int kGlArrayBuffer = 0x8892;
constexpr unsigned int kGlElementArrayBuffer = 0x8893;
constexpr unsigned int kGlStaticDraw = 0x88E4;
constexpr unsigned int kGlFloat = 0x1406;
constexpr unsigned int kGlFalse = 0;
constexpr unsigned int kGlUnsignedShort = 0x1403;
constexpr unsigned int kGlUnsignedInt = 0x1405;
constexpr unsigned int kGlUnsignedByte = 0x1401;
constexpr unsigned int kGlTriangles = 0x0004;
constexpr unsigned int kGlTexture2D = 0x0DE1;
constexpr unsigned int kGlTexture0 = 0x84C0;
constexpr unsigned int kGlRgba = 0x1908;
constexpr unsigned int kGlRgb = 0x1907;
constexpr unsigned int kGlUnsignedByteType = 0x1401;
constexpr unsigned int kGlLinear = 0x2601;
constexpr unsigned int kGlTextureWrapS = 0x2802;
constexpr unsigned int kGlTextureWrapT = 0x2803;
constexpr unsigned int kGlRepeat = 0x2901;
constexpr unsigned int kGlVertexShader = 0x8B31;
constexpr unsigned int kGlFragmentShader = 0x8B30;
constexpr unsigned int kGlCompileStatus = 0x8B81;
constexpr unsigned int kGlLinkStatus = 0x8B82;
constexpr unsigned int kGlBlend = 0x0BE2;
constexpr unsigned int kGlSrcAlpha = 0x0302;
constexpr unsigned int kGlOneMinusSrcAlpha = 0x0303;

constexpr float kSunDirX = 0.551f;
constexpr float kSunDirY = 0.701f;
constexpr float kSunDirZ = -0.451f;

constexpr const char *kAssetFile = "samuraianimatedv1.glb";
constexpr float kTargetHeight = 1.54f;

Mat4 align_direction_to_y(float x, float y, float z) {
  const float length = std::sqrt(x * x + y * y + z * z);
  x /= length;
  y /= length;
  z /= length;
  // Rodrigues rotation from the supplied direction to +Y.
  float ax = -z;
  float ay = 0.0f;
  float az = x;
  const float s = std::sqrt(ax * ax + az * az);
  const float c = y;
  if (s < 1e-5f) {
    return c >= 0.0f ? Mat4::identity() : Mat4::rotate_x(3.1415926535f);
  }
  ax /= s;
  az /= s;
  const float one_minus_c = 1.0f - c;
  Mat4 out = Mat4::identity();
  out.m[0] = c + ax * ax * one_minus_c;
  out.m[4] = ax * ay * one_minus_c - az * s;
  out.m[8] = ax * az * one_minus_c + ay * s;
  out.m[1] = ay * ax * one_minus_c + az * s;
  out.m[5] = c + ay * ay * one_minus_c;
  out.m[9] = ay * az * one_minus_c - ax * s;
  out.m[2] = az * ax * one_minus_c - ay * s;
  out.m[6] = az * ay * one_minus_c + ax * s;
  out.m[10] = c + az * az * one_minus_c;
  return out;
}

Mat4 align_y_to_direction(float x, float y, float z) {
  return mat4_inverse(align_direction_to_y(x, y, z));
}

constexpr const char *kSkinnedVertexShader = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv;
layout (location = 3) in ivec4 aJoints;
layout (location = 4) in vec4 aWeights;
uniform mat4 uVP;
uniform mat4 uModel;
uniform mat4 uBones[96];
out vec2 vUv;
out vec3 vNormal;
out vec3 vWorldPos;
void main() {
    mat4 skin =
        uBones[aJoints.x] * aWeights.x +
        uBones[aJoints.y] * aWeights.y +
        uBones[aJoints.z] * aWeights.z +
        uBones[aJoints.w] * aWeights.w;
    vec4 skinned = skin * vec4(aPos, 1.0);
    vec4 worldPos = uModel * skinned;
    gl_Position = uVP * worldPos;
    vWorldPos = worldPos.xyz;
    vNormal = normalize(mat3(uModel) * mat3(skin) * aNormal);
    vUv = aUv;
}
)";

constexpr const char *kRigidVertexShader = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec2 vUv;
out vec3 vNormal;
out vec3 vWorldPos;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uMVP * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    vUv = aUv;
}
)";

constexpr const char *kTexturedFragmentShader = R"(#version 330 core
in vec2 vUv;
in vec3 vNormal;
in vec3 vWorldPos;
uniform sampler2D uDiffuse;
uniform vec3 uLightDir;
uniform float uOpacity;
uniform float uDissolve;
uniform vec3 uDissolveCenter;
uniform float uRevealProgress;
uniform float uRevealBaseY;
out vec4 FragColor;
float dissolveNoise(vec3 p) {
    vec3 cell = floor(p * 48.0);
    return fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}
void main() {
    if (uRevealProgress < 0.999) {
        float revealEdge = uRevealBaseY + uRevealProgress * 1.62;
        float brokenEdge = (dissolveNoise(vWorldPos * 0.85) - 0.5) * 0.08;
        if (vWorldPos.y > revealEdge + brokenEdge) {
            discard;
        }
    }
    if (uDissolve > 0.0) {
        float radius = length(vWorldPos - uDissolveCenter);
        float inwardEdge = 1.30 - uDissolve * 1.36;
        float brokenEdge = (dissolveNoise(vWorldPos) - 0.5) * 0.04;
        if (radius + brokenEdge > inwardEdge) {
            discard;
        }
    }
    vec4 sampled = texture(uDiffuse, vUv);
    vec3 albedo = sampled.rgb;
    vec3 n = normalize(vNormal);
    vec3 l = normalize(uLightDir);
    float ndl = max(dot(n, l), 0.0);
    float skyFill = max(dot(n, vec3(0.0, 1.0, 0.0)), 0.0);
    vec3 h = normalize(l + vec3(0.0, 0.0, 1.0));
    float spec = pow(max(dot(n, h), 0.0), 32.0);
    vec3 lit = albedo * (0.58 + 0.34 * ndl + 0.08 * skyFill) + vec3(0.12) * spec;
    FragColor = vec4(lit, sampled.a * uOpacity);
}
)";

struct GpuVertex {
  float px, py, pz;
  float nx, ny, nz;
  float u, v;
  std::uint8_t joints[4]{};
  float weights[4]{1.0f, 0.0f, 0.0f, 0.0f};
};

bool file_exists(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return file.good();
}

std::vector<unsigned char> read_binary_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }
  const std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    return {};
  }
  return buffer;
}

unsigned int compile_shader(unsigned int type, const char *source) {
  const unsigned int shader = gl::create_shader(type);
  gl::shader_source(shader, 1, &source, nullptr);
  gl::compile_shader(shader);
  int success = 0;
  gl::get_shader_iv(shader, kGlCompileStatus, &success);
  if (!success) {
    std::vector<char> log(2048);
    gl::get_shader_info_log(shader, static_cast<int>(log.size()), nullptr, log.data());
    std::fprintf(stderr, "Shader compile error:\n%s\n", log.data());
    gl::delete_shader(shader);
    return 0;
  }
  return shader;
}

unsigned int link_program(unsigned int vs, unsigned int fs) {
  const unsigned int program = gl::create_program();
  gl::attach_shader(program, vs);
  gl::attach_shader(program, fs);
  gl::link_program(program);
  int success = 0;
  gl::get_program_iv(program, kGlLinkStatus, &success);
  if (!success) {
    std::vector<char> log(2048);
    gl::get_program_info_log(program, static_cast<int>(log.size()), nullptr, log.data());
    std::fprintf(stderr, "Program link error:\n%s\n", log.data());
    return 0;
  }
  return program;
}

float component_length(const cgltf_accessor *accessor) {
  switch (accessor->type) {
  case cgltf_type_vec3:
    return 3.0f;
  case cgltf_type_vec4:
    return 4.0f;
  case cgltf_type_scalar:
    return 1.0f;
  default:
    return 1.0f;
  }
}

float sample_animation_scalar(const cgltf_accessor *time_accessor,
                              const cgltf_accessor *value_accessor, float time,
                              int components) {
  if (time_accessor->count == 0 || value_accessor->count == 0) {
    return 0.0f;
  }

  cgltf_size key_index = 0;
  for (cgltf_size i = 0; i < time_accessor->count; ++i) {
  float key_time = 0.0f;
    cgltf_accessor_read_float(time_accessor, i, &key_time, 1);
    if (key_time <= time) {
      key_index = i;
    }
  }

  float t0 = 0.0f;
  float t1 = 0.0f;
  cgltf_accessor_read_float(time_accessor, key_index, &t0, 1);
  const cgltf_size next_index =
      std::min(key_index + 1, static_cast<cgltf_size>(time_accessor->count - 1));
  cgltf_accessor_read_float(time_accessor, next_index, &t1, 1);

  float alpha = 0.0f;
  if (t1 > t0) {
    alpha = (time - t0) / (t1 - t0);
    alpha = std::clamp(alpha, 0.0f, 1.0f);
  }

  std::vector<float> v0(static_cast<std::size_t>(components));
  std::vector<float> v1(static_cast<std::size_t>(components));
  cgltf_accessor_read_float(value_accessor, key_index, v0.data(), components);
  cgltf_accessor_read_float(value_accessor, next_index, v1.data(), components);

  std::vector<float> out(static_cast<std::size_t>(components));
  for (int i = 0; i < components; ++i) {
    out[static_cast<std::size_t>(i)] = v0[static_cast<std::size_t>(i)] +
                                       alpha * (v1[static_cast<std::size_t>(i)] -
                                                v0[static_cast<std::size_t>(i)]);
  }

  if (components == 1) {
    return out[0];
  }
  return 0.0f;
}

void skin_vertex_position(const Mat4 *bones, const std::uint8_t joints[4],
                          const float weights[4], float x, float y, float z, float &out_x,
                          float &out_y, float &out_z) {
  out_x = 0.0f;
  out_y = 0.0f;
  out_z = 0.0f;
  for (int i = 0; i < 4; ++i) {
    if (weights[i] < 1e-6f) {
      continue;
    }
    float sx = x;
    float sy = y;
    float sz = z;
    transform_point(bones[joints[i]], sx, sy, sz);
    out_x += weights[i] * sx;
    out_y += weights[i] * sy;
    out_z += weights[i] * sz;
  }
}

cgltf_size find_key_index(const cgltf_accessor *times, float time) {
  cgltf_size key_index = 0;
  for (cgltf_size i = 0; i < times->count; ++i) {
    float key_time = 0.0f;
    if (!cgltf_accessor_read_float(times, i, &key_time, 1)) {
      continue;
    }
    if (key_time <= time) {
      key_index = i;
    }
  }
  return key_index;
}

struct ChannelTimeSpan {
  cgltf_size start_index = 0;
  cgltf_size end_index = 0;
  float start_time = 0.0f;
  float end_time = 0.0f;
};

ChannelTimeSpan find_channel_time_span(const cgltf_accessor *times) {
  ChannelTimeSpan span{};
  if (!times || times->count == 0) {
    return span;
  }
  span.end_index = times->count - 1;
  span.start_time = 1e9f;
  span.end_time = -1e9f;
  for (cgltf_size i = 0; i < times->count; ++i) {
    float key_time = 0.0f;
    if (!cgltf_accessor_read_float(times, i, &key_time, 1)) {
      continue;
    }
    if (key_time < span.start_time) {
      span.start_time = key_time;
      span.start_index = i;
    }
    if (key_time > span.end_time) {
      span.end_time = key_time;
      span.end_index = i;
    }
  }
  if (span.end_time < span.start_time) {
    span.start_time = 0.0f;
    span.end_time = 0.0f;
    span.start_index = 0;
    span.end_index = 0;
  }
  return span;
}

cgltf_size clamp_key_index(cgltf_size key_index, const cgltf_accessor *times,
                            const cgltf_accessor *values) {
  const cgltf_size max_index =
      std::min(times->count, values->count) > 0 ? std::min(times->count, values->count) - 1 : 0;
  return std::min(key_index, max_index);
}

float sample_channel_alpha(const cgltf_accessor *times, cgltf_size key_index,
                           cgltf_size next_index, float time) {
  float t0 = 0.0f;
  float t1 = 0.0f;
  if (!cgltf_accessor_read_float(times, key_index, &t0, 1)) {
    return 0.0f;
  }
  if (!cgltf_accessor_read_float(times, next_index, &t1, 1)) {
    return 0.0f;
  }
  if (t1 <= t0) {
    return 0.0f;
  }
  return std::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f);
}

} // namespace

std::string resolve_asset_path(const char *filename) {
  const std::vector<std::string> candidates = {
#ifdef ASSETS_DIR
      std::string(ASSETS_DIR) + "/" + filename,
#endif
      std::string("assets/") + filename,
      std::string("../assets/") + filename,
      std::string("../../assets/") + filename,
  };
  for (const std::string &path : candidates) {
    if (file_exists(path)) {
      return path;
    }
  }
  return std::string("assets/") + filename;
}

bool create_samurai_programs(unsigned int &skinned_program, unsigned int &rigid_program) {
  const unsigned int vs_skin = compile_shader(kGlVertexShader, kSkinnedVertexShader);
  const unsigned int vs_rigid = compile_shader(kGlVertexShader, kRigidVertexShader);
  const unsigned int fs = compile_shader(kGlFragmentShader, kTexturedFragmentShader);
  if (!vs_skin || !vs_rigid || !fs) {
    return false;
  }
  skinned_program = link_program(vs_skin, fs);
  rigid_program = link_program(vs_rigid, fs);
  gl::delete_shader(vs_skin);
  gl::delete_shader(vs_rigid);
  gl::delete_shader(fs);
  return skinned_program != 0 && rigid_program != 0;
}

int SamuraiAsset::find_node(const char *name) const {
  const auto *data = static_cast<const cgltf_data *>(data_);
  if (!data) {
    return -1;
  }
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    if (data->nodes[i].name && std::strcmp(data->nodes[i].name, name) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

SamuraiAsset::AnimationClip SamuraiAsset::find_clip(const char *name, bool loop) const {
  AnimationClip clip{};
  clip.loop = loop;
  const auto *data = static_cast<const cgltf_data *>(data_);
  if (!data) {
    return clip;
  }
  for (cgltf_size i = 0; i < data->animations_count; ++i) {
    if (data->animations[i].name && std::strcmp(data->animations[i].name, name) == 0) {
      clip.animation_index = static_cast<int>(i);
      float start_time = 1e9f;
      float end_time = -1e9f;
      for (cgltf_size c = 0; c < data->animations[i].channels_count; ++c) {
        const cgltf_animation_channel &channel = data->animations[i].channels[c];
        const cgltf_accessor *times = channel.sampler->input;
        if (!times || times->count == 0) {
          continue;
        }
        clip.keyframes = std::max(clip.keyframes, static_cast<int>(times->count));
        for (cgltf_size k = 0; k < times->count; ++k) {
          float key_time = 0.0f;
          if (!cgltf_accessor_read_float(times, k, &key_time, 1)) {
            continue;
          }
          start_time = std::min(start_time, key_time);
          end_time = std::max(end_time, key_time);
        }
      }
      if (end_time > start_time) {
        clip.start_time = start_time;
        clip.end_time = end_time;
        clip.duration = end_time - start_time;
      } else {
        clip.start_time = 0.0f;
        clip.end_time = 0.001f;
        clip.duration = 0.001f;
      }
      return clip;
    }
  }
  std::fprintf(stderr, "Animation not found: %s\n", name);
  return clip;
}

void SamuraiAsset::log_animation_catalog() const {
  const auto *data = static_cast<const cgltf_data *>(data_);
  if (!data) {
    return;
  }
  std::fprintf(stderr, "Animation catalog (%zu clips, glTF times are in seconds):\n",
               data->animations_count);
  for (cgltf_size i = 0; i < data->animations_count; ++i) {
    AnimationClip clip = find_clip(data->animations[i].name, true);
    const float fps =
        clip.keyframes > 1 ? static_cast<float>(clip.keyframes - 1) / clip.duration : 0.0f;
    std::fprintf(stderr, "  %-22s keys=%d  %.4fs -> %.4fs  (%.4fs, ~%.1f fps)\n",
                 data->animations[i].name ? data->animations[i].name : "(unnamed)",
                 clip.keyframes, clip.start_time, clip.end_time, clip.duration, fps);
  }
}

float SamuraiAsset::clip_sample_time(const AnimationClip &clip, float playback_time) const {
  float local = playback_time;
  if (clip.loop) {
    local = std::fmod(local, clip.duration);
    if (local < 0.0f) {
      local += clip.duration;
    }
  } else {
    local = std::clamp(local, 0.0f, clip.duration);
  }
  return clip.start_time + local;
}

float SamuraiAsset::clip_playback_time(LocomotionState state) const {
  switch (state) {
  case LocomotionState::RunForward:
    return run_forward_time_;
  case LocomotionState::RunBack:
    return run_back_time_;
  case LocomotionState::RunLeft:
    return run_left_time_;
  case LocomotionState::RunRight:
    return run_right_time_;
  case LocomotionState::Sprint:
    return sprint_time_;
  case LocomotionState::Jump:
    return jump_time_;
  case LocomotionState::Fly:
    return fly_time_;
  case LocomotionState::Swing:
    return swing_time_;
  case LocomotionState::Shoot:
    return shoot_time_;
  case LocomotionState::Ultimate:
    return ultimate_time_;
  case LocomotionState::Equip:
    return equip_time_;
  case LocomotionState::Idle:
  default:
    return idle_time_;
  }
}

const SamuraiAsset::AnimationClip &SamuraiAsset::clip_for_state(LocomotionState state) const {
  switch (state) {
  case LocomotionState::RunForward:
    return clip_run_forward_;
  case LocomotionState::RunBack:
    return clip_run_back_;
  case LocomotionState::RunLeft:
    return clip_run_left_;
  case LocomotionState::RunRight:
    return clip_run_right_;
  case LocomotionState::Sprint:
    return clip_sprint_;
  case LocomotionState::Jump:
    return clip_jump_;
  case LocomotionState::Fly:
    return clip_fly_;
  case LocomotionState::Swing:
    return clip_swing_;
  case LocomotionState::Shoot:
    return clip_shoot_;
  case LocomotionState::Ultimate:
    return clip_ultimate_;
  case LocomotionState::Equip:
    return clip_equip_;
  case LocomotionState::Idle:
  default:
    return clip_idle_;
  }
}

SamuraiAsset::LocomotionState SamuraiAsset::choose_locomotion_state(
    bool moving, bool sprinting, bool airborne, bool flying, bool swinging,
    bool shooting, bool ultimate, bool dodging,
    float move_forward, float move_right) const {
  if (ultimate) {
    return LocomotionState::Ultimate;
  }
  if (swinging) {
    return LocomotionState::Swing;
  }
  if (shooting) {
    return LocomotionState::Shoot;
  }
  if (equip_active_) {
    return LocomotionState::Equip;
  }
  if (flying) {
    return LocomotionState::Fly;
  }
  if (airborne) {
    return LocomotionState::Jump;
  }
  if (!moving) {
    return LocomotionState::Idle;
  }
  if (sprinting) {
    return LocomotionState::Sprint;
  }
  if (std::abs(move_forward) >= std::abs(move_right)) {
    return move_forward < 0.0f ? LocomotionState::RunBack : LocomotionState::RunForward;
  }
  return move_right > 0.0f ? LocomotionState::RunRight : LocomotionState::RunLeft;
}

void SamuraiAsset::advance_clip_time(LocomotionState state, float delta_time, bool loop) {
  float *time = &idle_time_;
  const AnimationClip *clip = &clip_idle_;
  float playback_rate = 1.0f;
  switch (state) {
  case LocomotionState::RunForward:
    time = &run_forward_time_;
    clip = &clip_run_forward_;
    playback_rate = 1.70f;
    break;
  case LocomotionState::RunBack:
    time = &run_back_time_;
    clip = &clip_run_back_;
    playback_rate = 1.70f;
    break;
  case LocomotionState::RunLeft:
    time = &run_left_time_;
    clip = &clip_run_left_;
    playback_rate = 1.70f;
    break;
  case LocomotionState::RunRight:
    time = &run_right_time_;
    clip = &clip_run_right_;
    playback_rate = 1.70f;
    break;
  case LocomotionState::Sprint:
    time = &sprint_time_;
    clip = &clip_sprint_;
    playback_rate = 1.45f;
    break;
  case LocomotionState::Jump:
    time = &jump_time_;
    clip = &clip_jump_;
    playback_rate = 1.65f;
    break;
  case LocomotionState::Fly:
    time = &fly_time_;
    clip = &clip_fly_;
    playback_rate = 1.0f;
    break;
  case LocomotionState::Swing:
    time = &swing_time_;
    clip = &clip_swing_;
    break;
  case LocomotionState::Shoot:
    time = &shoot_time_;
    clip = &clip_shoot_;
    break;
  case LocomotionState::Ultimate:
    time = &ultimate_time_;
    clip = &clip_ultimate_;
    break;
  case LocomotionState::Equip:
    time = &equip_time_;
    clip = &clip_equip_;
    break;
  case LocomotionState::Idle:
  default:
    break;
  }

  *time += delta_time * playback_rate;
  if (loop) {
    const float period = std::max(clip->duration, 0.001f);
    *time = std::fmod(*time, period);
    if (*time < 0.0f) {
      *time += period;
    }
  } else {
    *time = std::min(*time, clip->duration);
  }
}

void SamuraiAsset::cache_rest_poses() {
  const auto *data = static_cast<const cgltf_data *>(data_);
  rest_poses_.resize(data->nodes_count);
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    const cgltf_node &node = data->nodes[i];
    NodePose &pose = rest_poses_[i];
    if (node.has_translation) {
      std::memcpy(pose.translation, node.translation, sizeof(float) * 3);
    }
    if (node.has_rotation) {
      pose.rotation = Quat{node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]};
    }
    if (node.has_scale) {
      std::memcpy(pose.scale, node.scale, sizeof(float) * 3);
    }
  }
  current_poses_ = rest_poses_;
  previous_poses_ = rest_poses_;
  global_matrices_.assign(data->nodes_count, Mat4::identity());
}

Mat4 SamuraiAsset::node_local_matrix(int node_index, const std::vector<NodePose> &poses) const {
  const NodePose &pose = poses[static_cast<std::size_t>(node_index)];
  return mat4_from_trs(pose.translation, pose.rotation, pose.scale);
}

void SamuraiAsset::sample_animation(int animation_index, float time,
                                    std::vector<NodePose> &out) const {
  const auto *data = static_cast<const cgltf_data *>(data_);
  out = rest_poses_;
  if (animation_index < 0) {
    return;
  }

  const cgltf_animation &animation = data->animations[animation_index];
  for (cgltf_size c = 0; c < animation.channels_count; ++c) {
    const cgltf_animation_channel &channel = animation.channels[c];
    if (!channel.target_node || !channel.sampler) {
      continue;
    }
    const cgltf_animation_sampler *sampler = channel.sampler;
    const cgltf_accessor *times = sampler->input;
    const cgltf_accessor *values = sampler->output;
    if (!times || !values || times->count == 0 || values->count == 0) {
      continue;
    }

    const int node_index = static_cast<int>(channel.target_node - data->nodes);
    if (node_index < 0 || node_index >= static_cast<int>(out.size())) {
      continue;
    }
    NodePose &pose = out[static_cast<std::size_t>(node_index)];
    const ChannelTimeSpan span = find_channel_time_span(times);
    const cgltf_size max_key_index = clamp_key_index(times->count - 1, times, values);

    if (channel.target_path == cgltf_animation_path_type_translation) {
      if (time <= span.start_time) {
        cgltf_accessor_read_float(values, span.start_index, pose.translation, 3);
        continue;
      }
      if (time >= span.end_time) {
        cgltf_accessor_read_float(values, span.end_index, pose.translation, 3);
        continue;
      }

      const cgltf_size key_index =
          clamp_key_index(find_key_index(times, time), times, values);
      const cgltf_size next_index = std::min(key_index + 1, max_key_index);
      const float alpha = sample_channel_alpha(times, key_index, next_index, time);
      float v0[3]{};
      float v1[3]{};
      if (!cgltf_accessor_read_float(values, key_index, v0, 3)) {
        continue;
      }
      if (!cgltf_accessor_read_float(values, next_index, v1, 3)) {
        continue;
      }
      for (int i = 0; i < 3; ++i) {
        pose.translation[i] = v0[i] + alpha * (v1[i] - v0[i]);
      }
    } else if (channel.target_path == cgltf_animation_path_type_rotation) {
      if (time <= span.start_time) {
        float q[4]{};
        cgltf_accessor_read_float(values, span.start_index, q, 4);
        pose.rotation = Quat{q[0], q[1], q[2], q[3]};
        continue;
      }
      if (time >= span.end_time) {
        float q[4]{};
        cgltf_accessor_read_float(values, span.end_index, q, 4);
        pose.rotation = Quat{q[0], q[1], q[2], q[3]};
        continue;
      }

      const cgltf_size key_index =
          clamp_key_index(find_key_index(times, time), times, values);
      const cgltf_size next_index = std::min(key_index + 1, max_key_index);
      const float alpha = sample_channel_alpha(times, key_index, next_index, time);
      float q0[4]{};
      float q1[4]{};
      if (!cgltf_accessor_read_float(values, key_index, q0, 4)) {
        continue;
      }
      if (!cgltf_accessor_read_float(values, next_index, q1, 4)) {
        continue;
      }
      pose.rotation = quat_slerp(Quat{q0[0], q0[1], q0[2], q0[3]},
                                 Quat{q1[0], q1[1], q1[2], q1[3]}, alpha);
    } else if (channel.target_path == cgltf_animation_path_type_scale) {
      if (time <= span.start_time) {
        cgltf_accessor_read_float(values, span.start_index, pose.scale, 3);
        continue;
      }
      if (time >= span.end_time) {
        cgltf_accessor_read_float(values, span.end_index, pose.scale, 3);
        continue;
      }

      const cgltf_size key_index =
          clamp_key_index(find_key_index(times, time), times, values);
      const cgltf_size next_index = std::min(key_index + 1, max_key_index);
      const float alpha = sample_channel_alpha(times, key_index, next_index, time);
      float v0[3]{};
      float v1[3]{};
      if (!cgltf_accessor_read_float(values, key_index, v0, 3)) {
        continue;
      }
      if (!cgltf_accessor_read_float(values, next_index, v1, 3)) {
        continue;
      }
      for (int i = 0; i < 3; ++i) {
        pose.scale[i] = v0[i] + alpha * (v1[i] - v0[i]);
      }
    }
  }
  if (lock_root_motion_ && root_motion_node_ >= 0 &&
      root_motion_node_ < static_cast<int>(out.size())) {
    NodePose &root = out[static_cast<std::size_t>(root_motion_node_)];
    const NodePose &rest = rest_poses_[static_cast<std::size_t>(root_motion_node_)];
    std::memcpy(root.translation, rest.translation, sizeof(root.translation));
  }
}

void SamuraiAsset::update_global_matrices() {
  const auto *data = static_cast<const cgltf_data *>(data_);
  std::vector<int> depth(data->nodes_count, 0);
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    int d = 0;
    const cgltf_node *node = &data->nodes[i];
    while (node->parent) {
      ++d;
      node = node->parent;
    }
    depth[i] = d;
  }

  std::vector<int> order(data->nodes_count);
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    order[i] = static_cast<int>(i);
  }
  std::sort(order.begin(), order.end(),
            [&](int a, int b) { return depth[static_cast<std::size_t>(a)] < depth[static_cast<std::size_t>(b)]; });

  for (const int node_index : order) {
    const cgltf_node &node = data->nodes[node_index];
    const Mat4 local = node_local_matrix(node_index, current_poses_);
    if (node.parent) {
      const int parent_index = static_cast<int>(node.parent - data->nodes);
      global_matrices_[static_cast<std::size_t>(node_index)] =
          global_matrices_[static_cast<std::size_t>(parent_index)] * local;
    } else {
      global_matrices_[static_cast<std::size_t>(node_index)] = local;
    }
  }
}

void SamuraiAsset::update_skin_matrices() {
  const auto *data = static_cast<const cgltf_data *>(data_);
  if (samurai_skin_index_ < 0) {
    return;
  }
  const cgltf_skin &skin = data->skins[samurai_skin_index_];
  skin_matrices_.resize(skin.joints_count);
  for (cgltf_size i = 0; i < skin.joints_count; ++i) {
    const int joint_node = static_cast<int>(skin.joints[i] - data->nodes);
    skin_matrices_[i] = global_matrices_[static_cast<std::size_t>(joint_node)] *
                        inverse_bind_[static_cast<std::size_t>(i)];
  }
}

void SamuraiAsset::compute_model_fixup() {
  const auto *data = static_cast<const cgltf_data *>(data_);
  if (!data || node_samurai_ < 0 || samurai_skin_index_ < 0) {
    model_fixup_ = Mat4::identity();
    return;
  }

  update_global_matrices();
  update_skin_matrices();

  const cgltf_node &samurai_node = data->nodes[node_samurai_];
  const cgltf_mesh *body_mesh = samurai_node.mesh;
  if (!body_mesh || body_mesh->primitives_count == 0) {
    model_fixup_ = Mat4::identity();
    return;
  }

  const cgltf_primitive &prim = body_mesh->primitives[0];
  const cgltf_accessor *pos = nullptr;
  const cgltf_accessor *joints = nullptr;
  const cgltf_accessor *weights = nullptr;
  for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
    switch (prim.attributes[a].type) {
    case cgltf_attribute_type_position:
      pos = prim.attributes[a].data;
      break;
    case cgltf_attribute_type_joints:
      joints = prim.attributes[a].data;
      break;
    case cgltf_attribute_type_weights:
      weights = prim.attributes[a].data;
      break;
    default:
      break;
    }
  }
  if (!pos || !joints || !weights) {
    model_fixup_ = Mat4::identity();
    return;
  }

  float min_y = 1e9f;
  float max_y = -1e9f;
  for (cgltf_size v = 0; v < pos->count; ++v) {
    float p[3]{};
    cgltf_uint joint_values[4]{};
    float weight_values[4]{};
    cgltf_accessor_read_float(pos, v, p, 3);
    cgltf_accessor_read_uint(joints, v, joint_values, 4);
    cgltf_accessor_read_float(weights, v, weight_values, 4);

    std::uint8_t joint_indices[4]{};
    for (int j = 0; j < 4; ++j) {
      joint_indices[j] = static_cast<std::uint8_t>(joint_values[j]);
    }

    float skinned_x = 0.0f;
    float skinned_y = 0.0f;
    float skinned_z = 0.0f;
    skin_vertex_position(skin_matrices_.data(), joint_indices, weight_values, p[0], p[1], p[2],
                         skinned_x, skinned_y, skinned_z);
    min_y = std::min(min_y, skinned_y);
    max_y = std::max(max_y, skinned_y);
  }

  const float raw_height = std::max(max_y - min_y, 0.001f);
  scale_ = kTargetHeight / raw_height;
  model_fixup_ = Mat4::scale(scale_, scale_, scale_) * Mat4::translate(0.0f, -min_y, 0.0f);
}

Mat4 SamuraiAsset::node_global_matrix(int node_index) const {
  return global_matrices_[static_cast<std::size_t>(node_index)];
}

Mat4 SamuraiAsset::character_root_matrix() const {
  return model_fixup_;
}

Mat4 SamuraiAsset::katana_world_matrix() const {
  const int socket_node =
      katana_attached_to_hand() ? node_sockethand_ : node_socketsheath_;
  if (socket_node < 0 || node_katana_ < 0) {
    return Mat4::identity();
  }
  if (keep_weapon_upright_) {
    const Mat4 hand = node_global_matrix(socket_node);
    const Mat4 one_hand_staff = hand * weapon_hand_attachment_;
    if (locomotion_ == LocomotionState::Ultimate) {
      // The power-up pose presents the staff as a vertical focus. Preserve
      // the hand-attached grip position but replace animation tilt with the
      // calibrated upright staff basis.
      Mat4 vertical_staff = upright_weapon_basis_;
      vertical_staff.m[12] = hand.m[12];
      vertical_staff.m[13] = one_hand_staff.m[13] + 0.08f;
      vertical_staff.m[14] = hand.m[14];
      if (node_lefthand_ >= 0) {
        const Mat4 left_hand = node_global_matrix(node_lefthand_);
        // A vertical shaft cannot follow a diagonal hand-to-hand line. Move
        // its upright axis between the two palms so both hands close around
        // it instead of leaving it beside the power-up pose.
        vertical_staff.m[12] = (hand.m[12] + left_hand.m[12]) * 0.50f;
        vertical_staff.m[14] = (hand.m[14] + left_hand.m[14]) * 0.50f;
      }
      return vertical_staff;
    }
    if (node_lefthand_ >= 0) {
      const Mat4 left_hand = node_global_matrix(node_lefthand_);
      const float direction_x = left_hand.m[12] - hand.m[12];
      const float direction_y = left_hand.m[13] - hand.m[13];
      const float direction_z = left_hand.m[14] - hand.m[14];
      const float hand_separation = std::sqrt(direction_x * direction_x +
                                              direction_y * direction_y +
                                              direction_z * direction_z);
      if (hand_separation > 1e-4f) {
        constexpr float kFrontHandGripClearance = 0.14f;
        const float hand_direction_x = direction_x / hand_separation;
        const float hand_direction_y = direction_y / hand_separation;
        const float hand_direction_z = direction_z / hand_separation;
        constexpr float staff_axis_x = 0.248998f;
        constexpr float staff_axis_y = 0.439192f;
        constexpr float staff_axis_z = 0.863198f;
        const float current_axis_x = one_hand_staff.m[0] * staff_axis_x +
                                     one_hand_staff.m[4] * staff_axis_y +
                                     one_hand_staff.m[8] * staff_axis_z;
        const float current_axis_y = one_hand_staff.m[1] * staff_axis_x +
                                     one_hand_staff.m[5] * staff_axis_y +
                                     one_hand_staff.m[9] * staff_axis_z;
        const float current_axis_z = one_hand_staff.m[2] * staff_axis_x +
                                     one_hand_staff.m[6] * staff_axis_y +
                                     one_hand_staff.m[10] * staff_axis_z;
        const Mat4 aim_staff_at_left_hand =
            align_y_to_direction(direction_x, direction_y, direction_z) *
            align_direction_to_y(current_axis_x, current_axis_y, current_axis_z);
        // Rotate the already-correct full-length staff around the right-hand
        // grip. This preserves its scale and placement while making the shaft
        // pass in the left hand's direction.
        return Mat4::translate(hand_direction_x * kFrontHandGripClearance,
                               hand_direction_y * kFrontHandGripClearance,
                               hand_direction_z * kFrontHandGripClearance) *
               Mat4::translate(hand.m[12], hand.m[13], hand.m[14]) *
               aim_staff_at_left_hand *
               Mat4::translate(-hand.m[12], -hand.m[13], -hand.m[14]) *
               one_hand_staff;
      }
    }
    return one_hand_staff;
  }
  return node_global_matrix(socket_node) * node_local_matrix(node_katana_, rest_poses_);
}

bool SamuraiAsset::katana_attached_to_hand() const {
  if (node_sockethand_ < 0 || node_socketsheath_ < 0 || node_katana_ < 0) {
    return sword_drawn_;
  }
  if (!equip_active_) {
    return sword_drawn_;
  }
  if (equip_drawing_) {
    return equip_time_ >= equip_swap_time_;
  }
  return equip_time_ < equip_swap_time_;
}

bool SamuraiAsset::load(const char *filename) {
  asset_path_ = resolve_asset_path(filename ? filename : kAssetFile);
  lock_root_motion_ = filename && std::strstr(filename, "pharaoh") != nullptr;
  file_bytes_ = read_binary_file(asset_path_);
  if (file_bytes_.empty()) {
    std::fprintf(stderr, "Failed to read asset: %s\n", asset_path_.c_str());
    return false;
  }

  cgltf_options options{};
  cgltf_data *data = nullptr;
  cgltf_result result =
      cgltf_parse(&options, file_bytes_.data(), file_bytes_.size(), &data);
  if (result != cgltf_result_success) {
    std::fprintf(stderr, "cgltf_parse failed for %s\n", asset_path_.c_str());
    return false;
  }
  result = cgltf_load_buffers(&options, data, asset_path_.c_str());
  if (result != cgltf_result_success) {
    std::fprintf(stderr, "cgltf_load_buffers failed for %s\n", asset_path_.c_str());
    cgltf_free(data);
    return false;
  }
  data_ = data;

  node_samurai_ = find_node("samurai");
  node_sheath_ = find_node("sheath");
  node_katana_ = find_node("mesh_input");
  node_sockethand_ = find_node("sockethand");
  if (node_sockethand_ < 0) {
    node_sockethand_ = find_node("mixamorig:RightHand");
  }
  node_lefthand_ = find_node("mixamorig:LeftHand");
  node_socketsheath_ = find_node("socketsheath");
  node_fingertip_ = find_node("mixamorig:RightHandIndex4_end");
  if (node_fingertip_ < 0) {
    node_fingertip_ = find_node("mixamorig:RightHandIndex4");
  }
  root_motion_node_ = find_node("mixamorig:Hips");

  cache_rest_poses();
  clip_idle_ = find_clip("breathingidle", true);
  clip_run_forward_ = find_clip("armedrunforward", true);
  clip_run_back_ = find_clip("armedrunback", true);
  clip_run_left_ = find_clip("combatwalkleft", true);
  clip_run_right_ = find_clip("combatwalkright", true);
  clip_sprint_ = find_clip("fastrun", true);
  clip_jump_ = lock_root_motion_
                   ? find_clip("Armature|Armature|flying_kick", false)
                   : find_clip("twistflip", false);
  clip_fly_ = lock_root_motion_
                  ? find_clip("Armature|Armature|flying", true)
                  : find_clip("twistflip", true);
  clip_swing_ = lock_root_motion_
                    ? find_clip("Armature|Armature|staff_smash", false)
                    : find_clip("overheadslash", false);
  clip_shoot_ = lock_root_motion_
                    ? find_clip("Armature|Armature|finger_shoot_position", false)
                    : AnimationClip{};
  clip_ultimate_ = lock_root_motion_
                       ? find_clip("Armature|Armature|ultimate", false)
                       : clip_swing_;
  clip_equip_ = find_clip("onearmequip", false);
  if (clip_sprint_.animation_index < 0) {
    clip_sprint_ = clip_run_forward_;
  }
  if (clip_jump_.animation_index < 0) {
    clip_jump_ = clip_run_forward_;
    clip_jump_.loop = false;
  }
  if (clip_fly_.animation_index < 0) {
    clip_fly_ = clip_idle_;
  }
  if (clip_run_back_.animation_index < 0) {
    clip_run_back_ = clip_run_forward_;
  }
  if (clip_run_left_.animation_index < 0) {
    clip_run_left_ = clip_run_forward_;
  }
  if (clip_run_right_.animation_index < 0) {
    clip_run_right_ = clip_run_forward_;
  }
  if (clip_equip_.animation_index >= 0) {
    equip_swap_time_ = clip_equip_.duration * 0.55f;
  }
  log_animation_catalog();

  const cgltf_node *samurai_node =
      node_samurai_ >= 0 ? &data->nodes[node_samurai_] : nullptr;
  if (!samurai_node || !samurai_node->mesh) {
    std::fprintf(stderr, "Samurai skinned mesh node not found\n");
    return false;
  }
  samurai_skin_index_ =
      samurai_node->skin ? static_cast<int>(samurai_node->skin - data->skins) : -1;

  if (samurai_skin_index_ >= 0) {
    const cgltf_skin &skin = data->skins[samurai_skin_index_];
    inverse_bind_.resize(skin.joints_count);
    for (cgltf_size i = 0; i < skin.joints_count; ++i) {
      float matrix[16]{};
      cgltf_accessor_read_float(skin.inverse_bind_matrices, i, matrix, 16);
      std::memcpy(inverse_bind_[i].m, matrix, sizeof(float) * 16);
    }
  }

  if (clip_idle_.animation_index >= 0) {
    sample_animation(clip_idle_.animation_index, clip_sample_time(clip_idle_, 0.0f),
                     current_poses_);
  }

  compute_model_fixup();

  keep_weapon_upright_ = lock_root_motion_ && node_sockethand_ >= 0 && node_katana_ >= 0;
  if (keep_weapon_upright_) {
    update_global_matrices();
    const Mat4 hand = node_global_matrix(node_sockethand_);
    const Mat4 weapon = node_global_matrix(node_katana_);
    // The exported staff mesh is authored with its long local Y axis upright.
    // Preserve only its animated hand-relative position here; carrying the
    // Mixamo bind-pose rotation into locomotion tilts it across the face.
    // PCA was measured after Blender's glTF import (Z-up). Convert that axis
    // back to the engine/glTF Y-up basis before aligning it to world-up.
    upright_weapon_basis_ = align_direction_to_y(0.248998f, 0.439192f, 0.863198f);
    Mat4 calibrated_staff = upright_weapon_basis_;
    calibrated_staff.m[12] = weapon.m[12];
    calibrated_staff.m[13] = weapon.m[13];
    calibrated_staff.m[14] = weapon.m[14];
    // Store the corrected staff pose in hand-local space. During the smash,
    // multiplying it by the animated hand makes position and rotation follow
    // the grip instead of leaving the staff upright in world space.
    weapon_hand_attachment_ = mat4_inverse(hand) * calibrated_staff;
  }

  textures_.assign(data->textures_count, 0);
  std::vector<unsigned int> image_textures(data->images_count, 0);
  for (cgltf_size i = 0; i < data->images_count; ++i) {
    const cgltf_image &image = data->images[i];
    int width = 0;
    int height = 0;
    int comp = 0;
    unsigned char *pixels = nullptr;
    if (image.buffer_view) {
      const cgltf_buffer_view *view = image.buffer_view;
      const unsigned char *buffer =
          static_cast<const unsigned char *>(view->buffer->data) + view->offset;
      pixels = stbi_load_from_memory(buffer, static_cast<int>(view->size), &width, &height, &comp,
                                     4);
    } else if (image.uri) {
      std::string image_path =
          asset_path_.substr(0, asset_path_.find_last_of("/\\") + 1) + image.uri;
      pixels = stbi_load(image_path.c_str(), &width, &height, &comp, 4);
    }
    unsigned int tex = 0;
    gl::gen_textures(1, &tex);
    gl::bind_texture(kGlTexture2D, tex);
    gl::texParameteri(kGlTexture2D, kGlTextureWrapS, kGlRepeat);
    gl::texParameteri(kGlTexture2D, kGlTextureWrapT, kGlRepeat);
    gl::texParameteri(kGlTexture2D, 0x2801, kGlLinear);
    gl::texParameteri(kGlTexture2D, 0x2800, kGlLinear);
    if (pixels) {
      gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), width, height, 0, kGlRgba,
                      kGlUnsignedByteType, pixels);
      stbi_image_free(pixels);
    } else {
      const unsigned char white[] = {255, 255, 255, 255};
      gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), 1, 1, 0, kGlRgba,
                      kGlUnsignedByteType, white);
    }
    if (i < image_textures.size()) {
      image_textures[i] = tex;
    }
  }
  for (cgltf_size i = 0; i < data->textures_count; ++i) {
    const cgltf_texture &texture = data->textures[i];
    if (texture.image) {
      const cgltf_size image_index = static_cast<cgltf_size>(texture.image - data->images);
      if (image_index < image_textures.size()) {
        textures_[i] = image_textures[image_index];
      }
    }
  }

  gl::gen_textures(1, &fallback_texture_);
  gl::bind_texture(kGlTexture2D, fallback_texture_);
  const unsigned char white[] = {255, 255, 255, 255};
  gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), 1, 1, 0, kGlRgba,
                  kGlUnsignedByteType, white);
  gl::texParameteri(kGlTexture2D, 0x2801, kGlLinear);
  gl::texParameteri(kGlTexture2D, 0x2800, kGlLinear);

  material_color_textures_.assign(data->materials_count, 0);
  for (cgltf_size i = 0; i < data->materials_count; ++i) {
    const cgltf_material &material = data->materials[i];
    float color[4]{1.0f, 1.0f, 1.0f, 1.0f};
    if (material.has_pbr_metallic_roughness) {
      std::memcpy(color, material.pbr_metallic_roughness.base_color_factor,
                  sizeof(color));
    }
    const unsigned char pixel[] = {
        static_cast<unsigned char>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color[3], 0.0f, 1.0f) * 255.0f)};
    unsigned int texture = 0;
    gl::gen_textures(1, &texture);
    gl::bind_texture(kGlTexture2D, texture);
    gl::tex_image2d(kGlTexture2D, 0, static_cast<int>(kGlRgba), 1, 1, 0, kGlRgba,
                    kGlUnsignedByteType, pixel);
    gl::texParameteri(kGlTexture2D, 0x2801, kGlLinear);
    gl::texParameteri(kGlTexture2D, 0x2800, kGlLinear);
    material_color_textures_[i] = texture;
  }

  auto upload_primitive = [&](const cgltf_primitive &prim, bool skinned, SkinnedMeshGpu *skinned_out,
                              RigidMeshGpu *rigid_out, int node_index) -> bool {
    const cgltf_accessor *pos = nullptr;
    const cgltf_accessor *norm = nullptr;
    const cgltf_accessor *uv = nullptr;
    const cgltf_accessor *joints = nullptr;
    const cgltf_accessor *weights = nullptr;
    for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
      const cgltf_attribute &attr = prim.attributes[a];
      switch (attr.type) {
      case cgltf_attribute_type_position:
        pos = attr.data;
        break;
      case cgltf_attribute_type_normal:
        norm = attr.data;
        break;
      case cgltf_attribute_type_texcoord:
        if (!uv) {
          uv = attr.data;
        }
        break;
      case cgltf_attribute_type_joints:
        joints = attr.data;
        break;
      case cgltf_attribute_type_weights:
        weights = attr.data;
        break;
      default:
        break;
      }
    }
    if (!pos || !prim.indices) {
      return false;
    }

    std::vector<GpuVertex> vertices(pos->count);
    for (cgltf_size v = 0; v < pos->count; ++v) {
      float p[3]{};
      float n[3]{0.0f, 1.0f, 0.0f};
      float t[2]{0.0f, 0.0f};
      cgltf_accessor_read_float(pos, v, p, 3);
      if (norm) {
        cgltf_accessor_read_float(norm, v, n, 3);
      }
      if (uv) {
        cgltf_accessor_read_float(uv, v, t, 2);
      }
      vertices[v].px = p[0];
      vertices[v].py = p[1];
      vertices[v].pz = p[2];
      vertices[v].nx = n[0];
      vertices[v].ny = n[1];
      vertices[v].nz = n[2];
      vertices[v].u = t[0];
      vertices[v].v = t[1];
      if (joints && weights) {
        cgltf_uint joint_values[4]{};
        float weight_values[4]{};
        cgltf_accessor_read_uint(joints, v, joint_values, 4);
        cgltf_accessor_read_float(weights, v, weight_values, 4);
        for (int j = 0; j < 4; ++j) {
          vertices[v].joints[j] = static_cast<std::uint8_t>(joint_values[j]);
          vertices[v].weights[j] = weight_values[j];
        }
      }
    }

    std::vector<unsigned short> indices(prim.indices->count);
    for (cgltf_size i = 0; i < prim.indices->count; ++i) {
      cgltf_uint index = 0;
      cgltf_accessor_read_uint(prim.indices, i, &index, 1);
      indices[i] = static_cast<unsigned short>(index);
    }

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    gl::gen_buffers(1, &vbo);
    gl::gen_buffers(1, &ebo);
    vao = gl::create_vertex_array();
    gl::bind_vertex_array(vao);
    gl::bind_buffer(kGlArrayBuffer, vbo);
    gl::buffer_data(kGlArrayBuffer,
                    static_cast<long long>(vertices.size() * sizeof(GpuVertex)), vertices.data(),
                    kGlStaticDraw);
    gl::bind_buffer(kGlElementArrayBuffer, ebo);
    gl::buffer_data(kGlElementArrayBuffer,
                    static_cast<long long>(indices.size() * sizeof(unsigned short)),
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
    if (skinned) {
      gl::vertex_attrib_ipointer(3, 4, kGlUnsignedByte, stride,
                                 reinterpret_cast<const void *>(offsetof(GpuVertex, joints)));
      gl::enable_vertex_attrib_array(3);
      gl::vertex_attrib_pointer(4, 4, kGlFloat, kGlFalse, stride,
                                reinterpret_cast<const void *>(offsetof(GpuVertex, weights)));
      gl::enable_vertex_attrib_array(4);
    }

    if (skinned && skinned_out) {
      skinned_out->vao = vao;
      skinned_out->vbo = vbo;
      skinned_out->ebo = ebo;
      skinned_out->index_count = static_cast<int>(indices.size());
      skinned_out->index_type = kGlUnsignedShort;
      skinned_out->material_index =
          prim.material ? static_cast<int>(prim.material - data->materials) : 0;
    } else if (rigid_out) {
      rigid_out->vao = vao;
      rigid_out->vbo = vbo;
      rigid_out->ebo = ebo;
      rigid_out->index_count = static_cast<int>(indices.size());
      rigid_out->index_type = kGlUnsignedShort;
      rigid_out->material_index =
          prim.material ? static_cast<int>(prim.material - data->materials) : 0;
      rigid_out->node_index = node_index;
    }
    return true;
  };

  const cgltf_mesh *body_mesh = samurai_node->mesh;
  for (cgltf_size i = 0; i < body_mesh->primitives_count; ++i) {
    SkinnedMeshGpu part{};
    if (!upload_primitive(body_mesh->primitives[i], true, &part, nullptr, node_samurai_)) {
      std::fprintf(stderr, "Failed to upload animated character mesh part %zu\n", i);
      return false;
    }
    skinned_meshes_.push_back(part);
  }

  if (node_sheath_ >= 0 && data->nodes[node_sheath_].mesh) {
    RigidMeshGpu sheath{};
    const cgltf_mesh *mesh = data->nodes[node_sheath_].mesh;
    if (mesh->primitives_count > 0) {
      upload_primitive(mesh->primitives[0], false, nullptr, &sheath, node_sheath_);
      rigid_meshes_.push_back(sheath);
    }
  }
  if (node_katana_ >= 0 && data->nodes[node_katana_].mesh) {
    RigidMeshGpu katana{};
    const cgltf_mesh *mesh = data->nodes[node_katana_].mesh;
    if (mesh->primitives_count > 0) {
      upload_primitive(mesh->primitives[0], false, nullptr, &katana, node_katana_);
      rigid_meshes_.push_back(katana);
    }
  }

  std::fprintf(stderr, "Loaded animated character from %s (scale %.4f, bones %zu)\n", asset_path_.c_str(),
               scale_, inverse_bind_.size());
  return true;
}

void SamuraiAsset::shutdown() {
  for (unsigned int texture : material_color_textures_) {
    if (texture != 0) {
      gl::delete_textures(1, &texture);
    }
  }
  material_color_textures_.clear();
  if (fallback_texture_ != 0) {
    gl::delete_textures(1, &fallback_texture_);
    fallback_texture_ = 0;
  }
  if (data_) {
    cgltf_free(static_cast<cgltf_data *>(data_));
    data_ = nullptr;
  }
  file_bytes_.clear();
  asset_path_.clear();
  textures_.clear();
  skinned_meshes_.clear();
  rigid_meshes_.clear();
}

void SamuraiAsset::update(float delta_time, bool moving, bool sprinting, bool airborne,
                          bool flying, bool swinging, bool shooting, bool ultimate, bool dodging,
                          bool toggle_equip,
                          float move_forward, float move_right) {
  staff_dismiss_time_ = std::max(0.0f, staff_dismiss_time_ - delta_time);
  if (toggle_equip && !equip_active_ && !swinging && !dodging) {
    if (clip_equip_.animation_index >= 0) {
      equip_active_ = true;
      equip_drawing_ = !sword_drawn_;
      equip_time_ = 0.0f;
    } else {
      sword_drawn_ = !sword_drawn_;
    }
  }

  const LocomotionState target =
      choose_locomotion_state(moving, sprinting, airborne, flying, swinging,
                              shooting, ultimate, dodging,
                              move_forward, move_right);
  if (target != locomotion_) {
    const LocomotionState from = locomotion_;
    const bool from_staff_action = from == LocomotionState::Swing ||
                                   from == LocomotionState::Ultimate;
    const bool to_staff_action = target == LocomotionState::Swing ||
                                 target == LocomotionState::Ultimate;
    if (keep_weapon_upright_ && from_staff_action && !to_staff_action) {
      staff_dismiss_time_ = 0.32f;
    }
    previous_locomotion_ = locomotion_;
    locomotion_ = target;
    blend_time_ = 0.0f;
    previous_poses_ = current_poses_;
    const bool from_action =
        from == LocomotionState::Idle || from == LocomotionState::Swing ||
        from == LocomotionState::Ultimate ||
        from == LocomotionState::Shoot || from == LocomotionState::Jump ||
        from == LocomotionState::Fly ||
        from == LocomotionState::Equip;
    if (target == LocomotionState::Swing) {
      swing_time_ = 0.0f;
      swing_active_ = true;
      staff_dismiss_time_ = 0.0f;
    } else if (target == LocomotionState::Ultimate) {
      ultimate_time_ = 0.0f;
      staff_dismiss_time_ = 0.0f;
    } else if (target == LocomotionState::Shoot) {
      shoot_time_ = 0.0f;
    } else if (target == LocomotionState::Jump) {
      jump_time_ = 0.0f;
    } else if (target == LocomotionState::Fly) {
      fly_time_ = 0.0f;
    } else if (target == LocomotionState::Equip) {
      equip_time_ = 0.0f;
    } else if (target == LocomotionState::Idle) {
      idle_time_ = 0.0f;
    } else if (from_action) {
      if (target == LocomotionState::Sprint) {
        sprint_time_ = 0.0f;
      } else if (target == LocomotionState::RunForward) {
        run_forward_time_ = 0.0f;
      } else if (target == LocomotionState::RunBack) {
        run_back_time_ = 0.0f;
      } else if (target == LocomotionState::RunLeft) {
        run_left_time_ = 0.0f;
      } else if (target == LocomotionState::RunRight) {
        run_right_time_ = 0.0f;
      }
    }
  }

  blend_time_ = std::min(1.0f, blend_time_ + delta_time / kBlendDuration);

  const AnimationClip &primary = clip_for_state(locomotion_);
  const AnimationClip &secondary = clip_for_state(previous_locomotion_);

  advance_clip_time(locomotion_, delta_time, primary.loop);
  if (blend_time_ < 1.0f && previous_locomotion_ != locomotion_) {
    advance_clip_time(previous_locomotion_, delta_time, secondary.loop);
  }

  if (locomotion_ == LocomotionState::Swing &&
      swing_time_ >= clip_swing_.duration - 1e-4f) {
    swing_active_ = false;
    if (keep_weapon_upright_) {
      staff_dismiss_time_ = 0.32f;
    }
    locomotion_ = choose_locomotion_state(moving, sprinting, airborne, flying,
                                          false, shooting, ultimate, dodging,
                                          move_forward, move_right);
    previous_locomotion_ = LocomotionState::Swing;
    blend_time_ = 0.0f;
    previous_poses_ = current_poses_;
    if (locomotion_ == LocomotionState::Sprint) {
      sprint_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunForward) {
      run_forward_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunBack) {
      run_back_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunLeft) {
      run_left_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunRight) {
      run_right_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::Idle) {
      idle_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::Equip) {
      equip_time_ = 0.0f;
    }
  }

  if (locomotion_ == LocomotionState::Shoot &&
      shoot_time_ >= clip_shoot_.duration - 1e-4f) {
    locomotion_ = choose_locomotion_state(moving, sprinting, airborne, flying,
                                          false, false, ultimate, dodging,
                                          move_forward, move_right);
    previous_locomotion_ = LocomotionState::Shoot;
    blend_time_ = 0.0f;
    previous_poses_ = current_poses_;
  }

  if (equip_active_ && equip_time_ >= clip_equip_.duration - 1e-4f) {
    sword_drawn_ = equip_drawing_;
    equip_active_ = false;
    locomotion_ = choose_locomotion_state(moving, sprinting, airborne, flying,
                                          false, false, ultimate, dodging,
                                          move_forward, move_right);
    previous_locomotion_ = LocomotionState::Equip;
    blend_time_ = 0.0f;
    previous_poses_ = current_poses_;
    if (locomotion_ == LocomotionState::Sprint) {
      sprint_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunForward) {
      run_forward_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunBack) {
      run_back_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunLeft) {
      run_left_time_ = 0.0f;
    } else if (locomotion_ == LocomotionState::RunRight) {
      run_right_time_ = 0.0f;
    } else {
      idle_time_ = 0.0f;
    }
  }

  const float primary_time = clip_sample_time(primary, clip_playback_time(locomotion_));
  const float secondary_time =
      clip_sample_time(secondary, clip_playback_time(previous_locomotion_));

  std::vector<NodePose> pose_a;
  std::vector<NodePose> pose_b;
  if (primary.animation_index >= 0) {
    sample_animation(primary.animation_index, primary_time, pose_a);
  } else {
    pose_a = rest_poses_;
  }
  if (blend_time_ < 1.0f) {
    sample_animation(secondary.animation_index, secondary_time, pose_b);
    current_poses_ = rest_poses_;
    for (std::size_t i = 0; i < current_poses_.size(); ++i) {
      const float t = blend_time_;
      for (int c = 0; c < 3; ++c) {
        current_poses_[i].translation[c] =
            pose_b[i].translation[c] +
            t * (pose_a[i].translation[c] - pose_b[i].translation[c]);
        current_poses_[i].scale[c] =
            pose_b[i].scale[c] + t * (pose_a[i].scale[c] - pose_b[i].scale[c]);
      }
      current_poses_[i].rotation = quat_slerp(pose_b[i].rotation, pose_a[i].rotation, t);
    }
  } else {
    current_poses_ = pose_a;
  }

  update_global_matrices();
  update_skin_matrices();
}

unsigned int SamuraiAsset::texture_for_material(int material_index) const {
  if (material_index < 0) {
    return fallback_texture_;
  }
  const auto *data = static_cast<const cgltf_data *>(data_);
  if (!data || material_index >= static_cast<int>(data->materials_count)) {
    return fallback_texture_;
  }
  const cgltf_material &material = data->materials[material_index];
  if (!material.has_pbr_metallic_roughness ||
      !material.pbr_metallic_roughness.base_color_texture.texture) {
    return material_index < static_cast<int>(material_color_textures_.size())
               ? material_color_textures_[static_cast<std::size_t>(material_index)]
               : fallback_texture_;
  }
  const cgltf_size tex_index = static_cast<cgltf_size>(
      material.pbr_metallic_roughness.base_color_texture.texture - data->textures);
  if (tex_index >= textures_.size() || textures_[tex_index] == 0) {
    return fallback_texture_;
  }
  return textures_[tex_index];
}

void SamuraiAsset::upload_bone_uniforms(unsigned int program) const {
  if (cached_bone_program_ != program || bone_uniform_locs_.empty()) {
    cached_bone_program_ = program;
    bone_uniform_locs_.assign(kMaxBones, -1);
    for (int i = 0; i < kMaxBones; ++i) {
      char name[32];
      std::snprintf(name, sizeof(name), "uBones[%d]", i);
      bone_uniform_locs_[static_cast<std::size_t>(i)] = gl::get_uniform_location(program, name);
    }
  }

  const int bone_count = std::min(static_cast<int>(skin_matrices_.size()), kMaxBones);
  for (int i = 0; i < bone_count; ++i) {
    const int loc = bone_uniform_locs_[static_cast<std::size_t>(i)];
    if (loc >= 0) {
      gl::uniform_matrix4fv(loc, 1, kGlFalse, skin_matrices_[static_cast<std::size_t>(i)].m);
    }
  }
}

void SamuraiAsset::draw(unsigned int skinned_program, unsigned int rigid_program,
                        const Mat4 &projection, const Mat4 &view,
                        const Mat4 &player_model, bool hide_shooting_finger,
                        float character_opacity, float character_dissolve,
                        float character_reveal_progress) {
  const Mat4 vp = projection * view;
  const Mat4 model = player_model * model_fixup_;

  if (!skinned_meshes_.empty()) {
    gl::use_program(skinned_program);
    const int vp_loc = gl::get_uniform_location(skinned_program, "uVP");
    const int model_loc = gl::get_uniform_location(skinned_program, "uModel");
    const int tex_loc = gl::get_uniform_location(skinned_program, "uDiffuse");
    const int light_loc = gl::get_uniform_location(skinned_program, "uLightDir");
    const int opacity_loc = gl::get_uniform_location(skinned_program, "uOpacity");
    const int dissolve_loc = gl::get_uniform_location(skinned_program, "uDissolve");
    const int dissolve_center_loc =
        gl::get_uniform_location(skinned_program, "uDissolveCenter");
    const int reveal_progress_loc =
        gl::get_uniform_location(skinned_program, "uRevealProgress");
    const int reveal_base_loc =
        gl::get_uniform_location(skinned_program, "uRevealBaseY");
    const bool translucent_character = character_opacity < 0.999f;
    if (translucent_character) {
      gl::enable(kGlBlend);
      gl::blend_func(kGlSrcAlpha, kGlOneMinusSrcAlpha);
    }

    gl::uniform_matrix4fv(vp_loc, 1, kGlFalse, vp.m);
    gl::uniform_matrix4fv(model_loc, 1, kGlFalse, model.m);
    gl::uniform3f(light_loc, kSunDirX, kSunDirY, kSunDirZ);
    gl::uniform1f(dissolve_loc, character_dissolve);
    gl::uniform3f(dissolve_center_loc, player_model.m[12],
                  player_model.m[13] + 0.77f, player_model.m[14]);
    gl::uniform1f(reveal_progress_loc, character_reveal_progress);
    gl::uniform1f(reveal_base_loc, player_model.m[13]);
    upload_bone_uniforms(skinned_program);

    const auto *data = static_cast<const cgltf_data *>(data_);
    const auto is_hidden_finger = [hide_shooting_finger, data](const SkinnedMeshGpu &mesh) {
      if (!hide_shooting_finger || !data || mesh.material_index < 0 ||
          mesh.material_index >= static_cast<int>(data->materials_count)) {
        return false;
      }
      const char *name = data->materials[mesh.material_index].name;
      return name && std::strstr(name, "sand_finger") != nullptr;
    };
    const auto draw_skinned_mesh = [&](const SkinnedMeshGpu &mesh, float opacity) {
      const unsigned int tex = texture_for_material(mesh.material_index);
      gl::active_texture(kGlTexture0);
      gl::bind_texture(kGlTexture2D, tex);
      gl::uniform1i(tex_loc, 0);
      gl::uniform1f(opacity_loc, opacity);
      gl::bind_vertex_array(mesh.vao);
      gl::draw_elements(kGlTriangles, mesh.index_count, mesh.index_type, nullptr);
    };

    // Once the projectile releases, omit the dedicated finger primitive so
    // the source finger vanishes on precisely the same rendered frame.
    if (character_opacity > 0.001f) {
      for (const SkinnedMeshGpu &mesh : skinned_meshes_) {
        if (!is_hidden_finger(mesh)) {
          draw_skinned_mesh(mesh, character_opacity);
        }
      }
    }
    if (translucent_character) {
      gl::disable(kGlBlend);
    }
  }

  gl::use_program(rigid_program);
  const int mvp_loc = gl::get_uniform_location(rigid_program, "uMVP");
  const int model_loc = gl::get_uniform_location(rigid_program, "uModel");
  const int tex_loc = gl::get_uniform_location(rigid_program, "uDiffuse");
  const int light_loc = gl::get_uniform_location(rigid_program, "uLightDir");
  const int opacity_loc = gl::get_uniform_location(rigid_program, "uOpacity");
  const int dissolve_loc = gl::get_uniform_location(rigid_program, "uDissolve");
  const int dissolve_center_loc =
      gl::get_uniform_location(rigid_program, "uDissolveCenter");
  const int reveal_progress_loc =
      gl::get_uniform_location(rigid_program, "uRevealProgress");
  const int reveal_base_loc =
      gl::get_uniform_location(rigid_program, "uRevealBaseY");
  gl::uniform3f(light_loc, kSunDirX, kSunDirY, kSunDirZ);
  gl::uniform1f(dissolve_loc, character_dissolve);
  gl::uniform3f(dissolve_center_loc, player_model.m[12],
                player_model.m[13] + 0.77f, player_model.m[14]);
  gl::uniform1f(reveal_progress_loc, character_reveal_progress);
  gl::uniform1f(reveal_base_loc, player_model.m[13]);
  gl::uniform1f(opacity_loc, 1.0f);

  for (const RigidMeshGpu &mesh : rigid_meshes_) {
    // The Pharaoh summons the staff for combat instead of carrying it during
    // locomotion. Keep the samurai's existing weapon behavior unchanged.
    const bool staff_action = locomotion_ == LocomotionState::Swing ||
                              locomotion_ == LocomotionState::Ultimate;
    if (keep_weapon_upright_ && mesh.node_index == node_katana_ &&
        !staff_action && staff_dismiss_time_ <= 0.0f) {
      continue;
    }
    Mat4 node_transform = Mat4::identity();
    if (mesh.node_index == node_katana_) {
      node_transform = katana_world_matrix();
    } else if (mesh.node_index >= 0) {
      node_transform = node_global_matrix(mesh.node_index);
    }
    const Mat4 world = model * node_transform;
    const Mat4 mvp = vp * world;
    gl::uniform_matrix4fv(mvp_loc, 1, kGlFalse, mvp.m);
    gl::uniform_matrix4fv(model_loc, 1, kGlFalse, world.m);

    // The Pharaoh's staff condenses from sand at the start of the smash
    // instead of appearing fully formed on a single frame.
    const bool pharaoh_staff = keep_weapon_upright_ && mesh.node_index == node_katana_;
    float opacity = 1.0f;
    if (pharaoh_staff && staff_action) {
      // Leave only the sand visible for a split second, then smoothly reveal
      // the solid weapon underneath it.
      const float staff_action_time = locomotion_ == LocomotionState::Ultimate
                                          ? ultimate_time_
                                          : swing_time_;
      opacity = std::clamp((staff_action_time - 0.24f) / 0.20f, 0.0f, 1.0f);
      opacity = opacity * opacity * (3.0f - 2.0f * opacity);
    } else if (pharaoh_staff && staff_dismiss_time_ > 0.0f) {
      opacity = std::clamp(staff_dismiss_time_ / 0.32f, 0.0f, 1.0f);
      opacity = opacity * opacity * (3.0f - 2.0f * opacity);
    }
    opacity *= character_opacity;
    const bool translucent_staff = opacity < 0.999f;
    if (translucent_staff) {
      gl::enable(kGlBlend);
      gl::blend_func(kGlSrcAlpha, kGlOneMinusSrcAlpha);
    }
    gl::uniform1f(opacity_loc, opacity);

    const unsigned int tex = texture_for_material(mesh.material_index);
    gl::active_texture(kGlTexture0);
    gl::bind_texture(kGlTexture2D, tex);
    gl::uniform1i(tex_loc, 0);

    if (opacity > 0.001f) {
      gl::bind_vertex_array(mesh.vao);
      gl::draw_elements(kGlTriangles, mesh.index_count, mesh.index_type, nullptr);
    }
    if (translucent_staff) {
      gl::disable(kGlBlend);
    }
  }
}

bool SamuraiAsset::sword_tip_world(const Mat4 &player_model, float &x, float &y, float &z) const {
  if (!katana_attached_to_hand()) {
    return false;
  }
  const Mat4 model = player_model * model_fixup_;
  const Mat4 katana = katana_world_matrix();
  x = 0.0f;
  y = 0.0f;
  z = 1.2f / scale_;
  transform_point(model * katana, x, y, z);
  return true;
}

bool SamuraiAsset::staff_segment_world(const Mat4 &player_model,
                                       float &start_x, float &start_y, float &start_z,
                                       float &end_x, float &end_y, float &end_z) const {
  const bool staff_action = locomotion_ == LocomotionState::Swing ||
                            locomotion_ == LocomotionState::Ultimate;
  if (!keep_weapon_upright_ ||
      (!staff_action && staff_dismiss_time_ <= 0.0f) ||
      !katana_attached_to_hand()) {
    return false;
  }
  const Mat4 staff_world = player_model * model_fixup_ * katana_world_matrix();
  constexpr float staff_axis_x = 0.248998f;
  constexpr float staff_axis_y = 0.439192f;
  constexpr float staff_axis_z = 0.863198f;
  // The exported staff extends mainly along the negative measured axis from
  // its grip. The old positive interval put the sand behind the hands while
  // the visible shaft extended in the opposite direction.
  const bool ultimate_sleeve = locomotion_ == LocomotionState::Ultimate ||
      (previous_locomotion_ == LocomotionState::Ultimate && staff_dismiss_time_ > 0.0f);
  // The ultimate sleeve sits one foot-length higher than the raw staff bounds
  // and just slightly toward the Pharaoh's right so it visually encloses the
  // shaft instead of hanging below and left of it.
  const float sleeve_start = ultimate_sleeve ? -0.65f : -0.84f;
  const float sleeve_end = ultimate_sleeve ? 0.55f : 0.36f;
  start_x = staff_axis_x * (sleeve_start / scale_);
  start_y = staff_axis_y * (sleeve_start / scale_);
  start_z = staff_axis_z * (sleeve_start / scale_);
  end_x = staff_axis_x * (sleeve_end / scale_);
  end_y = staff_axis_y * (sleeve_end / scale_);
  end_z = staff_axis_z * (sleeve_end / scale_);
  transform_point(staff_world, start_x, start_y, start_z);
  transform_point(staff_world, end_x, end_y, end_z);
  if (ultimate_sleeve) {
    constexpr float right_offset = -0.05f;
    const float right_length = std::max(
        std::sqrt(player_model.m[0] * player_model.m[0] +
                  player_model.m[2] * player_model.m[2]),
        0.001f);
    const float right_x = player_model.m[0] / right_length;
    const float right_z = player_model.m[2] / right_length;
    start_x += right_x * right_offset;
    start_z += right_z * right_offset;
    end_x += right_x * right_offset;
    end_z += right_z * right_offset;
  }
  return true;
}

bool SamuraiAsset::finger_tip_world(const Mat4 &player_model, float &x, float &y,
                                    float &z) const {
  if (node_fingertip_ < 0 ||
      node_fingertip_ >= static_cast<int>(global_matrices_.size())) {
    return false;
  }
  x = 0.0f;
  y = 0.0f;
  z = 0.0f;
  transform_point(player_model * model_fixup_ * node_global_matrix(node_fingertip_), x, y, z);
  return true;
}

} // namespace engine
