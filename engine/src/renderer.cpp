#include "renderer.h"

#include "arena_mesh.h"
#include "character_mesh.h"
#include "dummy_mesh.h"
#include "gl_loader.h"
#include "math.h"
#include "mesh_builder.h"

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
constexpr unsigned int kGlFloat = 0x1406;
constexpr unsigned int kGlFalse = 0;
constexpr unsigned int kGlVertexShader = 0x8B31;
constexpr unsigned int kGlFragmentShader = 0x8B30;
constexpr unsigned int kGlCompileStatus = 0x8B81;
constexpr unsigned int kGlLinkStatus = 0x8B82;
constexpr unsigned int kGlTriangles = 0x0004;

constexpr float kSunDirX = 0.551f;
constexpr float kSunDirY = 0.701f;
constexpr float kSunDirZ = -0.451f;

constexpr const char *kMeshVertexShader = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    vColor = aColor;
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

constexpr const char* kRaytraceVertexShader = R"(#version 330 core
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

constexpr const char* kRaytraceFragmentShader = R"(#version 330 core
in vec2 vUv;
out vec4 FragColor;

bool hitSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t, out vec3 normal) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;

    if (h < 0.0) return false;

    h = sqrt(h);
    t = -b - h;

    if (t < 0.001) t = -b + h;
    if (t < 0.001) return false;

    vec3 p = ro + rd * t;
    normal = normalize(p - center);
    return true;
}

vec3 skyColor(vec3 rd) {
    float a = 0.5 * (rd.y + 1.0);
    return mix(vec3(0.08, 0.06, 0.16), vec3(0.55, 0.75, 1.0), a);
}

