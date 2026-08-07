#include "renderer.h"

#include "arena_mesh.h"
#include "dummy_mesh.h"
#include "gl_loader.h"
#include "math.h"
#include "mesh_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace engine {
namespace {

constexpr unsigned int kGlColorBufferBit = 0x00004000;
constexpr unsigned int kGlDepthBufferBit = 0x00000100;
constexpr unsigned int kGlDepthTest = 0x0B71;
constexpr unsigned int kGlLess = 0x0201;
constexpr unsigned int kGlArrayBuffer = 0x8892;
constexpr unsigned int kGlStaticDraw = 0x88E4;
constexpr unsigned int kGlDynamicDraw = 0x88E8;
constexpr unsigned int kGlFloat = 0x1406;
constexpr unsigned int kGlFalse = 0;
constexpr unsigned int kGlVertexShader = 0x8B31;
constexpr unsigned int kGlFragmentShader = 0x8B30;
constexpr unsigned int kGlCompileStatus = 0x8B81;
constexpr unsigned int kGlLinkStatus = 0x8B82;
constexpr unsigned int kGlTriangles = 0x0004;
constexpr unsigned int kGlBlend = 0x0BE2;
constexpr unsigned int kGlSrcAlpha = 0x0302;
constexpr unsigned int kGlOneMinusSrcAlpha = 0x0303;

constexpr float kSunDirX = 0.551f;
constexpr float kSunDirY = 0.701f;
constexpr float kSunDirZ = -0.451f;

constexpr const char *kMeshVertexShader = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uLightDir;
uniform vec3 uColorTint;
out vec3 vColor;
void main() {
    vec3 n = normalize(mat3(uModel) * aNormal);
    float ndl = max(dot(n, normalize(uLightDir)), 0.0);
    vColor = aColor * uColorTint * (0.22 + 0.78 * ndl);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

constexpr const char *kMeshFragmentShader = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

constexpr const char *kRaytraceVertexShader = R"(#version 330 core
const vec2 verts[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

out vec2 vUv;

void main() {
    vUv = verts[gl_VertexID] * 0.5 + 0.5;
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
}
)";

constexpr const char *kRaytraceFragmentShader = R"(#version 330 core
in vec2 vUv;
uniform float uAspect;
uniform vec3 uCameraPos;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform vec3 uCameraForward;
uniform float uTime;
out vec4 FragColor;

void main() {
    vec2 uv = vUv * 2.0 - 1.0;
    uv.x *= uAspect;

    vec3 rd = normalize(uCameraForward + uv.x * uCameraRight + uv.y * uCameraUp);
    float elevation = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    float horizon = exp(-abs(rd.y) * 7.0);
    vec3 lowerSky = vec3(0.58, 0.70, 0.88);
    vec3 upperSky = vec3(0.30, 0.58, 0.96);
    vec3 color = mix(lowerSky, upperSky, pow(elevation, 0.65));
    color = mix(color, vec3(0.78, 0.84, 0.92), horizon * 0.45);

    // Analytic sunlight evaluated along the camera ray. This mode traces only
    // atmospheric rays; it never invents a second floor or test geometry.
    vec3 sunDir = normalize(vec3(0.55, 0.70, -0.45));
    float sunCore = pow(max(dot(rd, sunDir), 0.0), 900.0);
    float sunGlow = pow(max(dot(rd, sunDir), 0.0), 18.0);
    color += vec3(1.0, 0.82, 0.48) * sunCore * 2.2;
    color += vec3(1.0, 0.70, 0.32) * sunGlow * 0.18;

    FragColor = vec4(color, 1.0);
}
)";

constexpr const char *kCrosshairVertexShader = R"(#version 330 core
const vec2 verts[24] = vec2[24](
    vec2(-10.0, -1.0), vec2(-3.0, -1.0), vec2(-3.0,  1.0),
    vec2(-10.0, -1.0), vec2(-3.0,  1.0), vec2(-10.0, 1.0),
    vec2(  3.0, -1.0), vec2(10.0, -1.0), vec2(10.0,  1.0),
    vec2(  3.0, -1.0), vec2(10.0,  1.0), vec2( 3.0, 1.0),
    vec2( -1.0,  3.0), vec2( 1.0,  3.0), vec2( 1.0, 10.0),
    vec2( -1.0,  3.0), vec2( 1.0, 10.0), vec2(-1.0, 10.0),
    vec2( -1.0,-10.0), vec2( 1.0,-10.0), vec2( 1.0, -3.0),
    vec2( -1.0,-10.0), vec2( 1.0, -3.0), vec2(-1.0, -3.0)
);
uniform vec2 uInvViewport;
uniform float uScale;
uniform float uVerticalOffset;
void main() {
    vec2 ndc = verts[gl_VertexID] * uInvViewport * uScale;
    gl_Position = vec4(ndc.x, ndc.y + uVerticalOffset, 0.0, 1.0);
}
)";

constexpr const char *kCrosshairFragmentShader = R"(#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

constexpr const char *kSandCloudVertexShader = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv;
layout (location = 2) in vec3 aColor;
layout (location = 3) in float aOpacity;
layout (location = 4) in float aSeed;
uniform mat4 uVP;
out vec2 vUv;
out vec3 vColor;
out float vOpacity;
out float vSeed;
void main() {
    vUv = aUv;
    vColor = aColor;
    vOpacity = aOpacity;
    vSeed = aSeed;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

constexpr const char *kSandCloudFragmentShader = R"(#version 330 core
in vec2 vUv;
in vec3 vColor;
in float vOpacity;
in float vSeed;
out vec4 FragColor;

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec2 centered = vUv * 2.0 - 1.0;
    float radius = length(centered);
    float softBody = exp(-radius * radius * 2.6);
    float fine = hash21(floor(vUv * 54.0 + vSeed));
    float wispy = hash21(floor(vUv * 13.0 + vSeed * 0.37));
    float alpha = softBody * (0.72 + wispy * 0.25) * vOpacity;
    alpha *= 1.0 - smoothstep(0.62, 1.0, radius);
    if (alpha < 0.008) discard;
    vec3 color = vColor * (0.78 + fine * 0.18 + wispy * 0.10);
    FragColor = vec4(color, alpha);
}
)";

constexpr const char *kSandRibbonVertexShader = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv;
uniform mat4 uVP;
out vec2 vUv;
out vec3 vWorldPos;
void main() {
    vUv = aUv;
    vWorldPos = aPos;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

constexpr const char *kSandRibbonFragmentShader = R"(#version 330 core
in vec2 vUv;
in vec3 vWorldPos;
uniform vec3 uColor;
uniform float uOpacity;
uniform float uTime;
out vec4 FragColor;

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

void main() {
    float across = abs(vUv.y * 2.0 - 1.0);
    float coarse = hash31(floor(vWorldPos * 15.0 + uTime * vec3(5.0, -3.0, 4.0)));
    float grains = hash31(floor(vWorldPos * 52.0 - uTime * vec3(9.0, 5.0, 7.0)));
    float raggedEdge = 0.74 + (coarse - 0.5) * 0.30;
    float body = 1.0 - smoothstep(raggedEdge - 0.22, raggedEdge, across);
    float longitudinalFade = smoothstep(0.0, 0.07, vUv.x) *
                             smoothstep(1.0, 0.88, vUv.x);
    float alpha = body * longitudinalFade * (0.68 + grains * 0.32) * uOpacity;
    if (alpha < 0.01) discard;
    vec3 color = uColor * (0.72 + coarse * 0.22 + grains * 0.10);
    FragColor = vec4(color, alpha);
}
)";

constexpr const char *kSkyVertexShader = R"(#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 vNdc;
void main() {
    vNdc = aPos;
    gl_Position = vec4(aPos, 0.999999, 1.0);
}
)";

constexpr const char *kSkyFragmentShader = R"(#version 330 core
in vec2 vNdc;
uniform vec3 uCameraPos;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform vec3 uCameraForward;
uniform float uTanHalfFov;
uniform float uAspect;
uniform vec3 uSunDir;
uniform float uSkyTime;
out vec4 FragColor;

float skyHash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float skyValueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = skyHash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = skyHash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = skyHash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = skyHash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = skyHash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = skyHash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = skyHash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = skyHash31(i + vec3(1.0, 1.0, 1.0));
    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z);
}

float skyFbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.55;
    for (int octave = 0; octave < 6; ++octave) {
        value += amplitude * skyValueNoise(p);
        p = p * 2.03 + vec3(17.0, 9.0, 23.0);
        amplitude *= 0.5;
    }
    return value;
}

