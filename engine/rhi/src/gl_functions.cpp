// ============================================================================
// gl_functions.cpp - OpenGL function pointer loader implementation
// ============================================================================

#include <nexus/rhi/gl_functions.h>
#include <nexus/core/log.h>

namespace nexus::rhi::gl {

// ---------------------------------------------------------------------------
// Function pointer definitions (initially null)
// ---------------------------------------------------------------------------

// Buffers
void (*GenBuffers)(GLsizei, GLuint*)                                                   = nullptr;
void (*DeleteBuffers)(GLsizei, const GLuint*)                                          = nullptr;
void (*BindBuffer)(GLenum, GLuint)                                                     = nullptr;
void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum)                            = nullptr;
void (*BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*)                       = nullptr;

// VAO
void (*GenVertexArrays)(GLsizei, GLuint*)                                              = nullptr;
void (*DeleteVertexArrays)(GLsizei, const GLuint*)                                     = nullptr;
void (*BindVertexArray)(GLuint)                                                        = nullptr;

// Vertex attributes
void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)    = nullptr;
void (*EnableVertexAttribArray)(GLuint)                                                = nullptr;

// Shaders
GLuint (*CreateShader)(GLenum)                                                         = nullptr;
void   (*DeleteShader)(GLuint)                                                         = nullptr;
void   (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*)            = nullptr;
void   (*CompileShader)(GLuint)                                                        = nullptr;
void   (*GetShaderiv)(GLuint, GLenum, GLint*)                                          = nullptr;
void   (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*)                         = nullptr;

// Programs
GLuint (*CreateProgram)()                                                              = nullptr;
void   (*DeleteProgram)(GLuint)                                                        = nullptr;
void   (*AttachShader)(GLuint, GLuint)                                                 = nullptr;
void   (*LinkProgram)(GLuint)                                                          = nullptr;
void   (*UseProgram)(GLuint)                                                           = nullptr;
void   (*GetProgramiv)(GLuint, GLenum, GLint*)                                         = nullptr;
void   (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*)                        = nullptr;

// Uniforms
GLint (*GetUniformLocation)(GLuint, const GLchar*)                                     = nullptr;
void  (*Uniform1i)(GLint, GLint)                                                       = nullptr;
void  (*Uniform1iv)(GLint, GLsizei, const GLint*)                                      = nullptr;
void  (*Uniform1f)(GLint, GLfloat)                                                     = nullptr;
void  (*Uniform2f)(GLint, GLfloat, GLfloat)                                            = nullptr;
void  (*Uniform3f)(GLint, GLfloat, GLfloat, GLfloat)                                   = nullptr;
void  (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat)                          = nullptr;
void  (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*)                   = nullptr;

// Textures
void (*GenTextures)(GLsizei, GLuint*)                                                  = nullptr;
void (*DeleteTextures)(GLsizei, const GLuint*)                                         = nullptr;
void (*BindTexture)(GLenum, GLuint)                                                    = nullptr;
void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
void (*TexParameteri)(GLenum, GLenum, GLint)                                           = nullptr;
void (*GenerateMipmap)(GLenum)                                                         = nullptr;
void (*ActiveTexture)(GLenum)                                                          = nullptr;

// Framebuffers
void   (*GenFramebuffers)(GLsizei, GLuint*)                                            = nullptr;
void   (*DeleteFramebuffers)(GLsizei, const GLuint*)                                   = nullptr;
void   (*BindFramebuffer)(GLenum, GLuint)                                              = nullptr;
void   (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint)                 = nullptr;
GLenum (*CheckFramebufferStatus)(GLenum)                                               = nullptr;

// Renderbuffers
void (*GenRenderbuffers)(GLsizei, GLuint*)                                             = nullptr;
void (*DeleteRenderbuffers)(GLsizei, const GLuint*)                                    = nullptr;
void (*BindRenderbuffer)(GLenum, GLuint)                                               = nullptr;
void (*RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei)                          = nullptr;
void (*FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint)                        = nullptr;

