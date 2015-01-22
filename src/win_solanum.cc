#include "system_includes.h"
#include "imgui_helpers.h"

#define snprintf sprintf_s

// Platform services:
void platform_alert();
void platform_quit();
struct TimerState;
void platform_save_state(TimerState* state);

#include "solanum.h"

static HGLRC g_glcontext_handle;
static TimerState g_timer_state;
bool32 g_running = true;
bool32 g_alert_flag;

void platform_quit()
{
    g_running = false;
}

void platform_alert()
{
    g_alert_flag = true;
}

void platform_save_state(TimerState* state)
{
    FILE* fd = fopen("solanum.dat", "wb");
    assert(fd);
    fwrite(&state->num_records, (size_t)(sizeof(int64)), 1, fd);
    fwrite(state->records, sizeof(TimeRecord), (size_t)state->num_records, fd);
    fclose(fd);
}

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

static GLuint gl_compile_shader(const char* src, GLuint type)
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

static void gl_link_program(GLuint obj, GLuint shaders[], int64 num_shaders)
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

// Will setup an OpenGL 3.3 core profile context.
// Loads functions with GLEW
static void win32_setup_context(HWND window, HGLRC* context)
{
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
    format_index = ChoosePixelFormat(GetDC(window), &pfd);

    // make that the pixel format of the device context
    bool32 succeeded = SetPixelFormat(GetDC(window), format_index, &pfd);

    if (!succeeded)
    {
        OutputDebugStringA("Could not set pixel format\n");
        g_running = false;
        return;
    }

    HGLRC dummy_context = wglCreateContext(GetDC(window));
    if (!dummy_context)
    {
        OutputDebugStringA("Could not create GL context. Exiting");
        g_running = false;
        return;
    }
    wglMakeCurrent(GetDC(window), dummy_context);
    if (!succeeded)
    {
        OutputDebugStringA("Could not set current GL context. Exiting");
        g_running = false;
        return;
    }

    GLenum glew_result = glewInit();
    if (glew_result != GLEW_OK)
    {
        OutputDebugStringA("Could not init glew.\n");
        g_running = false;
        return;
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
        g_running = false;
        return;
    }

    succeeded = false;
    for (uint32 i = 0; i < num_formats - 1; ++i)
    {
        int local_index = (&format_index)[i];
        succeeded = SetPixelFormat(GetDC(window), local_index, &pfd);
        if (succeeded)
            return;
    }
    if (!succeeded)
    {
        OutputDebugStringA("Could not set pixel format for final rendering context.\n");
        g_running = false;
        return;
    }

    const int context_attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    *context = wglCreateContextAttribsARB(GetDC(window), 0/*shareContext*/,
            context_attribs);
    wglMakeCurrent(GetDC(window), *context);
}