float skyCloudDensity(vec3 dir) {
    if (dir.y <= 0.01) return 0.0;
    vec3 cloudPos = dir * 950.0;
    cloudPos.xz += uSkyTime * 14.0;
    cloudPos.y *= 0.55;
    float base = skyFbm(cloudPos * 0.0028);
    float detail = skyFbm(cloudPos * 0.011 + vec3(40.0, 0.0, 10.0));
    float density = base * 0.72 + detail * 0.28;
    density = smoothstep(0.46, 0.78, density);
    float heightFade = smoothstep(0.02, 0.18, dir.y);
    float horizonFade = pow(clamp(dir.y, 0.0, 1.0), 0.35);
    return density * heightFade * horizonFade;
}

vec3 renderSky(vec3 dir) {
    vec3 sunDir = normalize(uSunDir);
    float sunDot = max(dot(dir, sunDir), 0.0);
    vec3 zenith = vec3(0.18, 0.42, 0.88);
    vec3 mid = vec3(0.42, 0.64, 0.94);
    vec3 horizon = vec3(0.78, 0.82, 0.90);
    float elevation = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(horizon, mid, smoothstep(0.0, 0.45, elevation));
    sky = mix(sky, zenith, smoothstep(0.35, 1.0, elevation));
    sky += vec3(1.0, 0.82, 0.45) * pow(sunDot, 256.0) * 3.5;
    sky += vec3(1.0, 0.72, 0.35) * pow(sunDot, 32.0) * 0.45;
    sky += vec3(1.0, 0.95, 0.75) * smoothstep(0.9993, 0.99995, sunDot) * 6.0;
    float clouds = skyCloudDensity(dir);
    vec3 cloudLight = mix(vec3(0.88, 0.90, 0.94), vec3(1.0, 0.98, 0.92), sunDot);
    vec3 cloudShadow = vec3(0.62, 0.66, 0.74);
    vec3 cloudColor = mix(cloudShadow, cloudLight, 0.55 + sunDot * 0.45);
    sky = mix(sky, cloudColor, clouds * 0.94);
    sky = mix(sky, horizon, exp(-2.5 * max(0.0, 1.0 - dir.y)) * 0.12);
    return sky;
}

void main() {
    vec3 rayDir = normalize(
        uCameraForward +
        uCameraRight * vNdc.x * uAspect * uTanHalfFov +
        uCameraUp * vNdc.y * uTanHalfFov);
    FragColor = vec4(renderSky(rayDir), 1.0);
}
)";

void upload_mesh(unsigned int vbo, const std::vector<float> &vertices) {
  gl::bind_buffer(kGlArrayBuffer, vbo);
  gl::buffer_data(kGlArrayBuffer,
                  static_cast<long long>(vertices.size() * sizeof(float)),
                  vertices.data(),
                  kGlStaticDraw);
}

} // namespace

unsigned int Renderer::compile_shader(unsigned int type, const char *source) {
  const unsigned int shader = gl::create_shader(type);
  gl::shader_source(shader, 1, &source, nullptr);
  gl::compile_shader(shader);

  int success = 0;
  gl::get_shader_iv(shader, kGlCompileStatus, &success);
  if (!success) {
    std::vector<char> log(1024);
    gl::get_shader_info_log(shader, static_cast<int>(log.size()), nullptr, log.data());
    std::fprintf(stderr, "Shader compile error:\n%s\n", log.data());
    gl::delete_shader(shader);
    return 0;
  }

  return shader;
}

unsigned int Renderer::link_program(unsigned int vertex_shader, unsigned int fragment_shader) {
  const unsigned int program = gl::create_program();
  gl::attach_shader(program, vertex_shader);
  gl::attach_shader(program, fragment_shader);
  gl::link_program(program);

  int success = 0;
  gl::get_program_iv(program, kGlLinkStatus, &success);
  if (!success) {
    std::vector<char> log(1024);
    gl::get_program_info_log(program, static_cast<int>(log.size()), nullptr, log.data());
    std::fprintf(stderr, "Program link error:\n%s\n", log.data());
    return 0;
  }

  return program;
}

SceneTransforms Renderer::compute_scene_transforms(const SceneDrawParams &scene) const {
  SceneTransforms transforms{};
  transforms.dummy_model =
      Mat4::translate(kDummySpawnX, kDummySpawnY, kDummySpawnZ) *
      Mat4::rotate_y(kDummySpawnYaw);
  const float dummy_lean = scene.dummy_alive ? scene.dummy_hit_lean : 1.2f;
  transforms.dummy_body_local =
      Mat4::translate(0.0f, 0.55f, 0.0f) * Mat4::rotate_x(-dummy_lean) *
      Mat4::translate(0.0f, -0.55f, 0.0f);
  return transforms;
}

Mat4 Renderer::player_model_matrix(const SceneDrawParams &scene) const {
  // The Blender-authored Pharaoh uses the opposite yaw handedness from the
  // original samurai GLB. Keep camera/movement coordinates shared, but mirror
  // only the Pharaoh's visual root rotation so it turns with the camera.
  // Both imported characters now use the same world-facing convention. The
  // old Samurai-only sign reversal made him face opposite his travel vector.
  const float visual_yaw = -scene.player_yaw;
  return Mat4::translate(scene.player_x, scene.player_y, scene.player_z) *
         Mat4::rotate_y(visual_yaw);
}

