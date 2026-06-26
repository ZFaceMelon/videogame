#include "gl_loader.h"

#include <cstdio>

namespace engine::gl {
namespace {

using GLViewportProc = void (*)(int, int, int, int);
using GLClearColorProc = void (*)(float, float, float, float);
using GLClearProc = void (*)(unsigned int);
using GLEnableProc = void (*)(unsigned int);
using GLDisableProc = void (*)(unsigned int);
using GLDepthFuncProc = void (*)(unsigned int);
using GLDepthMaskProc = void (*)(unsigned char);
using GLGenVertexArraysProc = void (*)(int, unsigned int *);
using GLBindVertexArrayProc = void (*)(unsigned int);
using GLGenBuffersProc = void (*)(int, unsigned int *);
using GLBindBufferProc = void (*)(unsigned int, unsigned int);
using GLBufferDataProc = void (*)(unsigned int, long long, const void *, unsigned int);
using GLVertexAttribPointerProc = void (*)(unsigned int, int, unsigned int, unsigned char, int, const void *);
using GLEnableVertexAttribArrayProc = void (*)(unsigned int);
using GLCreateShaderProc = unsigned int (*)(unsigned int);
using GLShaderSourceProc = void (*)(unsigned int, int, const char **, const int *);
using GLCompileShaderProc = void (*)(unsigned int);
using GLGetShaderivProc = void (*)(unsigned int, unsigned int, int *);
using GLGetShaderInfoLogProc = void (*)(unsigned int, int, int *, char *);
using GLCreateProgramProc = unsigned int (*)();
using GLAttachShaderProc = void (*)(unsigned int, unsigned int);
using GLLinkProgramProc = void (*)(unsigned int);
using GLGetProgramivProc = void (*)(unsigned int, unsigned int, int *);
using GLGetProgramInfoLogProc = void (*)(unsigned int, int, int *, char *);
using GLDeleteShaderProc = void (*)(unsigned int);
using GLUseProgramProc = void (*)(unsigned int);
using GLGetUniformLocationProc = int (*)(unsigned int, const char *);
using GLUniformMatrix4fvProc = void (*)(int, int, unsigned char, const float *);
using GLUniform1fProc = void (*)(int, float);
using GLUniform1iProc = void (*)(int, int);
using GLUniform2fProc = void (*)(int, float, float);
using GLUniform3fProc = void (*)(int, float, float, float);
using GLDrawArraysProc = void (*)(unsigned int, int, int);
using GLGenFramebuffersProc = void (*)(int, unsigned int *);
using GLBindFramebufferProc = void (*)(unsigned int, unsigned int);
using GLFramebufferTexture2DProc = void (*)(unsigned int, unsigned int, unsigned int, unsigned int, int);
using GLGenRenderbuffersProc = void (*)(int, unsigned int *);
using GLBindRenderbufferProc = void (*)(unsigned int, unsigned int);
using GLRenderbufferStorageProc = void (*)(unsigned int, unsigned int, int, int);
using GLFramebufferRenderbufferProc = void (*)(unsigned int, unsigned int, unsigned int, unsigned int);
using GLCheckFramebufferStatusProc = unsigned int (*)(unsigned int);
using GLDeleteFramebuffersProc = void (*)(int, const unsigned int *);
using GLDeleteRenderbuffersProc = void (*)(int, const unsigned int *);
using GLGenTexturesProc = void (*)(int, unsigned int *);
using GLBindTextureProc = void (*)(unsigned int, unsigned int);
using GLDeleteTexturesProc = void (*)(int, const unsigned int *);
using GLActiveTextureProc = void (*)(unsigned int);
using GLTexImage2DProc = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
using GLTexParameteriProc = void (*)(unsigned int, unsigned int, int);
using GLCullFaceProc = void (*)(unsigned int);
using GLBlendFuncProc = void (*)(unsigned int, unsigned int);
using GLDrawBuffersProc = void (*)(int, const unsigned int *);

#define LOAD_GL(name, fn, type)                       \
    fn = reinterpret_cast<type>(glfwGetProcAddress(#name)); \
    if (!fn) {                                         \
        std::fprintf(stderr, "Failed to load OpenGL function: %s\n", #name); \
        return false;                                  \
    }

bool loaded = false;

GLViewportProc glViewport_fn = nullptr;
GLClearColorProc glClearColor_fn = nullptr;
GLClearProc glClear_fn = nullptr;
GLEnableProc glEnable_fn = nullptr;
GLDisableProc glDisable_fn = nullptr;
GLDepthFuncProc glDepthFunc_fn = nullptr;
GLDepthMaskProc glDepthMask_fn = nullptr;
GLGenVertexArraysProc glGenVertexArrays_fn = nullptr;
GLBindVertexArrayProc glBindVertexArray_fn = nullptr;
GLGenBuffersProc glGenBuffers_fn = nullptr;
GLBindBufferProc glBindBuffer_fn = nullptr;
GLBufferDataProc glBufferData_fn = nullptr;
GLVertexAttribPointerProc glVertexAttribPointer_fn = nullptr;
GLEnableVertexAttribArrayProc glEnableVertexAttribArray_fn = nullptr;
GLCreateShaderProc glCreateShader_fn = nullptr;
GLShaderSourceProc glShaderSource_fn = nullptr;
GLCompileShaderProc glCompileShader_fn = nullptr;
GLGetShaderivProc glGetShaderiv_fn = nullptr;
GLGetShaderInfoLogProc glGetShaderInfoLog_fn = nullptr;
GLCreateProgramProc glCreateProgram_fn = nullptr;
GLAttachShaderProc glAttachShader_fn = nullptr;
GLLinkProgramProc glLinkProgram_fn = nullptr;
GLGetProgramivProc glGetProgramiv_fn = nullptr;
GLGetProgramInfoLogProc glGetProgramInfoLog_fn = nullptr;
GLDeleteShaderProc glDeleteShader_fn = nullptr;
GLUseProgramProc glUseProgram_fn = nullptr;
GLGetUniformLocationProc glGetUniformLocation_fn = nullptr;
GLUniformMatrix4fvProc glUniformMatrix4fv_fn = nullptr;
GLUniform1fProc glUniform1f_fn = nullptr;
GLUniform1iProc glUniform1i_fn = nullptr;
GLUniform2fProc glUniform2f_fn = nullptr;
GLUniform3fProc glUniform3f_fn = nullptr;
GLDrawArraysProc glDrawArrays_fn = nullptr;
GLGenFramebuffersProc glGenFramebuffers_fn = nullptr;
GLBindFramebufferProc glBindFramebuffer_fn = nullptr;
GLFramebufferTexture2DProc glFramebufferTexture2D_fn = nullptr;
GLGenRenderbuffersProc glGenRenderbuffers_fn = nullptr;
GLBindRenderbufferProc glBindRenderbuffer_fn = nullptr;
GLRenderbufferStorageProc glRenderbufferStorage_fn = nullptr;
GLFramebufferRenderbufferProc glFramebufferRenderbuffer_fn = nullptr;
GLCheckFramebufferStatusProc glCheckFramebufferStatus_fn = nullptr;
GLDeleteFramebuffersProc glDeleteFramebuffers_fn = nullptr;
GLDeleteRenderbuffersProc glDeleteRenderbuffers_fn = nullptr;
GLGenTexturesProc glGenTextures_fn = nullptr;
GLBindTextureProc glBindTexture_fn = nullptr;
GLDeleteTexturesProc glDeleteTextures_fn = nullptr;
GLActiveTextureProc glActiveTexture_fn = nullptr;
GLTexImage2DProc glTexImage2D_fn = nullptr;
GLTexParameteriProc glTexParameteri_fn = nullptr;
GLCullFaceProc glCullFace_fn = nullptr;
GLBlendFuncProc glBlendFunc_fn = nullptr;
GLDrawBuffersProc glDrawBuffers_fn = nullptr;

} // namespace

bool load() {
    if (loaded) {
        return true;
    }

    LOAD_GL(glViewport, glViewport_fn, GLViewportProc);
    LOAD_GL(glClearColor, glClearColor_fn, GLClearColorProc);
    LOAD_GL(glClear, glClear_fn, GLClearProc);
    LOAD_GL(glEnable, glEnable_fn, GLEnableProc);
    LOAD_GL(glDisable, glDisable_fn, GLDisableProc);
    LOAD_GL(glDepthFunc, glDepthFunc_fn, GLDepthFuncProc);
    LOAD_GL(glDepthMask, glDepthMask_fn, GLDepthMaskProc);
    LOAD_GL(glGenVertexArrays, glGenVertexArrays_fn, GLGenVertexArraysProc);
    LOAD_GL(glBindVertexArray, glBindVertexArray_fn, GLBindVertexArrayProc);
    LOAD_GL(glGenBuffers, glGenBuffers_fn, GLGenBuffersProc);
    LOAD_GL(glBindBuffer, glBindBuffer_fn, GLBindBufferProc);
    LOAD_GL(glBufferData, glBufferData_fn, GLBufferDataProc);
    LOAD_GL(glVertexAttribPointer, glVertexAttribPointer_fn, GLVertexAttribPointerProc);
    LOAD_GL(glEnableVertexAttribArray, glEnableVertexAttribArray_fn, GLEnableVertexAttribArrayProc);
    LOAD_GL(glCreateShader, glCreateShader_fn, GLCreateShaderProc);
    LOAD_GL(glShaderSource, glShaderSource_fn, GLShaderSourceProc);
    LOAD_GL(glCompileShader, glCompileShader_fn, GLCompileShaderProc);
    LOAD_GL(glGetShaderiv, glGetShaderiv_fn, GLGetShaderivProc);
    LOAD_GL(glGetShaderInfoLog, glGetShaderInfoLog_fn, GLGetShaderInfoLogProc);
    LOAD_GL(glCreateProgram, glCreateProgram_fn, GLCreateProgramProc);
    LOAD_GL(glAttachShader, glAttachShader_fn, GLAttachShaderProc);
    LOAD_GL(glLinkProgram, glLinkProgram_fn, GLLinkProgramProc);
    LOAD_GL(glGetProgramiv, glGetProgramiv_fn, GLGetProgramivProc);
    LOAD_GL(glGetProgramInfoLog, glGetProgramInfoLog_fn, GLGetProgramInfoLogProc);
    LOAD_GL(glDeleteShader, glDeleteShader_fn, GLDeleteShaderProc);
    LOAD_GL(glUseProgram, glUseProgram_fn, GLUseProgramProc);
    LOAD_GL(glGetUniformLocation, glGetUniformLocation_fn, GLGetUniformLocationProc);
    LOAD_GL(glUniformMatrix4fv, glUniformMatrix4fv_fn, GLUniformMatrix4fvProc);
    LOAD_GL(glUniform1f, glUniform1f_fn, GLUniform1fProc);
    LOAD_GL(glUniform1i, glUniform1i_fn, GLUniform1iProc);
    LOAD_GL(glUniform2f, glUniform2f_fn, GLUniform2fProc);
    LOAD_GL(glUniform3f, glUniform3f_fn, GLUniform3fProc);
    LOAD_GL(glDrawArrays, glDrawArrays_fn, GLDrawArraysProc);
    LOAD_GL(glGenFramebuffers, glGenFramebuffers_fn, GLGenFramebuffersProc);
    LOAD_GL(glBindFramebuffer, glBindFramebuffer_fn, GLBindFramebufferProc);
    LOAD_GL(glFramebufferTexture2D, glFramebufferTexture2D_fn, GLFramebufferTexture2DProc);
    LOAD_GL(glGenRenderbuffers, glGenRenderbuffers_fn, GLGenRenderbuffersProc);
    LOAD_GL(glBindRenderbuffer, glBindRenderbuffer_fn, GLBindRenderbufferProc);
    LOAD_GL(glRenderbufferStorage, glRenderbufferStorage_fn, GLRenderbufferStorageProc);
    LOAD_GL(glFramebufferRenderbuffer, glFramebufferRenderbuffer_fn, GLFramebufferRenderbufferProc);
    LOAD_GL(glCheckFramebufferStatus, glCheckFramebufferStatus_fn, GLCheckFramebufferStatusProc);
    LOAD_GL(glDeleteFramebuffers, glDeleteFramebuffers_fn, GLDeleteFramebuffersProc);
    LOAD_GL(glDeleteRenderbuffers, glDeleteRenderbuffers_fn, GLDeleteRenderbuffersProc);
    LOAD_GL(glGenTextures, glGenTextures_fn, GLGenTexturesProc);
    LOAD_GL(glBindTexture, glBindTexture_fn, GLBindTextureProc);
    LOAD_GL(glDeleteTextures, glDeleteTextures_fn, GLDeleteTexturesProc);
    LOAD_GL(glActiveTexture, glActiveTexture_fn, GLActiveTextureProc);
    LOAD_GL(glTexImage2D, glTexImage2D_fn, GLTexImage2DProc);
    LOAD_GL(glTexParameteri, glTexParameteri_fn, GLTexParameteriProc);
    LOAD_GL(glCullFace, glCullFace_fn, GLCullFaceProc);
    LOAD_GL(glBlendFunc, glBlendFunc_fn, GLBlendFuncProc);
    LOAD_GL(glDrawBuffers, glDrawBuffers_fn, GLDrawBuffersProc);

    loaded = true;
    return true;
}

void viewport(int x, int y, int width, int height) { glViewport_fn(x, y, width, height); }
void clear_color(float r, float g, float b, float a) { glClearColor_fn(r, g, b, a); }
void clear(unsigned int mask) { glClear_fn(mask); }
void enable(unsigned int cap) { glEnable_fn(cap); }
void disable(unsigned int cap) { glDisable_fn(cap); }
void depth_func(unsigned int func) { glDepthFunc_fn(func); }
void depth_mask(unsigned char enabled) { glDepthMask_fn(enabled); }

unsigned int create_vertex_array() {
    unsigned int vao = 0;
    glGenVertexArrays_fn(1, &vao);
    return vao;
}

void bind_vertex_array(unsigned int vao) { glBindVertexArray_fn(vao); }
void gen_buffers(int n, unsigned int *buffers) { glGenBuffers_fn(n, buffers); }
void bind_buffer(unsigned int target, unsigned int buffer) { glBindBuffer_fn(target, buffer); }
void buffer_data(unsigned int target, long long size, const void *data, unsigned int usage) {
    glBufferData_fn(target, size, data, usage);
}
void vertex_attrib_pointer(unsigned int index, int size, unsigned int type,
                           unsigned char normalized, int stride, const void *pointer) {
    glVertexAttribPointer_fn(index, size, type, normalized, stride, pointer);
}
void enable_vertex_attrib_array(unsigned int index) { glEnableVertexAttribArray_fn(index); }

unsigned int create_shader(unsigned int type) { return glCreateShader_fn(type); }
void shader_source(unsigned int shader, int count, const char **source, const int *length) {
    glShaderSource_fn(shader, count, source, length);
}
void compile_shader(unsigned int shader) { glCompileShader_fn(shader); }
void get_shader_iv(unsigned int shader, unsigned int pname, int *params) {
    glGetShaderiv_fn(shader, pname, params);
}
void get_shader_info_log(unsigned int shader, int max_length, int *length, char *info_log) {
    glGetShaderInfoLog_fn(shader, max_length, length, info_log);
}

unsigned int create_program() { return glCreateProgram_fn(); }
void attach_shader(unsigned int program, unsigned int shader) { glAttachShader_fn(program, shader); }
void link_program(unsigned int program) { glLinkProgram_fn(program); }
void get_program_iv(unsigned int program, unsigned int pname, int *params) {
    glGetProgramiv_fn(program, pname, params);
}
void get_program_info_log(unsigned int program, int max_length, int *length, char *info_log) {
    glGetProgramInfoLog_fn(program, max_length, length, info_log);
}
void delete_shader(unsigned int shader) { glDeleteShader_fn(shader); }
void use_program(unsigned int program) { glUseProgram_fn(program); }
int get_uniform_location(unsigned int program, const char *name) {
    return glGetUniformLocation_fn(program, name);
}
void uniform_matrix4fv(int location, int count, unsigned char transpose, const float *value) {
    glUniformMatrix4fv_fn(location, count, transpose, value);
}
void uniform1f(int location, float value) { glUniform1f_fn(location, value); }
void uniform1i(int location, int value) { glUniform1i_fn(location, value); }
void uniform2f(int location, float x, float y) { glUniform2f_fn(location, x, y); }
void uniform3f(int location, float x, float y, float z) { glUniform3f_fn(location, x, y, z); }
void draw_arrays(unsigned int mode, int first, int count) { glDrawArrays_fn(mode, first, count); }

void gen_framebuffers(int n, unsigned int *ids) { glGenFramebuffers_fn(n, ids); }
void bind_framebuffer(unsigned int target, unsigned int framebuffer) {
    glBindFramebuffer_fn(target, framebuffer);
}
void framebuffer_texture2d(unsigned int target, unsigned int attachment,
                           unsigned int textarget, unsigned int texture, int level) {
    glFramebufferTexture2D_fn(target, attachment, textarget, texture, level);
}
void gen_renderbuffers(int n, unsigned int *ids) { glGenRenderbuffers_fn(n, ids); }
void bind_renderbuffer(unsigned int target, unsigned int renderbuffer) {
    glBindRenderbuffer_fn(target, renderbuffer);
}
void renderbuffer_storage(unsigned int target, unsigned int internalformat,
                          int width, int height) {
    glRenderbufferStorage_fn(target, internalformat, width, height);
}
void framebuffer_renderbuffer(unsigned int target, unsigned int attachment,
                              unsigned int renderbuffertarget, unsigned int renderbuffer) {
    glFramebufferRenderbuffer_fn(target, attachment, renderbuffertarget, renderbuffer);
}
unsigned int check_framebuffer_status(unsigned int target) {
    return glCheckFramebufferStatus_fn(target);
}
void delete_framebuffers(int n, const unsigned int *ids) { glDeleteFramebuffers_fn(n, ids); }
void delete_renderbuffers(int n, const unsigned int *ids) { glDeleteRenderbuffers_fn(n, ids); }

void gen_textures(int n, unsigned int *ids) { glGenTextures_fn(n, ids); }
void bind_texture(unsigned int target, unsigned int texture) { glBindTexture_fn(target, texture); }
void delete_textures(int n, const unsigned int *ids) { glDeleteTextures_fn(n, ids); }
void active_texture(unsigned int texture) { glActiveTexture_fn(texture); }
void tex_image2d(unsigned int target, int level, int internalformat,
                 int width, int height, int border, unsigned int format,
                 unsigned int type, const void *data) {
    glTexImage2D_fn(target, level, internalformat, width, height, border, format, type, data);
}
void texParameteri(unsigned int target, unsigned int pname, int param) {
    glTexParameteri_fn(target, pname, param);
}
void cull_face(unsigned int mode) { glCullFace_fn(mode); }
void enable_cull_face() { glEnable_fn(0x0B44); }
void disable_cull_face() { glDisable_fn(0x0B44); }
void blend_func(unsigned int sfactor, unsigned int dfactor) { glBlendFunc_fn(sfactor, dfactor); }
void draw_buffers(int n, const unsigned int *bufs) { glDrawBuffers_fn(n, bufs); }

} // namespace engine::gl
