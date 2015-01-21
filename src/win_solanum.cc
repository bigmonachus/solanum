#pragma warning(push, 0)
#include <windows.h>

// Platform independent includes:
#include <GL/gl.h>
#include <stdint.h>

// Local includes
#include <imgui/imgui.h>
#pragma warning(pop)

typedef int32_t bool32;

static HGLRC g_glcontext_handle;

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
            int format_index;

            PIXELFORMATDESCRIPTOR pfd = {
                sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
                1,                     // version number
                PFD_DRAW_TO_WINDOW |   // support window
                    PFD_SUPPORT_OPENGL |   // support OpenGL
                    PFD_DOUBLEBUFFER,      // double buffered
                PFD_TYPE_RGBA,         // RGBA type
                24,                    // 24-bit color depth
                0, 0, 0, 0, 0, 0,      // color bits ignored
                0,                     // no alpha buffer
                0,                     // shift bit ignored
                0,                     // no accumulation buffer
                0, 0, 0, 0,            // accum bits ignored
                32,                    // 32-bit z-buffer
                0,                     // no stencil buffer
                0,                     // no auxiliary buffer
                PFD_MAIN_PLANE,        // main layer
                0,                     // reserved
                0, 0, 0                // layer masks ignored
            };

            // get the best available match of pixel format for the device context
            format_index = ChoosePixelFormat(dc, &pfd);

            // make that the pixel format of the device context
            SetPixelFormat(dc, format_index, &pfd);

            g_glcontext_handle = wglCreateContext(dc);
            if (!g_glcontext_handle)
            {
                OutputDebugStringA("Could not create GL context. Exiting");
                PostQuitMessage(0);
            }
            bool32 succeeded = wglMakeCurrent(dc, g_glcontext_handle);
            if (!succeeded)
            {
                OutputDebugStringA("Could not set current GL context. Exiting");
                PostQuitMessage(0);
            }
            result = DefWindowProc(window, message, wParam, lParam);
            break;
        }
    case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
    case WM_PAINT:
        {
            /* PAINTSTRUCT ps; */
            /* BeginPaint(window, &ps); */
            // paint here
            {
                glClearColor(0,1,0,0.5);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                SwapBuffers(GetDC(window));
            }
            /* EndPaint(window, &ps); */
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
            WS_EX_TOPMOST ,  // dwExStyle
            window_class.lpszClassName,     // class Name
            "Solanum",                      // window name
            WS_VISIBLE | WS_POPUP,          // dwStyle
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
