#pragma once

#include <GLFW/glfw3.h>

namespace engine::gl {

bool load();
void viewport(int x, int y, int width, int height);
void clear_color(float r, float g, float b, float a);
void clear(unsigned int mask);
void enable(unsigned int cap);
void disable(unsigned int cap);
void depth_func(unsigned int func);
void depth_mask(unsigned char enabled);
void cull_face(unsigned int mode);
void enable_cull_face();
void disable_cull_face();
void blend_func(unsigned int sfactor, unsigned int dfactor);
void draw_buffers(int n, const unsigned int *bufs);

unsigned int create_vertex_array();
void bind_vertex_array(unsigned int vao);
void gen_buffers(int n, unsigned int *buffers);
void delete_buffer(int n, const unsigned int *buffers);
void bind_buffer(unsigned int target, unsigned int buffer);
void buffer_data(unsigned int target, long long size, const void *data, unsigned int usage);
void vertex_attrib_pointer(unsigned int index, int size, unsigned int type,
                           unsigned char normalized, int stride, const void *pointer);
void enable_vertex_attrib_array(unsigned int index);
void vertex_attrib_ipointer(unsigned int index, int size, unsigned int type, int stride,
                            const void *pointer);
void draw_elements(unsigned int mode, int count, unsigned int type, const void *indices);

unsigned int create_shader(unsigned int type);
void shader_source(unsigned int shader, int count, const char **source, const int *length);
void compile_shader(unsigned int shader);
void get_shader_iv(unsigned int shader, unsigned int pname, int *params);
void get_shader_info_log(unsigned int shader, int max_length, int *length, char *info_log);

unsigned int create_program();
void attach_shader(unsigned int program, unsigned int shader);
void link_program(unsigned int program);
void get_program_iv(unsigned int program, unsigned int pname, int *params);
void get_program_info_log(unsigned int program, int max_length, int *length, char *info_log);
void delete_shader(unsigned int shader);
void use_program(unsigned int program);
int get_uniform_location(unsigned int program, const char *name);
void uniform_matrix4fv(int location, int count, unsigned char transpose, const float *value);
void uniform1f(int location, float value);
void uniform1i(int location, int value);
void uniform2f(int location, float x, float y);
void uniform3f(int location, float x, float y, float z);
void draw_arrays(unsigned int mode, int first, int count);

void gen_framebuffers(int n, unsigned int *ids);
void bind_framebuffer(unsigned int target, unsigned int framebuffer);
void framebuffer_texture2d(unsigned int target, unsigned int attachment,
                           unsigned int textarget, unsigned int texture, int level);
void gen_renderbuffers(int n, unsigned int *ids);
void bind_renderbuffer(unsigned int target, unsigned int renderbuffer);
void renderbuffer_storage(unsigned int target, unsigned int internalformat,
                          int width, int height);
void framebuffer_renderbuffer(unsigned int target, unsigned int attachment,
                              unsigned int renderbuffertarget, unsigned int renderbuffer);
unsigned int check_framebuffer_status(unsigned int target);
void delete_framebuffers(int n, const unsigned int *ids);
void delete_renderbuffers(int n, const unsigned int *ids);

void gen_textures(int n, unsigned int *ids);
void bind_texture(unsigned int target, unsigned int texture);
void delete_textures(int n, const unsigned int *ids);
void active_texture(unsigned int texture);
void tex_image2d(unsigned int target, int level, int internalformat,
                 int width, int height, int border, unsigned int format,
                 unsigned int type, const void *data);
void texParameteri(unsigned int target, unsigned int pname, int param);

} // namespace engine::gl