bool Renderer::init() {
  if (!gl::load()) {
    return false;
  }

  const unsigned int mesh_vs = compile_shader(kGlVertexShader, kMeshVertexShader);
  const unsigned int mesh_fs = compile_shader(kGlFragmentShader, kMeshFragmentShader);
  const unsigned int sky_vs = compile_shader(kGlVertexShader, kSkyVertexShader);
  const unsigned int sky_fs = compile_shader(kGlFragmentShader, kSkyFragmentShader);
  const unsigned int ray_vs = compile_shader(kGlVertexShader, kRaytraceVertexShader);
  const unsigned int ray_fs = compile_shader(kGlFragmentShader, kRaytraceFragmentShader);
  const unsigned int crosshair_vs = compile_shader(kGlVertexShader, kCrosshairVertexShader);
  const unsigned int crosshair_fs = compile_shader(kGlFragmentShader, kCrosshairFragmentShader);
  const unsigned int sand_cloud_vs =
      compile_shader(kGlVertexShader, kSandCloudVertexShader);
  const unsigned int sand_cloud_fs =
      compile_shader(kGlFragmentShader, kSandCloudFragmentShader);
  const unsigned int sand_ribbon_vs =
      compile_shader(kGlVertexShader, kSandRibbonVertexShader);
  const unsigned int sand_ribbon_fs =
      compile_shader(kGlFragmentShader, kSandRibbonFragmentShader);

  if (!mesh_vs || !mesh_fs || !sky_vs || !sky_fs || !ray_vs || !ray_fs ||
      !crosshair_vs || !crosshair_fs || !sand_cloud_vs || !sand_cloud_fs ||
      !sand_ribbon_vs || !sand_ribbon_fs) {
    return false;
  }

  mesh_program_ = link_program(mesh_vs, mesh_fs);
  sky_program_ = link_program(sky_vs, sky_fs);
  raytrace_program_ = link_program(ray_vs, ray_fs);
  crosshair_program_ = link_program(crosshair_vs, crosshair_fs);
  sand_cloud_program_ = link_program(sand_cloud_vs, sand_cloud_fs);
  sand_ribbon_program_ = link_program(sand_ribbon_vs, sand_ribbon_fs);

  gl::delete_shader(mesh_vs);
  gl::delete_shader(mesh_fs);
  gl::delete_shader(sky_vs);
  gl::delete_shader(sky_fs);
  gl::delete_shader(ray_vs);
  gl::delete_shader(ray_fs);
  gl::delete_shader(crosshair_vs);
  gl::delete_shader(crosshair_fs);
  gl::delete_shader(sand_cloud_vs);
  gl::delete_shader(sand_cloud_fs);
  gl::delete_shader(sand_ribbon_vs);
  gl::delete_shader(sand_ribbon_fs);

  if (!mesh_program_ || !sky_program_ || !raytrace_program_ || !crosshair_program_ ||
      !sand_cloud_program_ || !sand_ribbon_program_) {
    return false;
  }

  if (!create_samurai_programs(skinned_program_, rigid_program_)) {
    std::fprintf(stderr, "Failed to create samurai shader programs\n");
    return false;
  }

  samurai_loaded_ = samurai_.load();
  if (!samurai_loaded_) {
    std::fprintf(stderr, "Failed to load the animated samurai player\n");
    return false;
  }
  pharaoh_loaded_ = pharaoh_.load("pharaoh_character.glb");
  remote_pharaoh_loaded_ = remote_pharaoh_.load("pharaoh_character.glb");
  // Center the projectile's FBX geometry around its origin. The source FBX was
  // cut from a full character and otherwise retains an offset behind its pivot.
  finger_projectile_loaded_ = finger_projectile_.load("finger_bullet.fbx", 0.045f);
  use_pharaoh_ = pharaoh_loaded_;
  std::fprintf(stderr, "Using %s player%s\n",
               use_pharaoh_ ? "animated Pharaoh" : "animated samurai",
               pharaoh_loaded_ ? " (press C to switch characters)" : "");

  mesh_mvp_location_ = gl::get_uniform_location(mesh_program_, "uMVP");
  mesh_model_location_ = gl::get_uniform_location(mesh_program_, "uModel");
  mesh_light_location_ = gl::get_uniform_location(mesh_program_, "uLightDir");
  mesh_color_tint_location_ = gl::get_uniform_location(mesh_program_, "uColorTint");

  sky_camera_pos_location_ = gl::get_uniform_location(sky_program_, "uCameraPos");
  sky_camera_right_location_ = gl::get_uniform_location(sky_program_, "uCameraRight");
  sky_camera_up_location_ = gl::get_uniform_location(sky_program_, "uCameraUp");
  sky_camera_forward_location_ = gl::get_uniform_location(sky_program_, "uCameraForward");
  sky_tan_half_fov_location_ = gl::get_uniform_location(sky_program_, "uTanHalfFov");
  sky_aspect_location_ = gl::get_uniform_location(sky_program_, "uAspect");
  sky_sun_dir_location_ = gl::get_uniform_location(sky_program_, "uSunDir");
  sky_time_location_ = gl::get_uniform_location(sky_program_, "uSkyTime");
  ray_aspect_location_ = gl::get_uniform_location(raytrace_program_, "uAspect");
  ray_camera_pos_location_ = gl::get_uniform_location(raytrace_program_, "uCameraPos");
  ray_camera_right_location_ = gl::get_uniform_location(raytrace_program_, "uCameraRight");
  ray_camera_up_location_ = gl::get_uniform_location(raytrace_program_, "uCameraUp");
  ray_camera_forward_location_ = gl::get_uniform_location(raytrace_program_, "uCameraForward");
  ray_time_location_ = gl::get_uniform_location(raytrace_program_, "uTime");
  crosshair_inv_viewport_location_ =
      gl::get_uniform_location(crosshair_program_, "uInvViewport");
  crosshair_scale_location_ = gl::get_uniform_location(crosshair_program_, "uScale");
  crosshair_color_location_ = gl::get_uniform_location(crosshair_program_, "uColor");
  crosshair_vertical_offset_location_ =
      gl::get_uniform_location(crosshair_program_, "uVerticalOffset");
  sand_cloud_vp_location_ = gl::get_uniform_location(sand_cloud_program_, "uVP");
  sand_cloud_center_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uCenter");
  sand_cloud_right_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uCameraRight");
  sand_cloud_up_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uCameraUp");
  sand_cloud_size_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uSize");
  sand_cloud_color_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uColor");
  sand_cloud_opacity_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uOpacity");
  sand_cloud_seed_location_ =
      gl::get_uniform_location(sand_cloud_program_, "uSeed");
  sand_ribbon_vp_location_ =
      gl::get_uniform_location(sand_ribbon_program_, "uVP");
  sand_ribbon_color_location_ =
      gl::get_uniform_location(sand_ribbon_program_, "uColor");
  sand_ribbon_opacity_location_ =
      gl::get_uniform_location(sand_ribbon_program_, "uOpacity");
  sand_ribbon_time_location_ =
      gl::get_uniform_location(sand_ribbon_program_, "uTime");

  vao_ = gl::create_vertex_array();
  gl::bind_vertex_array(vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);
  gl::enable_vertex_attrib_array(2);

  sky_vao_ = gl::create_vertex_array();
  gl::bind_vertex_array(sky_vao_);
  gl::enable_vertex_attrib_array(0);

  const float sky_triangle[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
  gl::gen_buffers(1, &sky_vbo_);
  gl::bind_buffer(kGlArrayBuffer, sky_vbo_);
  gl::buffer_data(kGlArrayBuffer, static_cast<long long>(sizeof(sky_triangle)), sky_triangle,
                  kGlStaticDraw);
  gl::vertex_attrib_pointer(0, 2, kGlFloat, kGlFalse, 2 * static_cast<int>(sizeof(float)), nullptr);

  sand_cloud_vao_ = gl::create_vertex_array();
  gl::bind_vertex_array(sand_cloud_vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);
  gl::enable_vertex_attrib_array(2);
  gl::enable_vertex_attrib_array(3);
  gl::enable_vertex_attrib_array(4);
  gl::gen_buffers(1, &sand_cloud_vbo_);
  gl::bind_buffer(kGlArrayBuffer, sand_cloud_vbo_);
  gl::buffer_data(kGlArrayBuffer, 1, nullptr, kGlDynamicDraw);
  constexpr int cloud_stride = 10 * static_cast<int>(sizeof(float));
  gl::vertex_attrib_pointer(0, 3, kGlFloat, kGlFalse, cloud_stride, nullptr);
  gl::vertex_attrib_pointer(1, 2, kGlFloat, kGlFalse, cloud_stride,
                            reinterpret_cast<const void *>(3 * sizeof(float)));
  gl::vertex_attrib_pointer(2, 3, kGlFloat, kGlFalse, cloud_stride,
                            reinterpret_cast<const void *>(5 * sizeof(float)));
  gl::vertex_attrib_pointer(3, 1, kGlFloat, kGlFalse, cloud_stride,
                            reinterpret_cast<const void *>(8 * sizeof(float)));
  gl::vertex_attrib_pointer(4, 1, kGlFloat, kGlFalse, cloud_stride,
                            reinterpret_cast<const void *>(9 * sizeof(float)));

  sand_ribbon_vao_ = gl::create_vertex_array();
  gl::bind_vertex_array(sand_ribbon_vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);
  gl::gen_buffers(1, &sand_ribbon_vbo_);
  gl::bind_buffer(kGlArrayBuffer, sand_ribbon_vbo_);
  gl::buffer_data(kGlArrayBuffer, 1, nullptr, kGlDynamicDraw);
  gl::vertex_attrib_pointer(0, 3, kGlFloat, kGlFalse,
                            5 * static_cast<int>(sizeof(float)), nullptr);
  gl::vertex_attrib_pointer(1, 2, kGlFloat, kGlFalse,
                            5 * static_cast<int>(sizeof(float)),
                            reinterpret_cast<const void *>(3 * sizeof(float)));

  const std::vector<float> arena_vertices = build_arena();
  const std::vector<float> dummy_post_vertices = build_dummy_post();
  const std::vector<float> dummy_body_vertices = build_dummy_body();
  std::vector<float> sand_particle_vertices;
  append_box(sand_particle_vertices, 0.0f, 0.0f, 0.0f,
             0.5f, 0.5f, 0.5f, 0.88f, 0.66f, 0.24f);

  arena_vertex_count_ = static_cast<int>(arena_vertices.size() / kVertexFloats);
  dummy_post_vertex_count_ = static_cast<int>(dummy_post_vertices.size() / kVertexFloats);
  dummy_body_vertex_count_ = static_cast<int>(dummy_body_vertices.size() / kVertexFloats);
  sand_particle_vertex_count_ =
      static_cast<int>(sand_particle_vertices.size() / kVertexFloats);

  gl::gen_buffers(1, &arena_vbo_);
  gl::gen_buffers(1, &dummy_post_vbo_);
  gl::gen_buffers(1, &dummy_body_vbo_);
  gl::gen_buffers(1, &sand_particle_vbo_);

  upload_mesh(arena_vbo_, arena_vertices);
  upload_mesh(dummy_post_vbo_, dummy_post_vertices);
  upload_mesh(dummy_body_vbo_, dummy_body_vertices);
  upload_mesh(sand_particle_vbo_, sand_particle_vertices);

  gl::enable(kGlDepthTest);
  gl::depth_func(kGlLess);
  gl::disable_cull_face();
  gl::clear_color(0.0f, 0.0f, 0.0f, 1.0f);

  return true;
}

void Renderer::resize(int width, int height) {
  width_ = width > 0 ? width : 1;
  height_ = height > 0 ? height : 1;
  gl::viewport(0, 0, width_, height_);
}

void Renderer::bind_mesh_vbo(unsigned int vbo) {
  gl::bind_buffer(kGlArrayBuffer, vbo);
  constexpr int stride = kVertexFloats * static_cast<int>(sizeof(float));
  gl::vertex_attrib_pointer(0, 3, kGlFloat, kGlFalse, stride, nullptr);
  gl::vertex_attrib_pointer(1, 3, kGlFloat, kGlFalse, stride,
                            reinterpret_cast<const void *>(3 * sizeof(float)));
  gl::vertex_attrib_pointer(2, 3, kGlFloat, kGlFalse, stride,
                            reinterpret_cast<const void *>(6 * sizeof(float)));
}

void Renderer::draw_lit_mesh(const Mat4 &mvp, const Mat4 &model, unsigned int vbo,
                             int vertex_count, float tint_r, float tint_g,
                             float tint_b) {
  bind_mesh_vbo(vbo);
  gl::uniform_matrix4fv(mesh_mvp_location_, 1, kGlFalse, mvp.m);
  gl::uniform_matrix4fv(mesh_model_location_, 1, kGlFalse, model.m);
  gl::uniform3f(mesh_light_location_, kSunDirX, kSunDirY, kSunDirZ);
  gl::uniform3f(mesh_color_tint_location_, tint_r, tint_g, tint_b);
  gl::draw_arrays(kGlTriangles, 0, vertex_count);
}

float Renderer::next_sand_random() {
  sand_random_state_ = sand_random_state_ * 1664525u + 1013904223u;
  return static_cast<float>((sand_random_state_ >> 8) & 0x00FFFFFFu) /
         static_cast<float>(0x01000000u);
}

void Renderer::assign_sand_shade(SandParticle &particle) {
  particle.cloud = true;
  particle.cloud_seed = next_sand_random() * 97.0f;
  const int shade = std::min(4, static_cast<int>(next_sand_random() * 5.0f));
  switch (shade) {
  case 0: // sunlit pale sand
    particle.tint_r = 0.92f;
    particle.tint_g = 0.82f;
    particle.tint_b = 0.58f;
    break;
  case 1: // rich gold
    particle.tint_r = 0.84f;
    particle.tint_g = 0.73f;
    particle.tint_b = 0.50f;
    break;
  case 2: // natural tan
    particle.tint_r = 0.78f;
    particle.tint_g = 0.68f;
    particle.tint_b = 0.46f;
    break;
  case 3: // warm amber
    particle.tint_r = 0.72f;
    particle.tint_g = 0.61f;
    particle.tint_b = 0.40f;
    break;
  default: // darker grains add depth between brighter particles
    particle.tint_r = 0.62f;
    particle.tint_g = 0.52f;
    particle.tint_b = 0.34f;
    break;
  }
}

void Renderer::update_sand_trail(const SceneDrawParams &scene) {
  const float delta_time = std::min(scene.delta_time, 0.05f);
  for (SandParticle &particle : sand_particles_) {
    particle.age += delta_time;
    if (particle.behavior == 0) {
      particle.vy -= 5.0f * delta_time;
      particle.x += particle.vx * delta_time;
      particle.y += particle.vy * delta_time;
      particle.z += particle.vz * delta_time;
      if (particle.y <= 0.0f) {
        particle.age = particle.lifetime;
      }
    }
  }
  sand_particles_.erase(
      std::remove_if(sand_particles_.begin(), sand_particles_.end(),
                     [](const SandParticle &particle) {
                       return particle.age >= particle.lifetime;
                     }),
      sand_particles_.end());

  if (!scene.finger_projectile_active) {
    had_active_projectile_ = false;
    sand_emit_accumulator_ = 0.0f;
    return;
  }

  const float cos_pitch = std::cos(scene.finger_projectile_pitch);
  const float direction_x = std::sin(scene.finger_projectile_yaw) * cos_pitch;
  const float direction_y = std::sin(scene.finger_projectile_pitch);
  const float direction_z = std::cos(scene.finger_projectile_yaw) * cos_pitch;

  if (!had_active_projectile_) {
    last_projectile_x_ = scene.finger_projectile_x;
    last_projectile_y_ = scene.finger_projectile_y;
    last_projectile_z_ = scene.finger_projectile_z;
    had_active_projectile_ = true;
  }

  constexpr float particles_per_second = 1200.0f;
  sand_emit_accumulator_ += delta_time * particles_per_second;
  const int emit_count = static_cast<int>(sand_emit_accumulator_);
  sand_emit_accumulator_ -= static_cast<float>(emit_count);

  for (int index = 0; index < emit_count && sand_particles_.size() < 1800; ++index) {
    const float t = static_cast<float>(index + 1) /
                    static_cast<float>(std::max(emit_count, 1));
    const float base_x = last_projectile_x_ +
                         (scene.finger_projectile_x - last_projectile_x_) * t;
    const float base_y = last_projectile_y_ +
                         (scene.finger_projectile_y - last_projectile_y_) * t;
    const float base_z = last_projectile_z_ +
                         (scene.finger_projectile_z - last_projectile_z_) * t;
    const float jitter_x = (next_sand_random() - 0.5f) * 0.10f;
    const float jitter_y = (next_sand_random() - 0.5f) * 0.10f;
    const float jitter_z = (next_sand_random() - 0.5f) * 0.10f;
    const float backward_speed = 1.2f + next_sand_random() * 1.8f;

    SandParticle particle{};
    particle.x = base_x - direction_x * 0.055f + jitter_x;
    particle.y = base_y - direction_y * 0.055f + jitter_y;
    particle.z = base_z - direction_z * 0.055f + jitter_z;
    particle.vx = -direction_x * backward_speed + jitter_x * 7.0f;
    particle.vy = -direction_y * backward_speed - 0.35f + jitter_y * 5.0f;
    particle.vz = -direction_z * backward_speed + jitter_z * 7.0f;
    particle.lifetime = 0.45f + next_sand_random() * 0.35f;
    particle.size = 0.010f + next_sand_random() * 0.015f;
    assign_sand_shade(particle);
    sand_particles_.push_back(particle);
  }

  last_projectile_x_ = scene.finger_projectile_x;
  last_projectile_y_ = scene.finger_projectile_y;
  last_projectile_z_ = scene.finger_projectile_z;
}

void Renderer::update_staff_summon(const SceneDrawParams &scene) {
  const float delta_time = std::min(scene.delta_time, 0.05f);
  const bool staff_action = scene.swinging || scene.ultimate;
  const bool just_started = use_pharaoh_ && staff_action && !staff_was_swinging_;
  const bool just_ended = use_pharaoh_ && !staff_action && staff_was_swinging_;
  if (just_ended && !scene.player_hidden) {
    staff_sand_dismiss_time_ = 0.32f;
  } else if (just_ended) {
    staff_sand_dismiss_time_ = 0.0f;
  } else {
    staff_sand_dismiss_time_ = std::max(0.0f, staff_sand_dismiss_time_ - delta_time);
  }

  const float staff_progress = scene.ultimate ? scene.ultimate_progress : scene.attack_progress;
  const bool summoning = use_pharaoh_ && staff_action && staff_progress < 0.36f;
  const bool dismissing = use_pharaoh_ && !scene.player_hidden && !staff_action &&
                          staff_sand_dismiss_time_ > 0.0f;
  staff_was_swinging_ = use_pharaoh_ && staff_action;

  float start_x = 0.0f;
  float start_y = 0.0f;
  float start_z = 0.0f;
  float end_x = 0.0f;
  float end_y = 0.0f;
  float end_z = 0.0f;
  const bool has_staff_segment = use_pharaoh_ &&
      pharaoh_.staff_segment_world(player_model_matrix(scene),
                                   start_x, start_y, start_z,
                                   end_x, end_y, end_z);

  // Directed grains rise from the floor and settle onto the moving shaft.
  // On dismissal their recorded path runs the other way, back to the floor.
  for (SandParticle &particle : sand_particles_) {
    if (particle.behavior == 1 && has_staff_segment) {
      const float target_x = start_x + (end_x - start_x) * particle.staff_along +
                             particle.staff_offset_x;
      const float target_y = start_y + (end_y - start_y) * particle.staff_along +
                             particle.staff_offset_y;
      const float target_z = start_z + (end_z - start_z) * particle.staff_along +
                             particle.staff_offset_z;
      float travel = std::clamp(particle.age / particle.travel_time, 0.0f, 1.0f);
      travel = travel * travel * (3.0f - 2.0f * travel);
      particle.x = particle.origin_x + (target_x - particle.origin_x) * travel;
      particle.y = particle.origin_y + (target_y - particle.origin_y) * travel +
                   std::sin(travel * 3.1415926535f) * 0.10f;
      particle.z = particle.origin_z + (target_z - particle.origin_z) * travel;
      const float swirl_angle = particle.cloud_seed + particle.age * 22.0f +
                                particle.staff_along * 12.0f;
      const float swirl_radius = 0.025f + (1.0f - travel) * 0.13f;
      particle.x += std::cos(swirl_angle) * swirl_radius;
      particle.z += std::sin(swirl_angle) * swirl_radius;
    } else if (particle.behavior == 2) {
      float travel = std::clamp(particle.age / particle.travel_time, 0.0f, 1.0f);
      travel = travel * travel * (3.0f - 2.0f * travel);
      particle.x = particle.origin_x +
                   (particle.destination_x - particle.origin_x) * travel;
      particle.y = particle.origin_y +
                   (particle.destination_y - particle.origin_y) * travel +
                   std::sin(travel * 3.1415926535f) * 0.06f;
      particle.z = particle.origin_z +
                   (particle.destination_z - particle.origin_z) * travel;
      const float swirl_angle = particle.cloud_seed + particle.age * 18.0f;
      const float swirl_radius = std::sin(travel * 3.1415926535f) * 0.10f;
      particle.x += std::cos(swirl_angle) * swirl_radius;
      particle.z += std::sin(swirl_angle) * swirl_radius;
    }
  }

  if (!summoning && !dismissing) {
    staff_sand_emit_accumulator_ = 0.0f;
    return;
  }
  if (!has_staff_segment) {
    return;
  }

  staff_sand_emit_accumulator_ += delta_time * (summoning ? 2600.0f : 1700.0f);
  int emit_count = static_cast<int>(staff_sand_emit_accumulator_);
  staff_sand_emit_accumulator_ -= static_cast<float>(emit_count);
  if (just_started) {
    emit_count += 650;
  } else if (just_ended) {
    emit_count += 360;
  }

  const float staff_x = end_x - start_x;
  const float staff_y = end_y - start_y;
  const float staff_z = end_z - start_z;
  for (int index = 0; index < emit_count && sand_particles_.size() < 3200; ++index) {
    const float along = next_sand_random();
    const float sleeve_width = summoning ? 0.12f : 0.07f;
    const float jitter_x = (next_sand_random() - 0.5f) * sleeve_width;
    const float jitter_y = (next_sand_random() - 0.5f) * sleeve_width;
    const float jitter_z = (next_sand_random() - 0.5f) * sleeve_width;
    const float target_x = start_x + staff_x * along + jitter_x;
    const float target_y = start_y + staff_y * along + jitter_y;
    const float target_z = start_z + staff_z * along + jitter_z;

    SandParticle particle{};
    particle.staff_along = along;
    particle.staff_offset_x = jitter_x;
    particle.staff_offset_y = jitter_y;
    particle.staff_offset_z = jitter_z;
    particle.size = (summoning ? 0.014f : 0.012f) +
                    next_sand_random() * (summoning ? 0.022f : 0.018f);
    assign_sand_shade(particle);
    if (summoning) {
      particle.behavior = 1;
      particle.origin_x = target_x + (next_sand_random() - 0.5f) * 0.75f;
      particle.origin_y = 0.018f;
      particle.origin_z = target_z + (next_sand_random() - 0.5f) * 0.75f;
      particle.x = particle.origin_x;
      particle.y = particle.origin_y;
      particle.z = particle.origin_z;
      particle.travel_time = 0.075f + next_sand_random() * 0.105f;
      particle.lifetime = particle.travel_time + 0.34f + next_sand_random() * 0.16f;
    } else {
      particle.behavior = 2;
      particle.origin_x = target_x;
      particle.origin_y = target_y;
      particle.origin_z = target_z;
      particle.destination_x = target_x + (next_sand_random() - 0.5f) * 0.65f;
      particle.destination_y = 0.018f;
      particle.destination_z = target_z + (next_sand_random() - 0.5f) * 0.65f;
      particle.x = particle.origin_x;
      particle.y = particle.origin_y;
      particle.z = particle.origin_z;
      particle.travel_time = 0.18f + next_sand_random() * 0.17f;
      particle.lifetime = particle.travel_time + 0.06f;
    }
    sand_particles_.push_back(particle);
  }
}

void Renderer::update_ultimate_sand(const SceneDrawParams &scene) {
  if (!use_pharaoh_) {
    return;
  }

  const bool return_started = scene.ultimate_returning && !ultimate_was_returning_;
  ultimate_was_returning_ = scene.ultimate_returning;
  if (scene.ultimate_returning) {
    ultimate_return_emit_accumulator_ += scene.delta_time * 1700.0f;
    int return_count = static_cast<int>(ultimate_return_emit_accumulator_);
    ultimate_return_emit_accumulator_ -= static_cast<float>(return_count);
    if (return_started) {
      return_count += 760;
    }
    for (int index = 0; index < return_count && sand_particles_.size() < 5200; ++index) {
      const float angle = next_sand_random() * 6.283185307f;
      const float body_radius = 0.10f + next_sand_random() * 0.28f;
      const float reveal_height = 0.10f +
          1.48f * std::clamp(scene.ultimate_return_progress + 0.12f, 0.0f, 1.0f);
      SandParticle particle{};
      particle.behavior = 2;
      particle.origin_x = scene.player_x + std::cos(angle) *
                          (0.30f + next_sand_random() * 0.75f);
      particle.origin_y = scene.player_y + 0.018f;
      particle.origin_z = scene.player_z + std::sin(angle) *
                          (0.30f + next_sand_random() * 0.75f);
      particle.destination_x = scene.player_x + std::cos(angle) * body_radius;
      particle.destination_y = scene.player_y + 0.04f +
                               next_sand_random() * reveal_height;
      particle.destination_z = scene.player_z + std::sin(angle) * body_radius;
      particle.x = particle.origin_x;
      particle.y = particle.origin_y;
      particle.z = particle.origin_z;
      particle.travel_time = 0.20f + next_sand_random() * 0.28f;
      particle.lifetime = particle.travel_time + 0.18f + next_sand_random() * 0.18f;
      particle.size = 0.010f + next_sand_random() * 0.020f;
      assign_sand_shade(particle);
      sand_particles_.push_back(particle);
    }
  } else {
    ultimate_return_emit_accumulator_ = 0.0f;
  }

  // Pull a smaller stream upward during the power-up so the eventual blast
  // has a visible charge rather than appearing from an empty frame.
  if (scene.ultimate && scene.ultimate_progress < 0.90f) {
    const int charge_count = std::max(1, static_cast<int>(scene.delta_time * 1500.0f));
    for (int index = 0; index < charge_count && sand_particles_.size() < 3600; ++index) {
      const float angle = next_sand_random() * 6.283185307f;
      const float radius = 0.25f + next_sand_random() * 1.10f;
      const float inward_speed = 0.45f + next_sand_random() * 0.65f;
      const float swirl_speed = 2.4f + next_sand_random() * 2.8f;
      SandParticle particle{};
      particle.x = scene.player_x + std::cos(angle) * radius;
      particle.y = scene.player_y + 0.025f;
      particle.z = scene.player_z + std::sin(angle) * radius;
      particle.vx = -std::cos(angle) * inward_speed -
                    std::sin(angle) * swirl_speed;
      particle.vy = 3.2f + next_sand_random() * 2.8f;
      particle.vz = -std::sin(angle) * inward_speed +
                    std::cos(angle) * swirl_speed;
      particle.lifetime = 0.55f + next_sand_random() * 0.35f;
      particle.size = 0.010f + next_sand_random() * 0.018f;
      assign_sand_shade(particle);
      sand_particles_.push_back(particle);
    }
  }

  if (scene.ultimate_blast) {
    ultimate_rush_time_ = 0.32f;
    ultimate_rush_emit_accumulator_ = 0.0f;
  }

  // Keep a very short outward rush alive only while the body is dissolving.
  // It stops with the 0.32-second disappearance instead of becoming a
  // sustained sand fountain.
  if (ultimate_rush_time_ > 0.0f && !scene.player_hidden &&
      scene.player_dissolve < 0.999f) {
    const float delta_time = std::min(scene.delta_time, 0.05f);
    ultimate_rush_time_ = std::max(0.0f, ultimate_rush_time_ - delta_time);
    ultimate_rush_emit_accumulator_ += delta_time * 12000.0f;
    const int rush_count = static_cast<int>(ultimate_rush_emit_accumulator_);
    ultimate_rush_emit_accumulator_ -= static_cast<float>(rush_count);
    for (int index = 0; index < rush_count && sand_particles_.size() < 12000; ++index) {
      const float angle = next_sand_random() * 6.283185307f;
      const float speed = 27.0f + next_sand_random() * 8.0f;
      const float curl = (next_sand_random() - 0.5f) * 12.0f;
      SandParticle particle{};
      particle.x = scene.player_x + (next_sand_random() - 0.5f) * 0.35f;
      particle.y = scene.player_y + 0.05f + next_sand_random() * 1.42f;
      particle.z = scene.player_z + (next_sand_random() - 0.5f) * 0.35f;
      particle.vx = std::cos(angle) * speed - std::sin(angle) * curl;
      particle.vy = 1.5f + next_sand_random() * 7.0f;
      particle.vz = std::sin(angle) * speed + std::cos(angle) * curl;
      particle.lifetime = 0.34f + next_sand_random() * 0.22f;
      particle.size = 0.0035f + next_sand_random() * 0.0090f;
      assign_sand_shade(particle);
      sand_particles_.push_back(particle);
    }

  } else if (scene.player_hidden || scene.player_dissolve >= 0.999f) {
    ultimate_rush_time_ = 0.0f;
    ultimate_rush_emit_accumulator_ = 0.0f;
  }

  if (!scene.ultimate_blast) {
    return;
  }

  // One compact burst replaces the former continuous stream. Its short-lived,
  // high-speed grains cover the rapid dissolve and read as a sand explosion.
  for (int index = 0; index < 9000 && sand_particles_.size() < 12000; ++index) {
    const float angle = next_sand_random() * 6.283185307f;
    const float radial_speed = 28.0f + next_sand_random() * 7.0f;
    const float curl = (next_sand_random() - 0.5f) * 14.0f;
    const float initial_radius = next_sand_random() * 0.32f;
    SandParticle particle{};
    particle.x = scene.player_x + std::cos(angle) * initial_radius;
    particle.y = scene.player_y + 0.06f + next_sand_random() * 1.45f;
    particle.z = scene.player_z + std::sin(angle) * initial_radius;
    particle.vx = std::cos(angle) * radial_speed - std::sin(angle) * curl;
    particle.vy = 2.0f + next_sand_random() * 8.0f;
    particle.vz = std::sin(angle) * radial_speed + std::cos(angle) * curl;
    particle.lifetime = 0.42f + next_sand_random() * 0.28f;
    particle.size = 0.0030f + next_sand_random() * 0.0095f;
    assign_sand_shade(particle);
    sand_particles_.push_back(particle);
  }

}

void Renderer::draw_sand_trail(const Mat4 &projection, const Mat4 &view,
                               const OrbitCamera &camera) {
  if (sand_particles_.empty()) {
    return;
  }
  gl::use_program(mesh_program_);
  gl::bind_vertex_array(vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);
  gl::enable_vertex_attrib_array(2);
  const Mat4 vp = projection * view;
  for (const SandParticle &particle : sand_particles_) {
    if (particle.cloud) {
      continue;
    }
    const float life_remaining =
        std::clamp(1.0f - particle.age / particle.lifetime, 0.0f, 1.0f);
    const float size = particle.size * (0.30f + 0.70f * life_remaining);
    const Mat4 model = Mat4::translate(particle.x, particle.y, particle.z) *
                       Mat4::scale(size, size, size);
    draw_lit_mesh(vp * model, model, sand_particle_vbo_, sand_particle_vertex_count_,
                  particle.tint_r, particle.tint_g, particle.tint_b);
  }

  std::vector<const SandParticle *> clouds;
  clouds.reserve(sand_particles_.size());
  for (const SandParticle &particle : sand_particles_) {
    if (particle.cloud) {
      clouds.push_back(&particle);
    }
  }
  if (clouds.empty()) {
    return;
  }
  std::sort(clouds.begin(), clouds.end(), [&camera](const SandParticle *a,
                                                    const SandParticle *b) {
    const float adx = a->x - camera.x;
    const float ady = a->y - camera.y;
    const float adz = a->z - camera.z;
    const float bdx = b->x - camera.x;
    const float bdy = b->y - camera.y;
    const float bdz = b->z - camera.z;
    return adx * adx + ady * ady + adz * adz >
           bdx * bdx + bdy * bdy + bdz * bdz;
  });

  gl::use_program(sand_cloud_program_);
  gl::bind_vertex_array(sand_cloud_vao_);
  gl::uniform_matrix4fv(sand_cloud_vp_location_, 1, kGlFalse, vp.m);
  gl::enable(kGlBlend);
  gl::blend_func(kGlSrcAlpha, kGlOneMinusSrcAlpha);
  gl::depth_mask(kGlFalse);

  // Build every camera-facing wisp into one dynamic vertex batch. This keeps
  // the dense five-figure explosion to a single GPU draw call.
  std::vector<float> cloud_vertices;
  cloud_vertices.reserve(clouds.size() * 6 * 10);
  const auto push_cloud_vertex = [&](float x, float y, float z, float u, float v,
                                     const SandParticle &particle, float opacity,
                                     float seed) {
    cloud_vertices.push_back(x);
    cloud_vertices.push_back(y);
    cloud_vertices.push_back(z);
    cloud_vertices.push_back(u);
    cloud_vertices.push_back(v);
    cloud_vertices.push_back(particle.tint_r);
    cloud_vertices.push_back(particle.tint_g);
    cloud_vertices.push_back(particle.tint_b);
    cloud_vertices.push_back(opacity);
    cloud_vertices.push_back(seed);
  };
  for (const SandParticle *particle : clouds) {
    const float life_remaining =
        std::clamp(1.0f - particle->age / particle->lifetime, 0.0f, 1.0f);
    const float fade_in = std::clamp(particle->age / 0.07f, 0.0f, 1.0f);
    const float fade_out = std::clamp(life_remaining / 0.24f, 0.0f, 1.0f);
    const float behavior_opacity = particle->behavior == 0 ? 0.72f : 0.90f;
    const float opacity = behavior_opacity * fade_in * fade_out;
    const float size = particle->size * 2.6f *
                       (0.72f + 0.28f * life_remaining);
    const float rx = camera.right_x * size;
    const float ry = camera.right_y * size;
    const float rz = camera.right_z * size;
    const float ux = camera.up_x * size;
    const float uy = camera.up_y * size;
    const float uz = camera.up_z * size;
    const float seed = particle->cloud_seed + particle->age * 8.0f;
    const float ax = particle->x - rx - ux;
    const float ay = particle->y - ry - uy;
    const float az = particle->z - rz - uz;
    const float bx = particle->x + rx - ux;
    const float by = particle->y + ry - uy;
    const float bz = particle->z + rz - uz;
    const float cx = particle->x + rx + ux;
    const float cy = particle->y + ry + uy;
    const float cz = particle->z + rz + uz;
    const float dx = particle->x - rx + ux;
    const float dy = particle->y - ry + uy;
    const float dz = particle->z - rz + uz;
    push_cloud_vertex(ax, ay, az, 0.0f, 0.0f, *particle, opacity, seed);
    push_cloud_vertex(bx, by, bz, 1.0f, 0.0f, *particle, opacity, seed);
    push_cloud_vertex(cx, cy, cz, 1.0f, 1.0f, *particle, opacity, seed);
    push_cloud_vertex(ax, ay, az, 0.0f, 0.0f, *particle, opacity, seed);
    push_cloud_vertex(cx, cy, cz, 1.0f, 1.0f, *particle, opacity, seed);
    push_cloud_vertex(dx, dy, dz, 0.0f, 1.0f, *particle, opacity, seed);
  }
  gl::bind_buffer(kGlArrayBuffer, sand_cloud_vbo_);
  gl::buffer_data(kGlArrayBuffer,
                  static_cast<long long>(cloud_vertices.size() * sizeof(float)),
                  cloud_vertices.data(), kGlDynamicDraw);
  gl::draw_arrays(kGlTriangles, 0, static_cast<int>(cloud_vertices.size() / 10));
  gl::depth_mask(1);
  gl::disable(kGlBlend);
}

void Renderer::draw_sand_cloud_ribbons(const Mat4 &projection, const Mat4 &view,
                                       const OrbitCamera &camera,
                                       const SceneDrawParams &scene) {
  struct RibbonPoint {
    float x, y, z, width;
  };
  const Mat4 vp = projection * view;
  gl::use_program(sand_ribbon_program_);
  gl::bind_vertex_array(sand_ribbon_vao_);
  gl::uniform_matrix4fv(sand_ribbon_vp_location_, 1, kGlFalse, vp.m);
  gl::uniform1f(sand_ribbon_time_location_, scene.time);
  gl::enable(kGlBlend);
  gl::blend_func(kGlSrcAlpha, kGlOneMinusSrcAlpha);
  gl::depth_mask(kGlFalse);

  const auto draw_curve = [&](const std::vector<RibbonPoint> &points,
                              float red, float green, float blue,
                              float opacity) {
    if (points.size() < 2) {
      return;
    }
    std::vector<float> vertices;
    vertices.reserve((points.size() - 1) * 6 * 5);
    const auto edge = [&](std::size_t index, float sign, float &x, float &y, float &z) {
      const RibbonPoint &point = points[index];
      const RibbonPoint &before = points[index > 0 ? index - 1 : index];
      const RibbonPoint &after = points[index + 1 < points.size() ? index + 1 : index];
      float tx = after.x - before.x;
      float ty = after.y - before.y;
      float tz = after.z - before.z;
      const float tangent_length = std::max(std::sqrt(tx * tx + ty * ty + tz * tz), 0.001f);
      tx /= tangent_length;
      ty /= tangent_length;
      tz /= tangent_length;
      float sx = ty * camera.forward_z - tz * camera.forward_y;
      float sy = tz * camera.forward_x - tx * camera.forward_z;
      float sz = tx * camera.forward_y - ty * camera.forward_x;
      float side_length = std::sqrt(sx * sx + sy * sy + sz * sz);
      if (side_length < 0.001f) {
        sx = camera.right_x;
        sy = camera.right_y;
        sz = camera.right_z;
        side_length = 1.0f;
      }
      sx /= side_length;
      sy /= side_length;
      sz /= side_length;
      x = point.x + sx * point.width * sign;
      y = point.y + sy * point.width * sign;
      z = point.z + sz * point.width * sign;
    };
    const auto push = [&](float x, float y, float z, float u, float v) {
      vertices.push_back(x);
      vertices.push_back(y);
      vertices.push_back(z);
      vertices.push_back(u);
      vertices.push_back(v);
    };
    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
      float ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz;
      edge(index, -1.0f, ax, ay, az);
      edge(index, 1.0f, bx, by, bz);
      edge(index + 1, -1.0f, cx, cy, cz);
      edge(index + 1, 1.0f, dx, dy, dz);
      const float u0 = static_cast<float>(index) /
                       static_cast<float>(points.size() - 1);
      const float u1 = static_cast<float>(index + 1) /
                       static_cast<float>(points.size() - 1);
      push(ax, ay, az, u0, 0.0f);
      push(bx, by, bz, u0, 1.0f);
      push(cx, cy, cz, u1, 0.0f);
      push(cx, cy, cz, u1, 0.0f);
      push(bx, by, bz, u0, 1.0f);
      push(dx, dy, dz, u1, 1.0f);
    }
    gl::bind_buffer(kGlArrayBuffer, sand_ribbon_vbo_);
    gl::buffer_data(kGlArrayBuffer,
                    static_cast<long long>(vertices.size() * sizeof(float)),
                    vertices.data(), kGlDynamicDraw);
    gl::uniform3f(sand_ribbon_color_location_, red, green, blue);
    gl::uniform1f(sand_ribbon_opacity_location_, opacity);
    gl::draw_arrays(kGlTriangles, 0, static_cast<int>(vertices.size() / 5));
  };

  // A single living cloud coils around the animated staff shaft.
  if (use_pharaoh_ && (scene.swinging || scene.ultimate)) {
    float start_x, start_y, start_z, end_x, end_y, end_z;
    if (pharaoh_.staff_segment_world(player_model_matrix(scene),
                                     start_x, start_y, start_z,
                                     end_x, end_y, end_z)) {
      float ax = end_x - start_x;
      float ay = end_y - start_y;
      float az = end_z - start_z;
      const float axis_length = std::max(std::sqrt(ax * ax + ay * ay + az * az), 0.001f);
      ax /= axis_length;
      ay /= axis_length;
      az /= axis_length;
      float bx = az;
      float by = 0.0f;
      float bz = -ax;
      float basis_length = std::sqrt(bx * bx + bz * bz);
      if (basis_length < 0.001f) {
        bx = 1.0f;
        bz = 0.0f;
        basis_length = 1.0f;
      }
      bx /= basis_length;
      bz /= basis_length;
      const float cx = ay * bz - az * by;
      const float cy = az * bx - ax * bz;
      const float cz = ax * by - ay * bx;
      std::vector<RibbonPoint> points;
      for (int index = 0; index < 52; ++index) {
        const float t = static_cast<float>(index) / 51.0f;
        const float angle = scene.time * 13.0f + t * 18.8495559f;
        const float radius = 0.055f + std::sin(t * 9.0f + scene.time * 4.0f) * 0.025f;
        points.push_back({start_x + (end_x - start_x) * t +
                              (bx * std::cos(angle) + cx * std::sin(angle)) * radius,
                          start_y + (end_y - start_y) * t +
                              (by * std::cos(angle) + cy * std::sin(angle)) * radius,
                          start_z + (end_z - start_z) * t +
                              (bz * std::cos(angle) + cz * std::sin(angle)) * radius,
                          0.085f});
      }
      draw_curve(points, 0.84f, 0.73f, 0.50f, 0.86f);
    }
  }

  gl::depth_mask(1);
  gl::disable(kGlBlend);
}