static void win32_process_input(HWND window, TimerState* state)
{
    MSG message;
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            g_running = false;
        }
        switch (message.message)
        {
        case WM_LBUTTONDOWN:
            {
                // FIXME: clicks that last less than one frame will be missed.
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDown[0] = true;
                break;
            }
        case WM_LBUTTONUP:
            {
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDown[0] = false;
                break;
            }
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_KEYDOWN:
            {
                uint32 vkcode = (uint32)message.wParam;
                bool32 was_down = ((message.lParam & (1 << 30)) != 0);
                bool32 is_down  = ((message.lParam & (1 << 31)) == 0);
                bool32 alt_key_was_down = (message.lParam & (1 << 29));
                if (was_down && vkcode == VK_ESCAPE)
                {
                    g_running = false;
                }
            }
        default:
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
                break;
            }
        }

    }
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
            win32_setup_context(window, &g_glcontext_handle);
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
                    //"    gl_Position = proj * vec4(pos.xy,0,1);\n"
                    //"    gl_Position = proj * vec4(pos.xy + vec2(0.375, 0.375),0,1);\n"
                    "    gl_Position = proj * vec4(pos.xy + vec2(0.5, 0.5),0,1);\n"
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
                g_imgui_program = glCreateProgram();
                gl_link_program(g_imgui_program, shader_handles, 2);

                GLint texture_location  = GLCHK( glGetUniformLocation(g_imgui_program, "sampler"));
                GLint proj_mtx_location = GLCHK( glGetUniformLocation(g_imgui_program, "proj"));
                GLint position_location = GLCHK( glGetAttribLocation(g_imgui_program, "pos"));
                GLint uv_location       = GLCHK( glGetAttribLocation(g_imgui_program, "texcoord"));
                GLint color_location    = GLCHK( glGetAttribLocation(g_imgui_program, "color"));
                assert(texture_location  >= 0);
                assert(proj_mtx_location >= 0);
                assert(position_location >= 0);
                assert(uv_location       >= 0);
                assert(color_location    >= 0);

                g_imgui_vbo_size = 2048;  // We will stretch it later, probably.
                GLCHK(glGenBuffers(1, &g_imgui_vbo));
                glBindBuffer(GL_ARRAY_BUFFER, g_imgui_vbo);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)g_imgui_vbo_size, NULL, GL_DYNAMIC_DRAW);

                GLCHK(glGenVertexArrays(1, &g_imgui_vao));
                glBindVertexArray(g_imgui_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_imgui_vbo);
                GLCHK(glEnableVertexAttribArray((GLuint)position_location));
                glEnableVertexAttribArray((GLuint)uv_location);
                GLCHK(glEnableVertexAttribArray((GLuint)color_location));

                glVertexAttribPointer((GLuint)position_location, 2,
                        GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, pos));
                glVertexAttribPointer((GLuint)uv_location, 2,
                        GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, uv));
                GLCHK(glVertexAttribPointer((GLuint)color_location, 4,
                            GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert),
                            (GLvoid*)offsetof(ImDrawVert, col)));
                GLCHK(glBindVertexArray(0));
                GLCHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
            }
            // Init imgui
            {
                ImGuiIO& io = ImGui::GetIO();
                io.DeltaTime = 1.0f / 30.0f;
                // TODO: io.KeyMap is not set

                io.RenderDrawListsFn = ImImpl_RenderDrawLists;
                io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
                io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;

                unsigned char* pixels;
                int width, height;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

                GLuint tex_id;
                glGenTextures(1, &tex_id);
                glBindTexture(GL_TEXTURE_2D, tex_id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

                // Store our identifier
                io.Fonts->TexID = (void *)(intptr_t)tex_id;
            }
            break;
        }
    case WM_DESTROY:
        {
            g_running = false;
        }
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);

            EndPaint(window, &paint);
            break;
        }

#if 0
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_KEYDOWN:
        {
            break;
        }
#endif
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

    int x = 100;
    int y = 100;
    int width = 400;
    int height = 500;
    HWND window = CreateWindowExA(
            0, //WS_EX_TOPMOST ,  // dwExStyle
            window_class.lpszClassName,     // class Name
            "Solanum",                      // window name
            //WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | WS_POPUP,          // dwStyle
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
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

    TimerState state = {};
    {
        state.time_unit_in_s = 60 * 30;
        state.records_size = 1024;
        state.records = (TimeRecord*) malloc(state.records_size * sizeof(TimeRecord));
        {
            FILE* fd = fopen("solanum.dat", "rb");
            if (fd)
            {
                fread(&state.num_records, (size_t)(sizeof(int64)), 1, fd);
                fread(state.records, sizeof(TimeRecord), (size_t)state.num_records, fd);
                fclose(fd);
            }
        }

        time_t one_day_ago;
        time(&one_day_ago);
        one_day_ago -= 60 * 24;
        for (int i = 0; i < state.num_records; ++i)
        {
            if (state.records[i].timestamp >= one_day_ago)
                state.num_seconds += state.records[i].elapsed;
        }
    }
    state.window_width = width;
    state.window_height = height;
    while (g_running)
    {
        if (g_alert_flag)
        {
            MessageBox(window, "Timer done", "Solanum", MB_OK | MB_SYSTEMMODAL);
            g_alert_flag = false;
        }
        win32_process_input(window, &state);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)width, (float)height);
        // Set mouse position
        {
            POINT mouse;
            GetCursorPos(&mouse);
            ScreenToClient(window, &mouse);
            io.MousePos = ImVec2((float)mouse.x, (float)mouse.y);
        }
        glClearColor(0.5f, 0.5f, 0.5f, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		ImGui::NewFrame();
        timer_step_and_render(&state);
        ImGui::Render();
        SwapBuffers(GetDC(window));
        // Sleep for a while
        Sleep(30);
    }

    return TRUE;
}
