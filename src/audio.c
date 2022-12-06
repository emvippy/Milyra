#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")

#include "audio.h"
#include "wav_parse.h"

#include <dsound.h>
#include "debug.h"

typedef HRESULT direct_sound_create(LPGUID lpGuid,
									LPDIRECTSOUND* ppDS, 
									LPUNKNOWN pUnkOuter);

/* ~~~~~~~~~~~~~~~~~~~~ AUDIO STRUCTS ~~~~~~~~~~~~~~~~~~~~ */

typedef struct game_sound_buffer_t {
	int size; //the size of the buffer
	int channel_count;
	int samples_per_second;
	int bytes_per_sample;
	int running_sample_index;

	int16_t* samples;
	int samples_to_write;
} game_sound_buffer_t;

typedef struct audio_t {
	heap_t* heap;
	wm_window_t* window;
	fs_work_t* work;
	char* wav_data;
	WAVFile_t wav_file;

	WAVEFORMATEX wav_format;

	LPDIRECTSOUND direct_sound;
	LPDIRECTSOUNDBUFFER primary_buffer;
	LPDIRECTSOUNDBUFFER fx_sound_buffer;
	LPDIRECTSOUNDBUFFER music_sound_buffer;
	game_sound_buffer_t sound_buffer;
} audio_t;

/* ~~~~~~~~~~~~~~~~~~~~ FUNCTIONS ~~~~~~~~~~~~~~~~~~~~ */


audio_t* audio_init(heap_t* heap, wm_window_t* window) {
	//Load the directsound library dll
	HMODULE dsound_dll = LoadLibraryA("dsound.dll");
	if (!dsound_dll) { 
		debug_print(k_print_error, "!!!!!!!!!!! dsound_dll not loaded\n");
		return NULL;
	}

	direct_sound_create* ds_create = (direct_sound_create*) GetProcAddress(dsound_dll, "DirectSoundCreate");
	if (!ds_create) {
		debug_print(k_print_error, "!!!!!!!!!!! ds_create not created\n");
		return NULL;
	}
	
	//Create the audio struct
	audio_t* audio = heap_alloc(heap, sizeof(audio_t), 8);
	audio->heap = heap;
	audio->window = window;
	LPDIRECTSOUND dsound = 0;
	audio->direct_sound = dsound;

	// Create and set cooperative level for the new directsound object
	if (SUCCEEDED(ds_create(0, &audio->direct_sound, 0))) {
		if (SUCCEEDED(audio->direct_sound->lpVtbl->SetCooperativeLevel(
													audio->direct_sound, 
													(HWND) wm_get_raw_window(window), 
													DSSCL_PRIORITY))) {
			//Create a sound buffer
			game_sound_buffer_t sbuff;
			sbuff.channel_count = 2;
			sbuff.samples_per_second = 44100;
			sbuff.bytes_per_sample = sbuff.channel_count * sizeof(int16_t);
			sbuff.size = sbuff.samples_per_second * sbuff.bytes_per_sample;
			audio->sound_buffer = sbuff;

			//Create the primary buffer, required for directsound use
			DSBUFFERDESC primary_buffer_desc = { 0 };
			primary_buffer_desc.dwSize = sizeof(primary_buffer_desc);
			primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
			LPDIRECTSOUNDBUFFER primary_buffer;
			audio->direct_sound->lpVtbl->CreateSoundBuffer(
				audio->direct_sound,
				&primary_buffer_desc,
				&primary_buffer,
				0);
			audio->primary_buffer = primary_buffer;

			//Define wave format for audio
			WAVEFORMATEX wave_format = { 0 };
			wave_format.wFormatTag = WAVE_FORMAT_PCM;
			wave_format.nChannels = sbuff.channel_count;
			wave_format.nSamplesPerSec = sbuff.samples_per_second;
			wave_format.wBitsPerSample = 16;
			wave_format.nBlockAlign = wave_format.nChannels * wave_format.wBitsPerSample / 8;
			wave_format.nAvgBytesPerSec = (wave_format.nBlockAlign * wave_format.nSamplesPerSec);
			wave_format.cbSize = 0;
			audio->wav_format = wave_format;

			//Create the secondary buffer, filled by fill_sound_buffer
			DSBUFFERDESC secondary_buffer_desc = { 0 };
			secondary_buffer_desc.dwSize = sizeof(secondary_buffer_desc);
			secondary_buffer_desc.dwFlags = DSBCAPS_GLOBALFOCUS;
			secondary_buffer_desc.dwBufferBytes = sbuff.size;
			secondary_buffer_desc.lpwfxFormat = &wave_format;
			LPDIRECTSOUNDBUFFER win32sbuff;
			audio->direct_sound->lpVtbl->CreateSoundBuffer(
				audio->direct_sound,
				&secondary_buffer_desc,
				&win32sbuff,
				0);
			audio->fx_sound_buffer = win32sbuff;
		}
	}
	return audio;
}