void Renderer::draw_scene_meshes(const Mat4 &projection, const Mat4 &view,
                                 const SceneDrawParams &scene,
                                 const SceneTransforms &transforms) {
  gl::use_program(mesh_program_);
  gl::bind_vertex_array(vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);
  gl::enable_vertex_attrib_array(2);

  const Mat4 vp = projection * view;
  draw_lit_mesh(vp, Mat4::identity(), arena_vbo_, arena_vertex_count_);
  draw_lit_mesh(vp * transforms.dummy_model, transforms.dummy_model, dummy_post_vbo_,
                dummy_post_vertex_count_);
  if (scene.dummy_alive) {
    const Mat4 body_model = transforms.dummy_model * transforms.dummy_body_local;
    draw_lit_mesh(vp * body_model, body_model, dummy_body_vbo_, dummy_body_vertex_count_);
  }

  const Mat4 player_model = player_model_matrix(scene);
  if (use_pharaoh_) {
    if (!scene.player_hidden) {
      pharaoh_.draw(skinned_program_, rigid_program_, projection, view, player_model,
                    scene.finger_hidden, scene.player_opacity,
                    scene.player_dissolve, scene.player_reveal_progress);
    }
  } else {
    samurai_.draw(skinned_program_, rigid_program_, projection, view, player_model);
  }

  if (use_pharaoh_ && scene.finger_projectile_active && finger_projectile_loaded_) {
    const float projectile_scale = 0.045f / std::max(finger_projectile_.height(), 0.001f);
    const Mat4 projectile_model =
        Mat4::translate(scene.finger_projectile_x, scene.finger_projectile_y,
                        scene.finger_projectile_z) *
        Mat4::rotate_y(1.5707963268f - scene.finger_projectile_yaw) *
        Mat4::rotate_z(-scene.finger_projectile_pitch) *
        Mat4::scale(projectile_scale, projectile_scale, projectile_scale);
    finger_projectile_.draw(rigid_program_, projection * view, projectile_model, 0);
  }

  if (scene.remote_active && remote_pharaoh_loaded_) {
    const Mat4 remote_model =
        Mat4::translate(scene.remote_x, scene.remote_y, scene.remote_z) *
        Mat4::rotate_y(-scene.remote_yaw);
    if (!scene.remote_hidden) {
      remote_pharaoh_.draw(skinned_program_, rigid_program_, projection, view,
                           remote_model, false, 1.0f, 0.0f);
    }
    if (scene.remote_projectile_active && finger_projectile_loaded_) {
      const float projectile_scale =
          0.045f / std::max(finger_projectile_.height(), 0.001f);
      const Mat4 projectile_model =
          Mat4::translate(scene.remote_projectile_x, scene.remote_projectile_y,
                          scene.remote_projectile_z) *
          Mat4::rotate_y(1.5707963268f - scene.remote_projectile_yaw) *
          Mat4::rotate_z(-scene.remote_projectile_pitch) *
          Mat4::scale(projectile_scale, projectile_scale, projectile_scale);
      finger_projectile_.draw(rigid_program_, projection * view, projectile_model, 0);
    }
  }
}

