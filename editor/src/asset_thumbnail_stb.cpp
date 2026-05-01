// Single-TU implementation of stb_image, isolated from the rest of the
// editor library so its third-party warnings don't pollute the strict
// `-Werror` flags applied to first-party code.  Compiler-specific warning
// suppressions live in the build system (editor/CMakeLists.txt) — this
// file's only job is to materialize the stb_image translation unit.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_STDIO_NO_FAIL  // unused, kept for grep clarity
#include "stb_image.h"
