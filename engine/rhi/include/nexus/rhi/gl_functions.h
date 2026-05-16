#pragma once

// ============================================================================
// gl_functions.h - Minimal OpenGL 4.5 function loader for NexusEngine
//
// Defines GL types, constants, and function pointers.
// Call nexus::rhi::gl::load(glfwGetProcAddress) before any GL usage.
// ============================================================================

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// GL type definitions
// ---------------------------------------------------------------------------
using GLenum     = unsigned int;
using GLboolean  = unsigned char;
using GLbitfield = unsigned int;
using GLint      = int;
using GLuint     = unsigned int;
using GLsizei    = int;
using GLfloat    = float;
using GLdouble   = double;
using GLchar     = char;
using GLsizeiptr = ptrdiff_t;
using GLintptr   = ptrdiff_t;
using GLubyte    = unsigned char;
using GLvoid     = void;

// ---------------------------------------------------------------------------
// GL constants
// ---------------------------------------------------------------------------

// Boolean
#define GL_TRUE                  1
#define GL_FALSE                 0

// Errors
#define GL_NO_ERROR              0

// Buffer targets
#define GL_ARRAY_BUFFER          0x8892
#define GL_ELEMENT_ARRAY_BUFFER  0x8893
#define GL_UNIFORM_BUFFER        0x8A11
#define GL_SHADER_STORAGE_BUFFER 0x90D2

// Buffer usage
#define GL_STATIC_DRAW           0x88E4
#define GL_DYNAMIC_DRAW          0x88E8
#define GL_STREAM_DRAW           0x88E0

// Primitive types
#define GL_POINTS                0x0000
#define GL_LINES                 0x0001
#define GL_LINE_STRIP            0x0003
#define GL_TRIANGLES             0x0004
#define GL_TRIANGLE_STRIP        0x0005

// Texture targets
#define GL_TEXTURE_2D            0x0DE1

// Texture parameters
#define GL_TEXTURE_MIN_FILTER    0x2801
#define GL_TEXTURE_MAG_FILTER    0x2800
#define GL_TEXTURE_WRAP_S        0x2802
#define GL_TEXTURE_WRAP_T        0x2803

// Texture filter values
#define GL_NEAREST               0x2600
#define GL_LINEAR                0x2601
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR  0x2703

// Texture wrap values
#define GL_REPEAT                0x2901
#define GL_CLAMP_TO_EDGE         0x812F
#define GL_MIRRORED_REPEAT       0x8370

// Texture active slots
#define GL_TEXTURE0              0x84C0

// Pixel formats
#define GL_RED                   0x1903
#define GL_RGB                   0x1907
#define GL_RGBA                  0x1908
#define GL_DEPTH_COMPONENT       0x1902
#define GL_DEPTH_STENCIL         0x84F9

// Internal formats
#define GL_R8                    0x8229
#define GL_RGB8                  0x8051
#define GL_RGBA8                 0x8058
#define GL_RGBA16F               0x881A
#define GL_RGBA32F               0x8814
#define GL_DEPTH_COMPONENT32F    0x8CAC
#define GL_DEPTH24_STENCIL8      0x88F0

// Pixel types
#define GL_UNSIGNED_BYTE         0x1401
#define GL_UNSIGNED_INT          0x1405
#define GL_UNSIGNED_SHORT        0x1403
#define GL_FLOAT                 0x1406
#define GL_UNSIGNED_INT_24_8     0x84FA

// Shader types
#define GL_VERTEX_SHADER         0x8B31
#define GL_FRAGMENT_SHADER       0x8B30
#define GL_GEOMETRY_SHADER       0x8DD9
#define GL_COMPUTE_SHADER        0x91B9

// Shader status
#define GL_COMPILE_STATUS        0x8B81
#define GL_LINK_STATUS           0x8B82
#define GL_INFO_LOG_LENGTH       0x8B84

// Framebuffer
#define GL_FRAMEBUFFER           0x8D40
#define GL_READ_FRAMEBUFFER      0x8CA8
#define GL_DRAW_FRAMEBUFFER      0x8CA9
#define GL_COLOR_ATTACHMENT0     0x8CE0
#define GL_DEPTH_ATTACHMENT      0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE  0x8CD5

// Renderbuffer
#define GL_RENDERBUFFER          0x8D41