void Renderer::draw_sky(const OrbitCamera &camera, float aspect, float time) {
  gl::use_program(sky_program_);
  gl::bind_vertex_array(sky_vao_);
  gl::enable_vertex_attrib_array(0);
  gl::depth_mask(0);

  gl::uniform3f(sky_camera_pos_location_, camera.x, camera.y, camera.z);
  gl::uniform3f(sky_camera_right_location_, camera.right_x, camera.right_y, camera.right_z);
  gl::uniform3f(sky_camera_up_location_, camera.up_x, camera.up_y, camera.up_z);
  gl::uniform3f(sky_camera_forward_location_, camera.forward_x, camera.forward_y, camera.forward_z);
  gl::uniform1f(sky_tan_half_fov_location_, std::tan(kCameraFovY * 0.5f));
  gl::uniform1f(sky_aspect_location_, aspect);
  gl::uniform3f(sky_sun_dir_location_, kSunDirX, kSunDirY, kSunDirZ);
  gl::uniform1f(sky_time_location_, time);

  gl::draw_arrays(kGlTriangles, 0, 3);
  gl::depth_mask(1);
}

void Renderer::draw_raytrace(const OrbitCamera &camera, float aspect, float time) {
  gl::disable(kGlDepthTest);
  gl::depth_mask(0);
  gl::use_program(raytrace_program_);
  if (ray_aspect_location_ >= 0) {
    gl::uniform1f(ray_aspect_location_, aspect);
  }
  gl::uniform3f(ray_camera_pos_location_, camera.x, camera.y, camera.z);
  gl::uniform3f(ray_camera_right_location_, camera.right_x, camera.right_y, camera.right_z);
  gl::uniform3f(ray_camera_up_location_, camera.up_x, camera.up_y, camera.up_z);
  gl::uniform3f(ray_camera_forward_location_, camera.forward_x, camera.forward_y, camera.forward_z);
  gl::uniform1f(ray_time_location_, time);
  gl::bind_vertex_array(sky_vao_);
  gl::draw_arrays(kGlTriangles, 0, 3);
  gl::depth_mask(1);
  gl::enable(kGlDepthTest);
}

