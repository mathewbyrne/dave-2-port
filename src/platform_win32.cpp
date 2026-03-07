#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app.cpp"

typedef struct {
    BITMAPINFO bitmap_info;
    void      *memory;
    int        width;
    int        height;
    int        pitch;
} win32_backbuffer_t;

static win32_backbuffer_t g_backbuffer;
static int                g_running = 1;
static game_state_t       game_state;
static game_input_t       game_input;
static HCURSOR            g_arrow_cursor;

static void win32_set_button(game_button_t *button, uint8_t is_down) {
    if (!button) {
        return;
    }

    if (is_down && !button->is_down) {
        button->pressed_this_frame = 1;
    }
    button->is_down = is_down;
}

static void win32_set_movement_key(WPARAM key, uint8_t is_down) {
    if (key == VK_LEFT) {
        game_input.direction_keys.move_left = is_down;
    } else if (key == VK_RIGHT) {
        game_input.direction_keys.move_right = is_down;
    } else if (key == VK_UP) {
        game_input.direction_keys.move_up = is_down;
    } else if (key == VK_DOWN) {
        game_input.direction_keys.move_down = is_down;
    } else {
        return;
    }
}

static void win32_set_action_key(WPARAM key, uint8_t is_down) {
    if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL) {
        win32_set_button(&game_input.action_1, is_down);
    } else if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU) {
        win32_set_button(&game_input.action_2, is_down);
    }
}

static void win32_set_menu_trigger(WPARAM key) {
    if (key == VK_F1) {
        game_input.menu_help = 1;
    } else if (key == VK_F2) {
        game_input.menu_sound_toggle = 1;
    } else if (key == VK_F3) {
        game_input.menu_keyboard_config = 1;
    } else if (key == VK_F5) {
        game_input.menu_reset_game = 1;
    } else if (key == VK_ESCAPE) {
        game_input.menu_quit_game = 1;
        g_running                 = 0;
    } else if (key == VK_TAB) {
        game_input.menu_status = 1;
    }
}

static void win32_resize_backbuffer(win32_backbuffer_t *buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width  = width;
    buffer->height = height;
    buffer->pitch  = width * 4;

    buffer->bitmap_info.bmiHeader.biSize        = sizeof(buffer->bitmap_info.bmiHeader);
    buffer->bitmap_info.bmiHeader.biWidth       = width;
    buffer->bitmap_info.bmiHeader.biHeight      = -height;
    buffer->bitmap_info.bmiHeader.biPlanes      = 1;
    buffer->bitmap_info.bmiHeader.biBitCount    = 32;
    buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;

    size_t bitmap_memory_size = (size_t)buffer->pitch * (size_t)buffer->height;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static void win32_present_backbuffer(win32_backbuffer_t *buffer, HDC device_context,
                                     int window_width, int window_height) {
    StretchDIBits(device_context, 0, 0, window_width, window_height, 0, 0, buffer->width,
                  buffer->height, buffer->memory, &buffer->bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK win32_main_window_proc(HWND window, UINT message, WPARAM w_param,
                                               LPARAM l_param) {
    switch (message) {
    case WM_CLOSE:
    case WM_DESTROY:
        g_running = 0;
        return 0;

    case WM_SIZE: {
        RECT client_rect;
        GetClientRect(window, &client_rect);
        int width  = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;
        if (width > 0 && height > 0) {
            win32_resize_backbuffer(&g_backbuffer, width, height);
        }
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(l_param) == HTCLIENT) {
            SetCursor(g_arrow_cursor);
            return TRUE;
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        if (w_param == VK_F4 && (l_param & (1 << 29))) {
            g_running = 0;
            return 0;
        }

        win32_set_movement_key(w_param, 1);
        win32_set_action_key(w_param, 1);
        if ((l_param & (1 << 30)) == 0) {
            win32_set_menu_trigger(w_param);
        }
        return 0;
    }
    case WM_SYSKEYUP:
    case WM_KEYUP:
        win32_set_movement_key(w_param, 0);
        win32_set_action_key(w_param, 0);
        return 0;

    default:
        return DefWindowProc(window, message, w_param, l_param);
    }

    return 0;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_code) {
    (void)prev_instance;
    (void)cmd_line;

    WNDCLASSA window_class     = {0};
    window_class.style         = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = win32_main_window_proc;
    window_class.hInstance     = instance;
    g_arrow_cursor             = LoadCursorA(0, IDC_ARROW);
    window_class.hCursor       = g_arrow_cursor;
    window_class.lpszClassName = "Dave2WindowClass";

    if (!RegisterClassA(&window_class)) {
        return 1;
    }

    HWND window = CreateWindowExA(0, window_class.lpszClassName, "Dangerous Dave 2 (Port)",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                  960, 600, 0, 0, instance, 0);
    if (!window) {
        return 1;
    }

    ShowWindow(window, show_code);

    LARGE_INTEGER frequency;
    LARGE_INTEGER last_counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last_counter);
    const double target_frame_seconds = 1.0 / 70.0;

    game_init(&game_state);

    while (g_running) {
        MSG message;
        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                g_running = 0;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        LARGE_INTEGER current_counter;
        QueryPerformanceCounter(&current_counter);
        float dt =
            (float)(current_counter.QuadPart - last_counter.QuadPart) / (float)frequency.QuadPart;
        last_counter = current_counter;

        if (!g_backbuffer.memory) {
            win32_resize_backbuffer(&g_backbuffer, 320, 200);
        }

        game_tick(&game_state, &game_input, dt, (uint32_t *)g_backbuffer.memory, g_backbuffer.width,
                  g_backbuffer.height);
        if (game_input.menu_reset_game) {
            game_init(&game_state);
        }
        game_input.action_1.pressed_this_frame = 0;
        game_input.action_2.pressed_this_frame = 0;
        game_input.menu_help                   = 0;
        game_input.menu_sound_toggle           = 0;
        game_input.menu_keyboard_config        = 0;
        game_input.menu_reset_game             = 0;
        game_input.menu_quit_game              = 0;
        game_input.menu_status                 = 0;

        RECT client_rect;
        GetClientRect(window, &client_rect);
        int window_width  = client_rect.right - client_rect.left;
        int window_height = client_rect.bottom - client_rect.top;

        HDC device_context = GetDC(window);
        win32_present_backbuffer(&g_backbuffer, device_context, window_width, window_height);
        ReleaseDC(window, device_context);

        LARGE_INTEGER frame_end;
        QueryPerformanceCounter(&frame_end);
        double frame_time =
            (double)(frame_end.QuadPart - current_counter.QuadPart) / (double)frequency.QuadPart;
        double sleep_time = target_frame_seconds - frame_time;
        if (sleep_time > 0.0) {
            DWORD sleep_ms = (DWORD)(sleep_time * 1000.0);
            if (sleep_ms > 0) {
                Sleep(sleep_ms);
            }
        }
    }

    return 0;
}