// Enable caps
#define GL_DEPTH_TEST            0x0B71
#define GL_SCISSOR_TEST          0x0C11
#define GL_BLEND                 0x0BE2
#define GL_CULL_FACE             0x0B44

// Blend functions
#define GL_ZERO                  0
#define GL_ONE                   1
#define GL_SRC_ALPHA             0x0302
#define GL_ONE_MINUS_SRC_ALPHA   0x0303
#define GL_DST_COLOR             0x0306
#define GL_ONE_MINUS_DST_COLOR   0x0307
#define GL_SRC_COLOR             0x0300

// Blend equations
#define GL_FUNC_ADD              0x8006

// Depth functions
#define GL_NEVER                 0x0200
#define GL_LESS                  0x0201
#define GL_EQUAL                 0x0202
#define GL_LEQUAL                0x0203
#define GL_GREATER               0x0204
#define GL_GEQUAL                0x0206
#define GL_ALWAYS                0x0207

// Draw / read buffer sentinel for depth-only FBOs.
#define GL_NONE                  0x0000

// Cull face
#define GL_FRONT                 0x0404
#define GL_BACK                  0x0405
#define GL_FRONT_AND_BACK        0x0408

// Polygon mode (desktop GL only — WebGL/GLES expose no enum)
#define GL_POINT                 0x1B00
#define GL_LINE                  0x1B01
#define GL_FILL                  0x1B02

// Front face
#define GL_CW                    0x0900
#define GL_CCW                   0x0901

// Clear bits
#define GL_COLOR_BUFFER_BIT      0x00004000
#define GL_DEPTH_BUFFER_BIT      0x00000100
#define GL_STENCIL_BUFFER_BIT    0x00000400

// GetString
#define GL_VENDOR                0x1F00
#define GL_RENDERER              0x1F01
#define GL_VERSION               0x1F02

// GetIntegerv
#define GL_MAX_TEXTURE_SIZE      0x0D33
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_COMPUTE_WORK_GROUP_COUNT     0x91BE
#define GL_MAX_COMPUTE_WORK_GROUP_SIZE      0x91BF
#define GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS 0x90DD

// Memory barrier bits
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT   0x00000001
#define GL_ELEMENT_ARRAY_BARRIER_BIT         0x00000002
#define GL_UNIFORM_BARRIER_BIT               0x00000004
#define GL_TEXTURE_FETCH_BARRIER_BIT         0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT   0x00000020
#define GL_COMMAND_BARRIER_BIT               0x00000040
#define GL_PIXEL_BUFFER_BARRIER_BIT          0x00000080
#define GL_TEXTURE_UPDATE_BARRIER_BIT        0x00000100
#define GL_BUFFER_UPDATE_BARRIER_BIT         0x00000200
#define GL_FRAMEBUFFER_BARRIER_BIT           0x00000400
#define GL_TRANSFORM_FEEDBACK_BARRIER_BIT    0x00000800
#define GL_ATOMIC_COUNTER_BARRIER_BIT        0x00001000
#define GL_SHADER_STORAGE_BARRIER_BIT        0x00002000
#define GL_ALL_BARRIER_BITS                  0xFFFFFFFF

// MapBufferRange access
#define GL_MAP_READ_BIT                      0x0001
#define GL_MAP_WRITE_BIT                     0x0002