void Renderer::draw_crosshair(float vertical_ndc) {
  gl::disable(kGlDepthTest);
  gl::use_program(crosshair_program_);
  gl::bind_vertex_array(sky_vao_);
  gl::uniform2f(crosshair_inv_viewport_location_,
                2.0f / static_cast<float>(width_),
                2.0f / static_cast<float>(height_));
  gl::uniform1f(crosshair_vertical_offset_location_, vertical_ndc);

  // A dark outline keeps the small white reticle readable against both the
  // bright sky and the Pharaoh's dark materials.
  gl::uniform1f(crosshair_scale_location_, 1.65f);
  gl::uniform3f(crosshair_color_location_, 0.02f, 0.02f, 0.02f);
  gl::draw_arrays(kGlTriangles, 0, 24);
  gl::uniform1f(crosshair_scale_location_, 1.0f);
  gl::uniform3f(crosshair_color_location_, 0.95f, 0.95f, 0.95f);
  gl::draw_arrays(kGlTriangles, 0, 24);
  gl::enable(kGlDepthTest);
}

void Renderer::update_samurai(const SceneDrawParams &scene) {
  if (scene.toggle_character && pharaoh_loaded_ && samurai_loaded_) {
    use_pharaoh_ = !use_pharaoh_;
    std::fprintf(stderr, "Player character: %s\n", use_pharaoh_ ? "Pharaoh" : "Samurai");
  }
  if (use_pharaoh_) {
    pharaoh_.update(scene.delta_time, scene.moving, scene.sprinting, scene.airborne,
                    scene.flying, scene.swinging, scene.finger_shooting, scene.ultimate,
                    scene.dodging, false,
                    scene.move_forward, scene.move_right);
  } else {
    samurai_.update(scene.delta_time, scene.moving, scene.sprinting, scene.airborne,
                    scene.flying, scene.swinging, false, false, scene.dodging,
                    scene.toggle_equip,
                    scene.move_forward, scene.move_right);
  }
  if (scene.remote_active && remote_pharaoh_loaded_) {
    remote_pharaoh_.update(scene.delta_time, scene.remote_moving,
                           scene.remote_sprinting, scene.remote_airborne,
                           scene.remote_flying, scene.remote_swinging,
                           scene.remote_shooting, scene.remote_ultimate,
                           false, false, scene.remote_move_forward,
                           scene.remote_move_right);
  }
}

