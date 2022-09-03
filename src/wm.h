#include <stdbool.h>
#include <stdint.h>

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