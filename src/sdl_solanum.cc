#include "system_includes.h"
#include <SDL2/SDL.h>

#include "imgui_impl_sdl.cpp"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// Platform services:
void platform_alert();
void platform_quit();
struct TimerState;
void platform_save_state(TimerState* state);

#include "solanum.h"

#define MAX_PATH 1024

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

size_t path_at_exe(char* full_path, int buffer_size, char* fname)
{
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
        return read;
    }
    return -1;
}

int cp(const char *from, const char *to)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
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
        ssize_t nwritten;

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

int main(int, char**)
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
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    SDL_Window *window = SDL_CreateWindow("Solanum",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          width, height,
                                          SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext glcontext = SDL_GL_CreateContext(window);

    // Setup ImGui binding
    ImGui_ImplSdl_Init(window);

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
            ImGui_ImplSdl_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                g_running = false;
            }
        }
        ImGui_ImplSdl_NewFrame(window);
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
    ImGui_ImplSdl_Shutdown();
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

