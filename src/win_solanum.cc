#pragma warning(push, 0)
#include <windows.h>

// Platform independent includes:
#include <stdint.h>

// Local includes
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/wglew.h>
#include <imgui/imgui.h>
#include <stdio.h> // fprintf
#pragma warning(pop)

typedef int32_t bool32;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;


static HGLRC g_glcontext_handle;
static GLuint g_imgui_vbo;
static int g_imgui_vbo_size;
static GLuint g_imgui_vao;

#define GLCHK(stmt) stmt; gl_query_error(#stmt, __FILE__, __LINE__)
inline void gl_query_error(const char* expr, const char* file, int line)
{
    GLenum err = glGetError();
    const char* str = "";
    if (err != GL_NO_ERROR)
    {
        char buffer[256];
        switch(err)
        {
        case GL_INVALID_ENUM:
            str = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            str = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            str = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            str = "GL_OUT_OF_MEMORY";
            break;
        case GL_STACK_OVERFLOW:
            str = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            str = "GL_STACK_UNDERFLOW";
            break;
        default:
            str = "SOME GL ERROR";
        }
        sprintf_s(buffer, 256, "%s in: %s:%d\n", str, file, line);
        OutputDebugStringA(buffer);
        sprintf_s(buffer, 256, "   ---- Expression: %s\n", expr);
        OutputDebugStringA(buffer);
    }
}

