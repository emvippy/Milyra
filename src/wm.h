#include <stdbool.h>
#include <stdint.h>
/*

====    CODING STANDARD     ====
1. Functions and variables should be named using snake_case (all words in lowercase, separated by underscores).
2. Avoid win32 specific types in headers. Avoid "windows.h" in engine headers.
3. Document all functions and types defined in engine headers.
4. Avoid global and module-level variables.

*/

typedef wm_window_t;

//Create new window; destroyed with wm_destroy()
//Failure: return null. Success: new window
wm_window_t* wm_create();

//Pump window messages; refresh mouse and key states
bool wm_pump(wm_window_t* window);

//Destroy window
void wm_destroy(wm_window_t* window);

//Get mouse buttons mask
uint32_t wm_get_mouse_mask(wm_window_t* window);

//Get keys mask
uint32_t wm_get_key_mask(wm_window_t* window);

//Get mouse movement (x, y)
void wm_get_mouse_move(wm_window_t* window, int* x, int* y);