// ---------------------------------------------------------------------------
// Function pointer types
// ---------------------------------------------------------------------------
namespace nexus::rhi::gl {

// Buffer functions
extern void (*GenBuffers)(GLsizei n, GLuint* buffers);
extern void (*DeleteBuffers)(GLsizei n, const GLuint* buffers);
extern void (*BindBuffer)(GLenum target, GLuint buffer);
extern void (*BufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
extern void (*BufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);

// VAO functions
extern void (*GenVertexArrays)(GLsizei n, GLuint* arrays);
extern void (*DeleteVertexArrays)(GLsizei n, const GLuint* arrays);
extern void (*BindVertexArray)(GLuint array);

// Vertex attribute functions
extern void (*VertexAttribPointer)(GLuint index, GLint size, GLenum type,
                                   GLboolean normalized, GLsizei stride, const void* pointer);
extern void (*EnableVertexAttribArray)(GLuint index);

// Shader functions
extern GLuint (*CreateShader)(GLenum type);
extern void   (*DeleteShader)(GLuint shader);
extern void   (*ShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string,
                              const GLint* length);
extern void   (*CompileShader)(GLuint shader);
extern void   (*GetShaderiv)(GLuint shader, GLenum pname, GLint* params);
extern void   (*GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);

// Program functions
extern GLuint (*CreateProgram)();
extern void   (*DeleteProgram)(GLuint program);
extern void   (*AttachShader)(GLuint program, GLuint shader);
extern void   (*LinkProgram)(GLuint program);
extern void   (*UseProgram)(GLuint program);
extern void   (*GetProgramiv)(GLuint program, GLenum pname, GLint* params);
extern void   (*GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length,
                                   GLchar* infoLog);

// Uniform functions
extern GLint (*GetUniformLocation)(GLuint program, const GLchar* name);
extern void  (*Uniform1i)(GLint location, GLint v0);
extern void  (*Uniform1iv)(GLint location, GLsizei count, const GLint* value);
extern void  (*Uniform1f)(GLint location, GLfloat v0);
extern void  (*Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void  (*Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void  (*Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void  (*UniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
                                 const GLfloat* value);

// Texture functions
extern void (*GenTextures)(GLsizei n, GLuint* textures);
extern void (*DeleteTextures)(GLsizei n, const GLuint* textures);
extern void (*BindTexture)(GLenum target, GLuint texture);
extern void (*TexImage2D)(GLenum target, GLint level, GLint internalformat,
                          GLsizei width, GLsizei height, GLint border,
                          GLenum format, GLenum type, const void* pixels);
extern void (*TexParameteri)(GLenum target, GLenum pname, GLint param);
extern void (*GenerateMipmap)(GLenum target);
extern void (*ActiveTexture)(GLenum texture);

// Framebuffer functions
extern void (*GenFramebuffers)(GLsizei n, GLuint* framebuffers);
extern void (*DeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
extern void (*BindFramebuffer)(GLenum target, GLuint framebuffer);
extern void (*FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
                                    GLuint texture, GLint level);
extern GLenum (*CheckFramebufferStatus)(GLenum target);
extern void (*DrawBuffer)(GLenum buf);
extern void (*ReadBuffer)(GLenum src);

// Renderbuffer functions
extern void (*GenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
extern void (*DeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
extern void (*BindRenderbuffer)(GLenum target, GLuint renderbuffer);
extern void (*RenderbufferStorage)(GLenum target, GLenum internalformat,
                                   GLsizei width, GLsizei height);
extern void (*FramebufferRenderbuffer)(GLenum target, GLenum attachment,
                                       GLenum renderbuffertarget, GLuint renderbuffer);

// State functions
extern void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
extern void (*Scissor)(GLint x, GLint y, GLsizei width, GLsizei height);
extern void (*Clear)(GLbitfield mask);
extern void (*ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (*ClearDepth)(GLdouble depth);
extern void (*Enable)(GLenum cap);
extern void (*Disable)(GLenum cap);
extern void (*BlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (*BlendEquation)(GLenum mode);
extern void (*DepthFunc_)(GLenum func);  // Trailing underscore to avoid macro clash
extern void (*DepthMask)(GLboolean flag);
extern void (*CullFace)(GLenum mode);
extern void (*FrontFace)(GLenum mode);
extern void (*PolygonMode)(GLenum face, GLenum mode);

// Draw functions
extern void (*DrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (*DrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);

// Query functions
extern GLenum        (*GetError)();
extern const GLubyte* (*GetString)(GLenum name);
extern void          (*GetIntegerv)(GLenum pname, GLint* data);

// Compute & SSBO functions (OpenGL 4.3+)
extern void  (*DispatchCompute)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
extern void  (*MemoryBarrier)(GLbitfield barriers);
extern void  (*BindBufferBase)(GLenum target, GLuint index, GLuint buffer);
extern void  (*BindBufferRange)(GLenum target, GLuint index, GLuint buffer,
                                GLintptr offset, GLsizeiptr size);
extern void* (*MapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length,
                               GLbitfield access);
extern GLboolean (*UnmapBuffer)(GLenum target);
extern void  (*Uniform1ui)(GLint location, GLuint v0);
extern void  (*GetBufferSubData)(GLenum target, GLintptr offset,
                                 GLsizeiptr size, void* data);

// ---------------------------------------------------------------------------
// Loader: call once after creating an OpenGL context
// ---------------------------------------------------------------------------
using GLLoadProc = void* (*)(const char*);
bool load(GLLoadProc proc);

} // namespace nexus::rhi::gl