GLuint gl_compile_shader(const char* src, GLuint type)
{
    GLuint obj = glCreateShader(type);
    GLCHK ( glShaderSource(obj, 1, &src, NULL) );
    GLCHK ( glCompileShader(obj) );
    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetShaderiv(obj, GL_COMPILE_STATUS, &res) );
    if (!res)
    {
        GLint length;
        GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
        char* log = (char*)malloc((size_t)length);
        GLsizei written_len;
        GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        OutputDebugStringA("Shader compilation failed. \n    ---- Info log:\n");
        OutputDebugStringA(log);
        free(log);
    }
    return obj;
}
void gl_link_program(GLuint obj, GLuint shaders[], int64 num_shaders)
{
    assert(glIsProgram(obj));
    for (int i = 0; i < num_shaders; ++i)
    {
        assert(glIsShader(shaders[i]));
        GLCHK ( glAttachShader(obj, shaders[i]) );
    }
    GLCHK ( glLinkProgram(obj) );

    // ERROR CHECKING
    int res = 0;
    GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    if (!res)
    {
        OutputDebugStringA("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = (char*)malloc((size_t)len);
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        OutputDebugStringA(log);
        free(log);
    }
    GLCHK ( glValidateProgram(obj) );
}
LRESULT APIENTRY WndProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
        )
{
    LRESULT result = 0;
    switch (message)
    {
    case WM_CREATE:
        {
            HDC dc = GetDC(window);
            int format_index = 0;

            PIXELFORMATDESCRIPTOR pfd = {
                sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
                1,                     // version number
                PFD_DRAW_TO_WINDOW |   // support window
                    PFD_SUPPORT_OPENGL |   // support OpenGL
                    PFD_DOUBLEBUFFER,      // double buffered
                PFD_TYPE_RGBA,         // RGBA type
                32,                    // 32-bit color depth
                0, 0, 0, 0, 0, 0,      // color bits ignored
                0,                     // no alpha buffer
                0,                     // shift bit ignored
                0,                     // no accumulation buffer
                0, 0, 0, 0,            // accum bits ignored
                24,                    // 24-bit z-buffer
                8,                     // 8-bit stencil buffer
                0,                     // no auxiliary buffer
                PFD_MAIN_PLANE,        // main layer
                0,                     // reserved
                0, 0, 0                // layer masks ignored
            };

            // get the best available match of pixel format for the device context
            format_index = ChoosePixelFormat(dc, &pfd);

            // make that the pixel format of the device context
            bool32 succeeded = SetPixelFormat(dc, format_index, &pfd);

			if (!succeeded)
			{
				OutputDebugStringA("Could not set pixel format\n");
				PostQuitMessage(0);
				break;
			}

            HGLRC dummy_context = wglCreateContext(dc);
            if (!dummy_context)
            {
                OutputDebugStringA("Could not create GL context. Exiting");
                PostQuitMessage(0);
				break;
            }
            wglMakeCurrent(dc, dummy_context);
            if (!succeeded)
            {
                OutputDebugStringA("Could not set current GL context. Exiting");
                PostQuitMessage(0);
				break;
            }

            GLenum glew_result = glewInit();
            if (glew_result != GLEW_OK)
            {
                OutputDebugStringA("Could not init glew.\n");
                PostQuitMessage(0);
				break;
            }
            const int pixel_attribs[] =
            {
                WGL_ACCELERATION_ARB   , WGL_FULL_ACCELERATION_ARB           ,
                WGL_DRAW_TO_WINDOW_ARB , GL_TRUE           ,
                WGL_SUPPORT_OPENGL_ARB , GL_TRUE           ,
                WGL_DOUBLE_BUFFER_ARB  , GL_TRUE           ,
                WGL_PIXEL_TYPE_ARB     , WGL_TYPE_RGBA_ARB ,
                WGL_COLOR_BITS_ARB     , 32                ,
                WGL_DEPTH_BITS_ARB     , 24                ,
                WGL_STENCIL_BITS_ARB   , 8                 ,
                0                      ,
            };
            UINT num_formats = 0;
            wglChoosePixelFormatARB(GetDC(window), pixel_attribs, NULL, 10 /*max_formats*/, &format_index, &num_formats);
            if (!num_formats)
            {
                OutputDebugStringA("Could not choose pixel format. Exiting.");
                PostQuitMessage(0);
				break;
            }

            succeeded = false;
            for (uint32 i = 0; i < num_formats - 1; ++i)
            {
                int local_index = (&format_index)[i];
                succeeded = SetPixelFormat(GetDC(window), local_index, &pfd);
                if (succeeded)
                    break;
            }
            if (!succeeded)
            {
                OutputDebugStringA("Could not set pixel format for final rendering context.\n");
                PostQuitMessage(0);
				break;
            }

            const int context_attribs[] =
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            g_glcontext_handle = wglCreateContextAttribsARB(GetDC(window), 0/*shareContext*/,
                    context_attribs);
            wglMakeCurrent(GetDC(window), g_glcontext_handle);

            // Init shaders for imgui
            {
                const GLchar* vertex_shader =
                    "#version 330\n"
                    "uniform mat4 proj;\n"
                    "in vec2 pos;\n"
                    "in vec2 texcoord;\n"
                    "in vec4 color;\n"
                    "out vec2 frag_texcoord;\n"
                    "out vec4 frag_color;\n"
                    "void main()\n"
                    "{\n"
                    "    frag_texcoord = texcoord;\n"
                    "    frag_color = color;\n"
                    "    gl_Position = proj * vec4(pos.xy,0,1);\n"
                    "}\n";

                const GLchar* fragment_shader =
                    "#version 330\n"
                    "uniform sampler2D sampler;\n"
                    "in vec2 frag_texcoord;\n"
                    "in vec4 frag_color;\n"
                    "out vec4 Out_color;\n"
                    "void main()\n"
                    "{\n"
                    "    Out_color = frag_color * texture( sampler, frag_texcoord.st);\n"
                    "}\n";

                GLuint shader_handles[2];
                shader_handles[0] = gl_compile_shader(vertex_shader, GL_VERTEX_SHADER);
                shader_handles[1] = gl_compile_shader(fragment_shader, GL_FRAGMENT_SHADER);
                GLuint program_handle = glCreateProgram();
                gl_link_program(program_handle, shader_handles, 2);

                GLint texture_location  = GLCHK( glGetUniformLocation(program_handle, "sampler"));
                GLint proj_mtx_location = GLCHK( glGetUniformLocation(program_handle, "proj"));
                GLint position_location = GLCHK( glGetAttribLocation(program_handle, "pos"));
                GLint uv_location       = GLCHK( glGetAttribLocation(program_handle, "texcoord"));
                GLint color_location    = GLCHK( glGetAttribLocation(program_handle, "color"));
                assert(texture_location  >= 0);
                assert(proj_mtx_location >= 0);
                assert(position_location >= 0);
                assert(uv_location       >= 0);
                assert(color_location    >= 0);

                g_imgui_vbo_size = 2048;  // We will stretch it later, probably.
                GLCHK(glGenBuffers(1, &g_imgui_vbo));
                glBindBuffer(GL_ARRAY_BUFFER, g_imgui_vbo);
                GLCHK(glBufferData(GL_ARRAY_BUFFER, g_imgui_vbo_size, NULL, GL_DYNAMIC_DRAW));

                GLCHK(glGenVertexArrays(1, &g_imgui_vao));
                glBindVertexArray(g_imgui_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_imgui_vbo);
                GLCHK(glEnableVertexAttribArray((GLuint)position_location));
                glEnableVertexAttribArray((GLuint)uv_location);
                GLCHK(glEnableVertexAttribArray((GLuint)color_location));

                glVertexAttribPointer((GLuint)position_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, pos));
                glVertexAttribPointer((GLuint)uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, uv));
                GLCHK(glVertexAttribPointer((GLuint)color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, col)));
                GLCHK(glBindVertexArray(0));
                GLCHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
            }
            break;
        }
    case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
    case WM_PAINT:
        {
            glClearColor(0,1,0,0.5);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            SwapBuffers(GetDC(window));
            break;
        }

    default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        }
    };
    return result;
}

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    WNDCLASS window_class = {};

    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = hInstance;
    window_class.hIcon = NULL;
    window_class.hCursor = LoadCursor(0, IDC_ARROW);
    window_class.lpszClassName = "SolanumClass";

    if (!RegisterClassA(&window_class))
    {
        return FALSE;
    }

    int x = 0;
    int y = 0;
    int width = 1024;
    int height = 560;
    HWND window = CreateWindowExA(
            0, //WS_EX_TOPMOST ,  // dwExStyle
            window_class.lpszClassName,     // class Name
            "Solanum",                      // window name
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | WS_POPUP,          // dwStyle
            x,                      // x
            y,                      // y
            width,                  // width
            height,                 // height
            0,                                  // parent
            0,                                  // menu
            hInstance,                          // hInstance
            0                                   // lpParam
            );

    if (!window)
    {
        return FALSE;
    }

    HDC device_context = GetDC(window);

    MSG message;
    while(GetMessage(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return TRUE;
}
