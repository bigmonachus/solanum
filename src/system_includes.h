#ifdef _WIN32
#pragma warning(push, 0)
#include <windows.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/wglew.h>
#elif defined(__linux__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <unistd.h>
#endif
// Platform independent includes:
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __MACH__

#include <GL/GLEW.h>


#endif

#include <imgui.h>


#ifdef _WIN32
#pragma warning(pop)
#endif

