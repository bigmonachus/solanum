#include "system_includes.h"
#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "imgui_impl_sdl_gl3.h"

#include <fcntl.h>
#ifndef _WIN32
#define MAX_PATH 1024
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#include <errno.h>

#if defined(__MACH__)
#include <mach-o/dyld.h>
#endif

// Platform services:
void platform_alert();
void platform_quit();
struct TimerState;
void platform_save_state(TimerState* state);

#include "solanum.h"


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

void path_at_exe(char* full_path, int buffer_size, char* fname)
{
#if defined(_WIN32)
    GetModuleFileName(NULL, full_path, (DWORD)buffer_size);
    {  // Trim to backslash
        int len = 0;
        for(int i = 0; i < buffer_size; ++i)
        {
            char c = full_path[i];
            if (!c)
                break;
            if (c == '\\')
                len = i;
        }
        full_path[len + 1] = '\0';
    }

    strcat(full_path, fname);
#elif defined(__MACH__)
    uint32_t size = 0;
    _NSGetExecutablePath(full_path, &size);
    puts(full_path);
    strcat(full_path, fname);
    puts(full_path);
#elif defined(__linux__)

#if 1
    char* dropbox_path = "/Dropbox/solanum/";
    full_path[0] = '\0';
    strcat(full_path, getenv("HOME"));
    strcat(full_path, dropbox_path);
    strcat(full_path, fname);

#else  // Actual implementation

    size_t read = readlink("/proc/self/exe", full_path, buffer_size);
    if (read > 0 && read + 1 < buffer_size)
    {
        int last_slash_i = 0;
        for (int i = 0; i < read; ++i)
        {
            if (full_path[i] == '/')
            {
                last_slash_i = i;
            }
        }
        full_path[last_slash_i + 1] = 0;

        strcat(full_path, fname);
    }
#endif // if 0

#endif
}

int cp(const char *from, const char *to)
{
#ifdef _WIN32
    CopyFile(from, to, FALSE);
    return 0;
#else
    int fd_to, fd_from;
    char buf[4096];
    size_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        size_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
#endif
}

void platform_save_state(TimerState* state)
{
    static int num_backups = 5;
    static int backup_i = 0;

    char data_path[MAX_PATH];
    path_at_exe(data_path, MAX_PATH, "solanum.dat");
    char backup_path[MAX_PATH];
    char fname[MAX_PATH];
    sprintf(fname, "BAK%d_solanum.dat", backup_i);
    path_at_exe(backup_path, MAX_PATH, fname);

    cp(data_path, backup_path);

    FILE* fd = fopen(data_path, "wb");
    assert(fd);
    fwrite(&state->num_records, (size_t)(sizeof(int64)), 1, fd);
    fwrite(state->records, sizeof(TimeRecord), (size_t)state->num_records, fd);
    fclose(fd);

    backup_i = (backup_i + 1) % num_backups;
}

uint32 timer_callback(Uint32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return(interval);
}

#ifdef _WIN32
int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nCmdShow
)
#else
int main()
#endif
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    int width = 500;
    int height = 288;
    // Setup window
    /* SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); */
    /* SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); */
    /* SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8); */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    SDL_Window* window = SDL_CreateWindow("Solanum",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        printf("Could not generate GL context\n");
        exit(-1);
    }

    glewExperimental = true;
    GLenum glew_err = glewInit();

    if (glew_err != GLEW_OK) {
        printf("glewInit failed with error: %s\nExiting.\n",
                   glewGetErrorString(glew_err));
        exit(EXIT_FAILURE);
    }

    if (GLEW_VERSION_1_4) {
        if ( glewIsSupported("GL_ARB_shader_objects "
                             "GL_ARB_vertex_program "
                             "GL_ARB_fragment_program "
                             "GL_ARB_vertex_buffer_object ") ) {
            printf("[DEBUG] GL OK.\n");
        } else {
            printf("One or more OpenGL extensions are not supported.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        printf("OpenGL 1.4 not supported.\n");
        exit(EXIT_FAILURE);
    }

    // Setup ImGui binding
    ImGui_ImplSDLGL3_Init();

    bool show_test_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImColor(114, 144, 154);

    char data_path[MAX_PATH];
    path_at_exe(data_path, MAX_PATH, "solanum.dat");

    TimerState state = {};
    {
        state.time_unit_in_s = 60 * 30;
        state.records_size = 1024 * 1024;
        time_t current_time;
        time(&current_time);
        state.time_persp = current_time;
        state.records = (TimeRecord*) malloc(state.records_size * sizeof(TimeRecord));
        {

            FILE* fd = fopen(data_path, "rb");
            if (fd)
            {
                fread(&state.num_records, (size_t)(sizeof(int64)), 1, fd);
                if ((size_t)state.num_records > state.records_size)
                {
                    return EXIT_FAILURE;
                }
                fread(state.records, sizeof(TimeRecord), (size_t)state.num_records, fd);
                fclose(fd);
            }
        }
    }

    SDL_TimerID periodical = SDL_AddTimer(450, timer_callback, NULL);


    // Main loop
    while (g_running)
    {
        ImGuiIO& imgui_io = ImGui::GetIO();
        if (g_alert_flag)
        {
            const SDL_MessageBoxButtonData buttons[] = {
                { /* .flags, .buttonid, .text */        0, 0, "Ok" },
                //{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "ok" },
                //{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "cancel" },
            };
            const SDL_MessageBoxColorScheme colorScheme = {
                { /* .colors (.r, .g, .b) */
                    /* [SDL_MESSAGEBOX_COLOR_BACKGROUND] */
                    { 128,   128,   128 },
                    /* [SDL_MESSAGEBOX_COLOR_TEXT] */
                    {   0, 0,   0 },
                    /* [SDL_MESSAGEBOX_COLOR_BUTTON_BORDER] */
                    { 255, 255,   0 },
                    /* [SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND] */
                    {   255,   0, 255 },
                    /* [SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED] */
                    { 255,   0, 255 }
                }
            };
            const SDL_MessageBoxData messageboxdata = {
                SDL_MESSAGEBOX_INFORMATION, /* .flags */
                NULL, /* .window */
                "Timer done", /* .title */
                "Timer done!", /* .message */
                SDL_arraysize(buttons), /* .numbuttons */
                buttons, /* .buttons */
                &colorScheme /* .colorScheme */
            };
            int buttonid;
            if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
                SDL_Log("error displaying message box");
                return 1;
            }
            g_alert_flag = false;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            //ImGui_ImplSDLGL3_KeyCallback_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                g_running = false;
            }
        }
        {
            int mouse_x;
            int mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);

            imgui_io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);

            imgui_io.MouseDown[0] = (bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT));
            imgui_io.MouseDown[1] = (bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE));
            imgui_io.MouseDown[2] = (bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT));
        }
        int d_w, d_h;

        SDL_GL_GetDrawableSize(window,
                               &d_w, &d_h);
        ImGui_ImplSDLGL3_NewFrame(width, height,
                                  d_w, d_h);
        timer_step_and_render(&state);
        // Rendering
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        SDL_GL_SwapWindow(window);
        SDL_WaitEvent(NULL);
    }

    // Cleanup
    ImGui_ImplSDLGL3_Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