// State
void (*Viewport)(GLint, GLint, GLsizei, GLsizei)                                      = nullptr;
void (*Scissor)(GLint, GLint, GLsizei, GLsizei)                                       = nullptr;
void (*Clear)(GLbitfield)                                                              = nullptr;
void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat)                                = nullptr;
void (*ClearDepth)(GLdouble)                                                           = nullptr;
void (*Enable)(GLenum)                                                                 = nullptr;
void (*Disable)(GLenum)                                                                = nullptr;
void (*BlendFunc)(GLenum, GLenum)                                                      = nullptr;
void (*BlendEquation)(GLenum)                                                          = nullptr;
void (*DepthFunc_)(GLenum)                                                             = nullptr;
void (*DepthMask)(GLboolean)                                                           = nullptr;
void (*CullFace)(GLenum)                                                               = nullptr;
void (*FrontFace)(GLenum)                                                              = nullptr;
void (*PolygonMode)(GLenum, GLenum)                                                    = nullptr;

// Draw
void (*DrawArrays)(GLenum, GLint, GLsizei)                                            = nullptr;
void (*DrawElements)(GLenum, GLsizei, GLenum, const void*)                            = nullptr;

// Query
GLenum        (*GetError)()                                                            = nullptr;
const GLubyte* (*GetString)(GLenum)                                                    = nullptr;
void          (*GetIntegerv)(GLenum, GLint*)                                           = nullptr;

// Compute & SSBO
void  (*DispatchCompute)(GLuint, GLuint, GLuint)                                       = nullptr;
void  (*MemoryBarrier)(GLbitfield)                                                     = nullptr;
void  (*BindBufferBase)(GLenum, GLuint, GLuint)                                        = nullptr;
void  (*BindBufferRange)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)                 = nullptr;
void* (*MapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield)                      = nullptr;
GLboolean (*UnmapBuffer)(GLenum)                                                       = nullptr;
void  (*Uniform1ui)(GLint, GLuint)                                                     = nullptr;
void  (*GetBufferSubData)(GLenum, GLintptr, GLsizeiptr, void*)                         = nullptr;

// ---------------------------------------------------------------------------
// Loader helper
// ---------------------------------------------------------------------------

template <typename T>
static bool load_fn(GLLoadProc proc, T& fn_ptr, const char* name) {
    fn_ptr = reinterpret_cast<T>(proc(name));
    if (!fn_ptr) {
        NX_ERROR("Failed to load GL function: {}", name);
        return false;
    }
    return true;
}

// Same as load_fn() but silent on miss — for optional entry points such as
// GL 4.3+ compute that are simply absent on macOS (Apple capped GL at 4.1).
template <typename T>
static bool load_fn_optional(GLLoadProc proc, T& fn_ptr, const char* name) {
    fn_ptr = reinterpret_cast<T>(proc(name));
    return fn_ptr != nullptr;
}