void fill_sound_buffer(audio_t* audio, DWORD byte_lock, DWORD byte_write) {
	// Lock the buffer
	void* region_1;
	void* region_2;
	DWORD region_1_size, region_2_size;
	audio->fx_sound_buffer->lpVtbl->Lock(audio->fx_sound_buffer,
											0,
											audio->sound_buffer.size,
											&region_1,
											&region_1_size,
											&region_2,
											&region_2_size, 0);

	// Fill the regions with data equal to a sound with frequency 200
	bool temp = true;
	int square_freq = 0;
	int16_t* at = region_1;
	DWORD region_1_sample_count = region_1_size / audio->sound_buffer.bytes_per_sample;
	for (DWORD i = 0; i < region_1_sample_count; i++) {
		int16_t sample;
		if (temp) {
			sample = 500;
		}
		else {
			sample = -500;
		}
		*at++ = sample;
		*at++ = sample;

		square_freq++;
		if (square_freq > 200) {
			square_freq = 0;
			temp = !temp;
		}
	}
	at = region_2;
	DWORD region_2_sample_count = region_2_size / audio->sound_buffer.bytes_per_sample;
	for (DWORD i = 0; i < region_2_sample_count; i++) {
		int16_t sample;
		if (temp) {
			sample = 500;
		}
		else {
			sample = -500;
		}
		*at++ = sample;
		*at++ = sample;

		square_freq++;
		if (square_freq > 200) {
			square_freq = 0;
			temp = !temp;
		}
	}
	// Unlock the buffer when finished
	audio->fx_sound_buffer->lpVtbl->Unlock(audio->fx_sound_buffer,
		region_1,
		region_1_size,
		region_2,
		region_2_size);
}

void clear_sound_buffer(audio_t* audio, DWORD byte_lock, DWORD byte_write) {
	// Lock the buffer
	void* region_1;
	void* region_2;
	DWORD region_1_size, region_2_size;
	audio->fx_sound_buffer->lpVtbl->Lock(audio->fx_sound_buffer,
											0,
											audio->sound_buffer.size,
											&region_1,
											&region_1_size,
											&region_2,
											&region_2_size, 0);

	// Clear regions by zeroing out the values
	DWORD region_1_sample_count = region_1_size / audio->sound_buffer.bytes_per_sample;
	int16_t* at = region_1;
	for (DWORD i = 0; i < region_1_sample_count; i++) {

		*at++ = 0;
		*at++ = 0;

	}
	DWORD region_2_sample_count = region_2_size / audio->sound_buffer.bytes_per_sample;
	at = region_2;
	for (DWORD i = 0; i < region_2_sample_count; i++) {

		*at++ = 0;
		*at++ = 0;

	}
	// Unlock the buffer when finished
	audio->fx_sound_buffer->lpVtbl->Unlock(audio->fx_sound_buffer, 
											region_1, 
											region_1_size, 
											region_2, 
											region_2_size);
}

void load_wav_file(audio_t* audio, heap_t* heap, fs_t* fs, const char* file_name) {
	// Read the file using fs_read, then get the work buffer and send that to a wav parser
	audio->work = fs_read(fs, file_name, heap, false, false);
	audio->wav_data = fs_work_get_buffer(audio->work);
	audio->wav_file = WAV_ParseFileData(audio->wav_data);

	// Fill in the buffer for the music
	// Set the buffer description of the music sound buffer for wav file
	DSBUFFERDESC music_desc = { 0 };
	music_desc.dwSize = sizeof(DSBUFFERDESC);
	music_desc.dwFlags = DSBCAPS_CTRLVOLUME;
	music_desc.dwBufferBytes = audio->wav_file.header.data_size;
	music_desc.dwReserved = 0;
	music_desc.lpwfxFormat = &(audio->wav_format);
	music_desc.guid3DAlgorithm = GUID_NULL;

	// Create the music sound buffer with the specific buffer settings.
	audio->direct_sound->lpVtbl->CreateSoundBuffer(
		audio->direct_sound, 
		&music_desc, 
		&audio->music_sound_buffer, 
		NULL);

	// Lock the secondary buffer to write wave data into it.
	char* char_buffer;
	DWORD char_bsize;
	audio->music_sound_buffer->lpVtbl->Lock(audio->music_sound_buffer, 
											0, 
											audio->wav_file.header.data_size, 
											(void**)&char_buffer, 
											(DWORD*)&char_bsize, 
											NULL, 
											0, 
											0);

	// Copy the wave data into the buffer.
	memcpy(char_buffer, audio->wav_file.data, audio->wav_file.header.data_size);

	// Unlock the secondary buffer after the data has been written to it.
	audio->music_sound_buffer->lpVtbl->Unlock(audio->music_sound_buffer, (void*)char_buffer, char_bsize, NULL, 0);

}

void play_sound_buffer(audio_t* audio) {
	// Use the buffer function to play the sound stored in the buffer
	audio->fx_sound_buffer->lpVtbl->Play(audio->fx_sound_buffer, 0, 0, 0); //DSBPLAY_LOOPING for loop
}

void play_music(audio_t* audio) {
	audio->music_sound_buffer->lpVtbl->Play(audio->music_sound_buffer, 0, 0, DSBPLAY_LOOPING);
}

void audio_destroy(audio_t* audio) {
	//Clear the buffer(s), destroy the work, and free the memory used
	clear_sound_buffer(audio, 0, 0);
	audio->fx_sound_buffer->lpVtbl->Release(audio->fx_sound_buffer);
	audio->music_sound_buffer->lpVtbl->Release(audio->music_sound_buffer);
	audio->primary_buffer->lpVtbl->Release(audio->primary_buffer);
	fs_work_destroy(audio->work);
	heap_free(audio->heap, audio->wav_data);
	heap_free(audio->heap, audio);
}