void main() {
    vec2 uv = vUv * 2.0 - 1.0;

    vec3 ro = vec3(0.0, 1.0, 4.0);
   uv.x *= 16.0 / 9.0;
vec3 rd = normalize(vec3(uv.x, uv.y, -1.5));

    vec3 color = skyColor(rd);

    float t;
    vec3 n;

    if (hitSphere(ro, rd, vec3(0.0, 1.0, 0.0), 1.0, t, n)) {
        vec3 hitPos = ro + rd * t;
        vec3 reflected = reflect(rd, n);

        vec3 lightDir = normalize(vec3(-0.4, 0.8, 0.3));
        float diffuse = max(dot(n, lightDir), 0.0);

        vec3 reflectionColor = skyColor(reflected);
        vec3 baseColor = vec3(0.85, 0.85, 0.9) * diffuse;

        color = mix(baseColor, reflectionColor, 0.95);
    }

    FragColor = vec4(color, 1.0);
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
  transforms.player_model =
      Mat4::translate(scene.player_x, scene.player_y, scene.player_z) *
      Mat4::rotate_y(scene.player_yaw);

  const float leg_stride = std::sin(scene.walk_phase) * 0.10f;
  const float leg_swing = std::sin(scene.walk_phase) * 0.45f;
  transforms.left_leg_local =
      Mat4::translate(kLeftLegX, kLegCenterY, leg_stride) * Mat4::rotate_x(leg_swing);
  transforms.right_leg_local =
      Mat4::translate(kRightLegX, kLegCenterY, -leg_stride) * Mat4::rotate_x(-leg_swing);
  transforms.weapon_arm_local =
      Mat4::translate(kWeaponShoulderX, kWeaponShoulderY, kWeaponShoulderZ) *
      Mat4::rotate_x(scene.swing_angle);
  transforms.dummy_model =
      Mat4::translate(kDummySpawnX, kDummySpawnY, kDummySpawnZ) *
      Mat4::rotate_y(kDummySpawnYaw);
  const float dummy_lean = scene.dummy_alive ? scene.dummy_hit_lean : 1.2f;
  transforms.dummy_body_local =
      Mat4::translate(0.0f, 0.55f, 0.0f) * Mat4::rotate_x(-dummy_lean) *
      Mat4::translate(0.0f, -0.55f, 0.0f);
  return transforms;
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

  if (!mesh_vs || !mesh_fs || !sky_vs || !sky_fs) {
    return false;
  }

  mesh_program_ = link_program(mesh_vs, mesh_fs);
  sky_program_ = link_program(sky_vs, sky_fs);
  raytrace_program_ = link_program(ray_vs, ray_fs);

  gl::delete_shader(mesh_vs);
  gl::delete_shader(mesh_fs);
  gl::delete_shader(sky_vs);
  gl::delete_shader(sky_fs);

  if (!mesh_program_ || !sky_program_) {
    return false;
  }

  mesh_mvp_location_ = gl::get_uniform_location(mesh_program_, "uMVP");

  sky_camera_pos_location_ = gl::get_uniform_location(sky_program_, "uCameraPos");
  sky_camera_right_location_ = gl::get_uniform_location(sky_program_, "uCameraRight");
  sky_camera_up_location_ = gl::get_uniform_location(sky_program_, "uCameraUp");
  sky_camera_forward_location_ = gl::get_uniform_location(sky_program_, "uCameraForward");
  sky_tan_half_fov_location_ = gl::get_uniform_location(sky_program_, "uTanHalfFov");
  sky_aspect_location_ = gl::get_uniform_location(sky_program_, "uAspect");
  sky_sun_dir_location_ = gl::get_uniform_location(sky_program_, "uSunDir");
  sky_time_location_ = gl::get_uniform_location(sky_program_, "uSkyTime");

  vao_ = gl::create_vertex_array();
  gl::bind_vertex_array(vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);

  sky_vao_ = gl::create_vertex_array();
  gl::bind_vertex_array(sky_vao_);
  gl::enable_vertex_attrib_array(0);

  const float sky_triangle[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
  gl::gen_buffers(1, &sky_vbo_);
  gl::bind_buffer(kGlArrayBuffer, sky_vbo_);
  gl::buffer_data(kGlArrayBuffer, static_cast<long long>(sizeof(sky_triangle)), sky_triangle,
                  kGlStaticDraw);
  gl::vertex_attrib_pointer(0, 2, kGlFloat, kGlFalse, 2 * static_cast<int>(sizeof(float)), nullptr);

  const std::vector<float> arena_vertices = build_arena();
  const std::vector<float> torso_vertices = build_character_torso();
  const std::vector<float> left_leg_vertices = build_left_leg();
  const std::vector<float> right_leg_vertices = build_right_leg();
  const std::vector<float> weapon_arm_vertices = build_weapon_arm();
  const std::vector<float> sword_vertices = build_sword_mesh();
  const std::vector<float> dummy_post_vertices = build_dummy_post();
  const std::vector<float> dummy_body_vertices = build_dummy_body();

  arena_vertex_count_ = static_cast<int>(arena_vertices.size() / kVertexFloats);
  torso_vertex_count_ = static_cast<int>(torso_vertices.size() / kVertexFloats);
  leg_vertex_count_ = static_cast<int>(left_leg_vertices.size() / kVertexFloats);
  weapon_arm_vertex_count_ = static_cast<int>(weapon_arm_vertices.size() / kVertexFloats);
  sword_vertex_count_ = static_cast<int>(sword_vertices.size() / kVertexFloats);
  dummy_post_vertex_count_ = static_cast<int>(dummy_post_vertices.size() / kVertexFloats);
  dummy_body_vertex_count_ = static_cast<int>(dummy_body_vertices.size() / kVertexFloats);

  gl::gen_buffers(1, &arena_vbo_);
  gl::gen_buffers(1, &torso_vbo_);
  gl::gen_buffers(1, &left_leg_vbo_);
  gl::gen_buffers(1, &right_leg_vbo_);
  gl::gen_buffers(1, &weapon_arm_vbo_);
  gl::gen_buffers(1, &sword_vbo_);
  gl::gen_buffers(1, &dummy_post_vbo_);
  gl::gen_buffers(1, &dummy_body_vbo_);

  upload_mesh(arena_vbo_, arena_vertices);
  upload_mesh(torso_vbo_, torso_vertices);
  upload_mesh(left_leg_vbo_, left_leg_vertices);
  upload_mesh(right_leg_vbo_, right_leg_vertices);
  upload_mesh(weapon_arm_vbo_, weapon_arm_vertices);
  upload_mesh(sword_vbo_, sword_vertices);
  upload_mesh(dummy_post_vbo_, dummy_post_vertices);
  upload_mesh(dummy_body_vbo_, dummy_body_vertices);

  gl::enable(kGlDepthTest);
  gl::depth_func(kGlLess);
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
                            reinterpret_cast<const void *>(6 * sizeof(float)));
}