float Renderer::swing_clip_duration() const {
  return use_pharaoh_ ? pharaoh_.swing_clip_duration() : samurai_.swing_clip_duration();
}

float Renderer::shoot_clip_duration() const {
  return use_pharaoh_ ? pharaoh_.shoot_clip_duration() : 0.5f;
}

float Renderer::ultimate_clip_duration() const {
  return use_pharaoh_ ? pharaoh_.ultimate_clip_duration() : 1.0f;
}

bool Renderer::sword_drawn() const {
  return use_pharaoh_ ? pharaoh_.sword_drawn() : samurai_.sword_drawn();
}

bool Renderer::can_swing() const {
  // Left-click is character-specific: staff smash for Pharaoh, sword slash
  // for Samurai. Finger shooting and the sand ultimate remain Pharaoh-only.
  return use_pharaoh_ ? pharaoh_.can_swing() : samurai_.can_swing();
}

bool Renderer::action_overrides_sprint() const {
  return use_pharaoh_ ? pharaoh_.action_overrides_sprint() : samurai_.action_overrides_sprint();
}

bool Renderer::equip_active() const {
  return use_pharaoh_ ? false : samurai_.equip_active();
}

bool Renderer::sword_tip_world(const SceneDrawParams &scene, float &x, float &y,
                               float &z) const {
  const Mat4 player_model = player_model_matrix(scene);
  return use_pharaoh_ ? pharaoh_.sword_tip_world(player_model, x, y, z)
                      : samurai_.sword_tip_world(player_model, x, y, z);
}

