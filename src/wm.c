#include "wm.h"
#include "heap.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


//mouse buttons defined
enum {
	k_mouse_button_left = 1 << 0,
	k_mouse_button_right = 1 << 1,
	k_mouse_button_middle = 1 << 2
};

//key buttons defined
enum {
	k_key_up = 1 << 0,
	k_key_down = 1 << 1,
	k_key_left = 1 << 2,
	k_key_right = 1 << 3
};

typedef struct wm_window_t {
	HWND hwnd;
	heap_t* heap;
	bool quit;
	bool has_focus;
	// char padding[2];
	uint32_t mouse_mask;
	uint32_t key_mask;
	int mouse_x;
	int mouse_y;
} wm_window_t; //28 bytes

wm_window_t A_WINDOW;

//struct for virtual keys
const struct {
	int virtual_key;
	int ga_key;
} k_key_map[] = {
	{ .virtual_key = VK_LEFT, .ga_key = k_key_left },
	{ .virtual_key = VK_RIGHT, .ga_key = k_key_right },
	{ .virtual_key = VK_UP, .ga_key = k_key_up },
	{ .virtual_key = VK_DOWN, .ga_key = k_key_down }
};

static LRESULT CALLBACK _window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	wm_window_t* win = (wm_window_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (win) {
		switch (uMsg)
		{
			case WM_KEYDOWN:
				for (int i = 0; i < _countof(k_key_map); ++i) {
					if (k_key_map[i].virtual_key == wParam) {
						win->key_mask |= k_key_map[i].ga_key;
						break;
					}
				}
				break;

			case WM_KEYUP:
				for (int i = 0; i < _countof(k_key_map); ++i) {
					if (k_key_map[i].virtual_key == wParam) {
						win->key_mask &= ~k_key_map[i].ga_key;
						break;
					}
				}
				break;

			//Only for client area actions
			case WM_LBUTTONDOWN:
				//bitwise OR operation - same as win->mouse_mask = win->mouse_mask | k_mouse_button_left
				win->mouse_mask |= k_mouse_button_left;
				break;
			case WM_LBUTTONUP:
				//bitwise AND NOT operation - we want everything but the 0th bit
				//same as XOR (only one can be true) ^=
				win->mouse_mask &= ~k_mouse_button_left;
				break;
			case WM_RBUTTONDOWN:
				win->mouse_mask |= k_mouse_button_right;
				break;
			case WM_RBUTTONUP:
				win->mouse_mask &= ~k_mouse_button_right;
				break;
			case WM_MBUTTONDOWN:
				win->mouse_mask |= k_mouse_button_middle;
				break;
			case WM_MBUTTONUP:
				win->mouse_mask &= ~k_mouse_button_middle;
				break;
			
			case WM_MOUSEMOVE:
				if (win->has_focus) {
					//relative mouse movement
					//1. get current pos (old_cursor)
					//2. move mouse back to center
					//3. get current pos (new_cursor)
					//4. compute relative movement as old - new
					POINT old_cursor;
					GetCursorPos(&old_cursor);

					RECT window_rect;
					GetWindowRect(hwnd, &window_rect);
					SetCursorPos((window_rect.left + window_rect.right) / 2,
						(window_rect.bottom + window_rect.top) / 2);
					
					POINT new_cursor;
					GetCursorPos(&new_cursor);

					win->mouse_x = old_cursor.x - new_cursor.x;
					win->mouse_y = old_cursor.y - new_cursor.y;

				}
				break;

			case WM_ACTIVATEAPP: //A window is "active" when it is in focus
				ShowCursor(!wParam);
				win->has_focus = wParam;
				break;

			case WM_CLOSE:
				win->quit = true;
				break;
		}

	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

wm_window_t* wm_create(heap_t* heap)
{
	WNDCLASS wc =
	{
		.lpfnWndProc = _window_proc,
		.hInstance = GetModuleHandle(NULL),
		.lpszClassName = L"ga2022 window class",
	};
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		L"GA 2022",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		wc.hInstance,
		NULL);

	if (!hwnd) {
		return NULL;
	}

	wm_window_t* win = heap_alloc(heap, sizeof(wm_window_t), 8);
	win->has_focus = false;
	win->hwnd = hwnd;
	win->key_mask = 0;
	win->mouse_mask = 0;
	win->quit = false;
	win->heap = heap;

	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);

	ShowWindow(hwnd, TRUE);

	return win;
}

bool wm_pump(wm_window_t* window)
{
	MSG msg = { 0 };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return window->quit;
}

uint32_t wm_get_mouse_mask(wm_window_t* window) {
	return window->mouse_mask;
}

uint32_t wm_get_key_mask(wm_window_t* window) {
	return window->key_mask;
}

void wm_get_mouse_move(wm_window_t* window, int*x, int* y) {
	*x = window->mouse_x;
	*y = window->mouse_y;
}

void wm_destroy(wm_window_t* window)
{
	DestroyWindow(window->hwnd);
	heap_free(window->heap, window);
}