bool load(GLLoadProc proc) {
    if (!proc) {
        NX_ERROR("GL load proc is null");
        return false;
    }

    bool ok = true;

    // Macro to reduce boilerplate
    #define LOAD(fn, glName) ok = load_fn(proc, fn, #glName) && ok

    // Buffers
    LOAD(GenBuffers,        glGenBuffers);
    LOAD(DeleteBuffers,     glDeleteBuffers);
    LOAD(BindBuffer,        glBindBuffer);
    LOAD(BufferData,        glBufferData);
    LOAD(BufferSubData,     glBufferSubData);

    // VAO
    LOAD(GenVertexArrays,       glGenVertexArrays);
    LOAD(DeleteVertexArrays,    glDeleteVertexArrays);
    LOAD(BindVertexArray,       glBindVertexArray);

    // Vertex attributes
    LOAD(VertexAttribPointer,     glVertexAttribPointer);
    LOAD(EnableVertexAttribArray, glEnableVertexAttribArray);

    // Shaders
    LOAD(CreateShader,      glCreateShader);
    LOAD(DeleteShader,      glDeleteShader);
    LOAD(ShaderSource,      glShaderSource);
    LOAD(CompileShader,     glCompileShader);
    LOAD(GetShaderiv,       glGetShaderiv);
    LOAD(GetShaderInfoLog,  glGetShaderInfoLog);

    // Programs
    LOAD(CreateProgram,     glCreateProgram);
    LOAD(DeleteProgram,     glDeleteProgram);
    LOAD(AttachShader,      glAttachShader);
    LOAD(LinkProgram,       glLinkProgram);
    LOAD(UseProgram,        glUseProgram);
    LOAD(GetProgramiv,      glGetProgramiv);
    LOAD(GetProgramInfoLog, glGetProgramInfoLog);

    // Uniforms
    LOAD(GetUniformLocation, glGetUniformLocation);
    LOAD(Uniform1i,          glUniform1i);
    LOAD(Uniform1iv,         glUniform1iv);
    LOAD(Uniform1f,          glUniform1f);
    LOAD(Uniform2f,          glUniform2f);
    LOAD(Uniform3f,          glUniform3f);
    LOAD(Uniform4f,          glUniform4f);
    LOAD(UniformMatrix4fv,   glUniformMatrix4fv);

    // Textures
    LOAD(GenTextures,     glGenTextures);
    LOAD(DeleteTextures,  glDeleteTextures);
    LOAD(BindTexture,     glBindTexture);
    LOAD(TexImage2D,      glTexImage2D);
    LOAD(TexParameteri,   glTexParameteri);
    LOAD(GenerateMipmap,  glGenerateMipmap);
    LOAD(ActiveTexture,   glActiveTexture);

    // Framebuffers
    LOAD(GenFramebuffers,        glGenFramebuffers);
    LOAD(DeleteFramebuffers,     glDeleteFramebuffers);
    LOAD(BindFramebuffer,        glBindFramebuffer);
    LOAD(FramebufferTexture2D,   glFramebufferTexture2D);
    LOAD(CheckFramebufferStatus, glCheckFramebufferStatus);

    // Renderbuffers
    LOAD(GenRenderbuffers,        glGenRenderbuffers);
    LOAD(DeleteRenderbuffers,     glDeleteRenderbuffers);
    LOAD(BindRenderbuffer,        glBindRenderbuffer);
    LOAD(RenderbufferStorage,     glRenderbufferStorage);
    LOAD(FramebufferRenderbuffer, glFramebufferRenderbuffer);

    // State
    LOAD(Viewport,       glViewport);
    LOAD(Scissor,        glScissor);
    LOAD(Clear,          glClear);
    LOAD(ClearColor,     glClearColor);
    LOAD(ClearDepth,     glClearDepth);
    LOAD(Enable,         glEnable);
    LOAD(Disable,        glDisable);
    LOAD(BlendFunc,      glBlendFunc);
    LOAD(BlendEquation,  glBlendEquation);
    LOAD(DepthFunc_,     glDepthFunc);
    LOAD(DepthMask,      glDepthMask);
    LOAD(CullFace,       glCullFace);
    LOAD(PolygonMode,    glPolygonMode);
    LOAD(FrontFace,      glFrontFace);

    // Draw
    LOAD(DrawArrays,   glDrawArrays);
    LOAD(DrawElements, glDrawElements);

    // Query
    LOAD(GetError,    glGetError);
    LOAD(GetString,   glGetString);
    LOAD(GetIntegerv, glGetIntegerv);

    // Compute & SSBO (4.3+). Track separately — availability is reported but
    // absence is not fatal so older contexts can still load the rest of GL.
    bool compute_ok = true;
    #define LOAD_COMPUTE(fn, glName) compute_ok = load_fn_optional(proc, fn, #glName) && compute_ok
    LOAD_COMPUTE(DispatchCompute,   glDispatchCompute);
    LOAD_COMPUTE(MemoryBarrier,     glMemoryBarrier);
    LOAD_COMPUTE(BindBufferBase,    glBindBufferBase);
    LOAD_COMPUTE(BindBufferRange,   glBindBufferRange);
    LOAD_COMPUTE(MapBufferRange,    glMapBufferRange);
    LOAD_COMPUTE(UnmapBuffer,       glUnmapBuffer);
    LOAD_COMPUTE(Uniform1ui,        glUniform1ui);
    LOAD_COMPUTE(GetBufferSubData,  glGetBufferSubData);
    #undef LOAD_COMPUTE

    if (!compute_ok) {
        // Demote to debug-level: macOS will *always* hit this since Apple
        // never shipped GL 4.3+.  Renderer subsystems that need compute
        // (e.g. GPU particles) check the function pointers themselves and
        // gracefully fall back to CPU paths.
        NX_TRACE("OpenGL compute-shader entry points unavailable (requires GL 4.3+)");
    }

    #undef LOAD

    if (ok) {
        NX_INFO("OpenGL functions loaded successfully");
        NX_INFO("  Vendor:   {}", reinterpret_cast<const char*>(GetString(GL_VENDOR)));
        NX_INFO("  Renderer: {}", reinterpret_cast<const char*>(GetString(GL_RENDERER)));
        NX_INFO("  Version:  {}", reinterpret_cast<const char*>(GetString(GL_VERSION)));
    }

    return ok;
}

} // namespace nexus::rhi::gl