void Renderer::draw_mesh(const Mat4 &mvp, unsigned int vbo, int vertex_count) {
  bind_mesh_vbo(vbo);
  gl::uniform_matrix4fv(mesh_mvp_location_, 1, kGlFalse, mvp.m);
  gl::draw_arrays(kGlTriangles, 0, vertex_count);
}

void Renderer::draw_scene_meshes(const Mat4 &projection, const Mat4 &view,
                                 const SceneDrawParams &scene,
                                 const SceneTransforms &transforms) {
  gl::use_program(mesh_program_);
  gl::bind_vertex_array(vao_);
  gl::enable_vertex_attrib_array(0);
  gl::enable_vertex_attrib_array(1);

  const Mat4 mvp = projection * view;
  draw_mesh(mvp, arena_vbo_, arena_vertex_count_);
  draw_mesh(mvp * transforms.dummy_model, dummy_post_vbo_, dummy_post_vertex_count_);
  if (scene.dummy_alive) {
    draw_mesh(mvp * transforms.dummy_model * transforms.dummy_body_local,
              dummy_body_vbo_, dummy_body_vertex_count_);
  }
  draw_mesh(mvp * transforms.player_model * transforms.left_leg_local,
            left_leg_vbo_, leg_vertex_count_);
  draw_mesh(mvp * transforms.player_model * transforms.right_leg_local,
            right_leg_vbo_, leg_vertex_count_);
  draw_mesh(mvp * transforms.player_model, torso_vbo_, torso_vertex_count_);
  draw_mesh(mvp * transforms.player_model * transforms.weapon_arm_local,
            weapon_arm_vbo_, weapon_arm_vertex_count_);
  draw_mesh(mvp * transforms.player_model * transforms.weapon_arm_local,
            sword_vbo_, sword_vertex_count_);
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

void Renderer::draw(const SceneDrawParams &scene) {
  gl::viewport(0, 0, width_, height_);
  gl::clear(kGlColorBufferBit | kGlDepthBufferBit);

  const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
  const Mat4 projection = Mat4::perspective(kCameraFovY, aspect, 0.1f, 100.0f);
  const OrbitCamera camera = compute_orbit_camera(
      scene.player_x, scene.player_y, scene.player_z,
      scene.camera_yaw, scene.camera_pitch);
  const Mat4 view = Mat4::look_at(
      camera.x, camera.y, camera.z,
      camera.target_x, camera.target_y, camera.target_z,
      0.0f, 1.0f, 0.0f);
  const SceneTransforms transforms = compute_scene_transforms(scene);

  //draw_sky(camera, aspect, scene.time);
  //draw_scene_meshes(projection, view, scene, transforms);
  gl::use_program(raytrace_program_);
  gl::bind_vertex_array(sky_vao_);
  gl::draw_arrays(kGlTriangles, 0, 3);

}

void Renderer::shutdown() {
  mesh_program_ = 0;
  sky_program_ = 0;
  vao_ = 0;
  sky_vao_ = 0;
  arena_vbo_ = 0;
  torso_vbo_ = 0;
  left_leg_vbo_ = 0;
  right_leg_vbo_ = 0;
  weapon_arm_vbo_ = 0;
  sword_vbo_ = 0;
  dummy_post_vbo_ = 0;
  dummy_body_vbo_ = 0;
  sky_vbo_ = 0;
}

} // namespace engine
