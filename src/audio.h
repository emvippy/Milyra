#include "heap.h"
#include "wm.h"
#include "fs.h"

#include <dsound.h>

typedef struct audio_t audio_t;

// Initialize audio system using DirectSound
audio_t* audio_init(heap_t* heap, wm_window_t* win);

// Fill the sound buffer with data
void fill_sound_buffer(audio_t* audio, DWORD byte_lock, DWORD byte_write);

// Play sound from audio buffer
void play_sound_buffer(audio_t* audio);

// Play music from music buffer after loading wav file data
void play_music(audio_t* audio);

// Load data from a wav file using fs
void load_wav_file(audio_t* audio, heap_t* heap, fs_t* fs, const char* file_name);

// Clear the sound buffer, setting all waves to 0
void clear_sound_buffer(audio_t* audio, DWORD byte_lock, DWORD byte_write);

// Destroy audio system, free from heap
void audio_destroy(audio_t* audio);