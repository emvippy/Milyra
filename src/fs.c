#include "fs.h"
#include "lz4/lz4.h"
#include "debug.h"

#include "event.h"
#include "heap.h"
#include "queue.h"
#include "thread.h"

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct fs_t
{
	heap_t* heap;
	// Queue and thread used for file operations
	queue_t* file_queue;
	thread_t* file_thread;
	// Queue and thread used for compression
	queue_t* comp_queue;
	thread_t* comp_thread;
} fs_t;

typedef enum fs_work_op_t
{
	k_fs_work_op_read,
	k_fs_work_op_write,
} fs_work_op_t;

typedef struct fs_work_t
{
	heap_t* heap;
	fs_work_op_t op;
	char path[1024];
	bool null_terminate;
	bool use_compression;
	void* buffer;
	size_t size;
	event_t* done;
	int result;
} fs_work_t;

static int file_thread_func(void* user);
static int comp_thread_func(void* user);

fs_t* fs_create(heap_t* heap, int queue_capacity)
{
	fs_t* fs = heap_alloc(heap, sizeof(fs_t), 8);
	fs->heap = heap;
	fs->file_queue = queue_create(heap, queue_capacity);
	fs->file_thread = thread_create(file_thread_func, fs);
	fs->comp_queue = queue_create(heap, queue_capacity);
	fs->comp_thread = thread_create(comp_thread_func, fs);
	return fs;
}

void fs_destroy(fs_t* fs)
{
	queue_push(fs->file_queue, NULL);
	queue_push(fs->comp_queue, NULL);
	thread_destroy(fs->file_thread);
	queue_destroy(fs->file_queue);
	thread_destroy(fs->comp_thread);
	queue_destroy(fs->comp_queue);
	heap_free(fs->heap, fs);
}

fs_work_t* fs_read(fs_t* fs, const char* path, heap_t* heap, bool null_terminate, bool use_compression)
{
	fs_work_t* work = heap_alloc(fs->heap, sizeof(fs_work_t), 8);
	work->heap = heap;
	work->op = k_fs_work_op_read;
	strcpy_s(work->path, sizeof(work->path), path);
	work->buffer = NULL;
	work->size = 0;
	work->done = event_create();
	work->result = 0;
	work->null_terminate = null_terminate;
	work->use_compression = use_compression;
	queue_push(fs->file_queue, work);
	return work;
}

fs_work_t* fs_write(fs_t* fs, const char* path, const void* buffer, size_t size, bool use_compression)
{
	fs_work_t* work = heap_alloc(fs->heap, sizeof(fs_work_t), 8);
	work->heap = fs->heap;
	work->op = k_fs_work_op_write;
	strcpy_s(work->path, sizeof(work->path), path);
	work->buffer = (void*)buffer;
	work->size = size;
	work->done = event_create();
	work->result = 0;
	work->null_terminate = false;
	work->use_compression = use_compression;

	if (use_compression)
	{
		// HOMEWORK 2: Queue file write work on compression queue!

		queue_push(fs->comp_queue, work);
	}
	else
	{
		queue_push(fs->file_queue, work);
	}

	return work;
}

bool fs_work_is_done(fs_work_t* work)
{
	return work ? event_is_raised(work->done) : true;
}

void fs_work_wait(fs_work_t* work)
{
	if (work)
	{
		event_wait(work->done);
	}
}

int fs_work_get_result(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->result : -1;
}

void* fs_work_get_buffer(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->buffer : NULL;
}

size_t fs_work_get_size(fs_work_t* work)
{
	fs_work_wait(work);
	return work ? work->size : 0;
}

void fs_work_destroy(fs_work_t* work)
{
	if (work)
	{
		event_wait(work->done);
		event_destroy(work->done);
		if (work->use_compression && (work->op == k_fs_work_op_write)) {
			heap_free(work->heap, work->buffer);
		}
		heap_free(work->heap, work);
	}
}