bool Renderer::staff_segment_world(const SceneDrawParams &scene,
                                   float &start_x, float &start_y, float &start_z,
                                   float &end_x, float &end_y, float &end_z) const {
  if (!use_pharaoh_) {
    return false;
  }
  return pharaoh_.staff_segment_world(player_model_matrix(scene),
                                      start_x, start_y, start_z,
                                      end_x, end_y, end_z);
}

bool Renderer::can_ultimate() const {
  return use_pharaoh_ && pharaoh_loaded_;
}

bool Renderer::using_pharaoh() const {
  return use_pharaoh_;
}

bool Renderer::finger_tip_world(const SceneDrawParams &scene, float &x, float &y,
                                float &z) const {
  if (!use_pharaoh_) {
    return false;
  }
  return pharaoh_.finger_tip_world(player_model_matrix(scene), x, y, z);
}

void Renderer::draw(const SceneDrawParams &scene) {
  update_sand_trail(scene);
  update_staff_summon(scene);
  update_ultimate_sand(scene);

  gl::viewport(0, 0, width_, height_);
  gl::clear(kGlColorBufferBit | kGlDepthBufferBit);

  const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
  const Mat4 projection = Mat4::perspective(kCameraFovY, aspect, 0.1f, 100.0f);
  const OrbitCamera camera = compute_orbit_camera(
      scene.player_x, scene.player_y, scene.player_z,
      scene.camera_yaw, scene.camera_pitch, scene.camera_side);
  const Mat4 view = Mat4::look_at(
      camera.x, camera.y, camera.z,
      camera.target_x, camera.target_y, camera.target_z,
      0.0f, 1.0f, 0.0f);
  const SceneTransforms transforms = compute_scene_transforms(scene);

  if (scene.raytracing) {
    draw_raytrace(camera, aspect, scene.time);
  } else {
    draw_sky(camera, aspect, scene.time);
  }
  draw_scene_meshes(projection, view, scene, transforms);
  draw_sand_cloud_ribbons(projection, view, camera, scene);
  draw_sand_trail(projection, view, camera);
  if (use_pharaoh_) {
    draw_crosshair(head_level_crosshair_ndc_y(
        camera, scene.player_x, scene.player_y, scene.player_z));
  }
}

void Renderer::shutdown() {
  pharaoh_.shutdown();
  remote_pharaoh_.shutdown();
  finger_projectile_.shutdown();
  samurai_.shutdown();
  pharaoh_loaded_ = false;
  remote_pharaoh_loaded_ = false;
  samurai_loaded_ = false;
  finger_projectile_loaded_ = false;
  mesh_program_ = 0;
  skinned_program_ = 0;
  rigid_program_ = 0;
  sky_program_ = 0;
  raytrace_program_ = 0;
  crosshair_program_ = 0;
  sand_cloud_program_ = 0;
  sand_ribbon_program_ = 0;
  vao_ = 0;
  sky_vao_ = 0;
  sand_cloud_vao_ = 0;
  sand_ribbon_vao_ = 0;
  arena_vbo_ = 0;
  dummy_post_vbo_ = 0;
  dummy_body_vbo_ = 0;
  sand_particle_vbo_ = 0;
  sky_vbo_ = 0;
  sand_cloud_vbo_ = 0;
  sand_ribbon_vbo_ = 0;
  sand_particle_vertex_count_ = 0;
  sand_particles_.clear();
  sand_emit_accumulator_ = 0.0f;
  staff_sand_emit_accumulator_ = 0.0f;
  staff_sand_dismiss_time_ = 0.0f;
  had_active_projectile_ = false;
  staff_was_swinging_ = false;
  ultimate_was_returning_ = false;
  ultimate_return_emit_accumulator_ = 0.0f;
  ultimate_rush_time_ = 0.0f;
  ultimate_rush_emit_accumulator_ = 0.0f;
}

} // namespace engine
