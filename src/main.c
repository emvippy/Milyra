#include "debug.h"
#include "fs.h"
#include "heap.h"
#include "render.h"
#include "frogger_game.h"
#include "timer.h"
#include "wm.h"
#include "audio.h"

#include "cpp_test.h"

int main(int argc, const char* argv[])
{
	debug_set_print_mask(k_print_info | k_print_warning | k_print_error);
	debug_install_exception_handler();

	timer_startup();

	cpp_test_function(42);

	heap_t* heap = heap_create(2 * 1024 * 1024);
	fs_t* fs = fs_create(heap, 8);
	wm_window_t* window = wm_create(heap);
	render_t* render = render_create(heap, window);

	// Init, fill, and load audio; start playing music
	audio_t* audio = audio_init(heap, window);
	fill_sound_buffer(audio, 0, 0);
	load_wav_file(audio, heap, fs, "arcade_loop.wav");
	play_music(audio);

	frogger_game_t* game = frogger_game_create(heap, fs, window, render, audio, argc, argv);

	while (!wm_pump(window))
	{
		frogger_game_update(game);
	}

	/* XXX: Shutdown render before the game. Render uses game resources. */
	render_destroy(render);
	audio_destroy(audio);

	frogger_game_destroy(game);

	wm_destroy(window);
	fs_destroy(fs);
	heap_destroy(heap);

	return 0;
}