static void file_read(fs_t* fs, fs_work_t* work)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, work->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		work->result = -1;
		return;
	}

	HANDLE handle = CreateFile(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		work->result = GetLastError();
		return;
	}

	if (!GetFileSizeEx(handle, (PLARGE_INTEGER)&work->size))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	work->buffer = heap_alloc(work->heap, work->null_terminate ? work->size + 1 : work->size, 8);

	DWORD bytes_read = 0;
	if (!ReadFile(handle, work->buffer, (DWORD)work->size, &bytes_read, NULL))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	work->size = bytes_read;
	if (work->null_terminate)
	{
		((char*)work->buffer)[bytes_read] = 0;
	}

	CloseHandle(handle);

	if (work->use_compression)
	{
		// HOMEWORK 2: Queue file read work on compression queue!
		queue_push(fs->comp_queue, work);
	}
	else
	{
		event_signal(work->done);
	}
}

static void file_write(fs_work_t* work)
{
	wchar_t wide_path[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, work->path, -1, wide_path, sizeof(wide_path)) <= 0)
	{
		work->result = -1;
		return;
	}

	HANDLE handle = CreateFile(wide_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		work->result = GetLastError();
		return;
	}

	DWORD bytes_written = 0;
	if (!WriteFile(handle, work->buffer, (DWORD)work->size, &bytes_written, NULL))
	{
		work->result = GetLastError();
		CloseHandle(handle);
		return;
	}

	work->size = bytes_written;

	CloseHandle(handle);

	event_signal(work->done);
}

static int file_thread_func(void* user)
{
	fs_t* fs = user;
	while (true)
	{
		fs_work_t* work = queue_pop(fs->file_queue);
		if (work == NULL)
		{
			break;
		}

		switch (work->op)
		{
		case k_fs_work_op_read:
			file_read(fs, work);
			break;
		case k_fs_work_op_write:
			file_write(work);
			break;
		}
	}
	return 0;
}

static int comp_thread_func(void* user)
{
	fs_t* fs = user;
	while (true)
	{
		fs_work_t* work = queue_pop(fs->comp_queue);
		if (work == NULL)
		{
			break;
		}

		int dst_buff_size;
		char* dst_buff;

		switch (work->op)
		{
		case k_fs_work_op_read:
			//decompress data
			//Copy the size data stored at the front of buffer into the new size
			memcpy(&dst_buff_size, (int*)work->buffer, 1);
			//Allocate a new buffer and decompress into it
			dst_buff = heap_alloc(fs->heap, dst_buff_size, 8);
			int decomp_size = LZ4_decompress_safe((char*)(work->buffer) + 4, dst_buff, (int)work->size - 4, dst_buff_size);
			//Restore original size, free previous buffer, and write decompressed text to buffer
			work->size = decomp_size;
			heap_free(fs->heap, work->buffer);
			work->buffer = dst_buff;
			//If null terminate, add null terminate to the end of the text
			if (work->null_terminate)
			{
				((char*)work->buffer)[work->size] = '\0';
			}
			//Signal the work is done for decompression reading
			event_signal(work->done);
			break;
		case k_fs_work_op_write:
			//compress data
			//Get a size for the compressed text and allocate buffer to store the compressed text
			dst_buff_size = LZ4_compressBound((int)work->size);
			dst_buff = heap_alloc(fs->heap, (size_t)(dst_buff_size) + 4, 8);
			//At the front of the buffer, store the original file size to be used in read
			((size_t*)dst_buff)[0] = (char)work->size;
			//Compress the work into the new buffer, store it and the compressed size into work
			int comp_size = LZ4_compress_default(work->buffer, (char*)(dst_buff)+4, (int)work->size, dst_buff_size) + 4;
			work->size = comp_size;
			work->buffer = dst_buff;
			//Add work to the file queue
			queue_push(fs->file_queue, work);
			break;
		}
	}
	return 0;
}
