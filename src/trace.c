#include "trace.h"

#include "heap.h"
#include "queue.h"
#include "timer.h"
#include "timer_object.h"
#include "fs.h"
#include "mutex.h"
#include "debug.h"

#include <stddef.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct trace_t
{
	heap_t* heap;
	queue_t* event_queue;
	mutex_t* mutex;
	char* event_buff;
	const char* file_path;
	size_t event_capacity;
	int event_buff_count;
	bool capturing;
} trace_t;

typedef struct trace_event_t
{
	const char* name;
	char ph;
	int pid;
	int tid;
	uint64_t ts;
} trace_event_t;

trace_t* trace_create(heap_t* heap, int event_capacity)
{
	trace_t* t = heap_alloc(heap, sizeof(t), 8);
	t->heap = heap;
	t->event_queue = queue_create(heap, event_capacity);
	t->mutex = mutex_create();
	t->event_buff = heap_alloc(t->heap, sizeof(trace_event_t)*event_capacity*2, 8);
	t->event_capacity = (size_t)event_capacity;
	t->event_buff_count = 0;
	t->capturing = false;
	return t;
}

void trace_destroy(trace_t* trace)
{
	heap_free(trace->heap, trace->event_buff);
	queue_push(trace->event_queue, NULL);
	mutex_destroy(trace->mutex);
	queue_destroy(trace->event_queue);
	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name)
{
	mutex_lock(trace->mutex);
	if (trace->capturing)
	{
		trace_event_t* ev = (trace_event_t*)(trace->event_buff + (trace->event_buff_count * sizeof(trace_event_t)));
		ev->name = name;
		ev->ph = 'B';
		ev->pid = GetCurrentProcessId();
		ev->tid = GetCurrentThreadId();
		ev->ts = timer_get_ticks();
		trace->event_buff_count++;
		queue_push(trace->event_queue, ev);
	}
	mutex_unlock(trace->mutex);
}

void trace_duration_pop(trace_t* trace)
{
	mutex_lock(trace->mutex);
	if (trace->capturing)
	{
		trace_event_t* popped_ev = dequeue(trace->event_queue);
		trace_event_t* ev = (trace_event_t*)(trace->event_buff + (trace->event_buff_count * sizeof(trace_event_t)));
		ev->name = popped_ev->name;
		ev->ph = 'E';
		ev->pid = popped_ev->pid;
		ev->tid = popped_ev->tid;
		ev->ts = timer_get_ticks();
		trace->event_buff_count++;
	}
	mutex_unlock(trace->mutex);
}

void trace_capture_start(trace_t* trace, const char* path)
{
	if (!trace->capturing) {
		trace->capturing = true;
		trace->file_path = path;
	}
}

void trace_capture_stop(trace_t* trace)
{
	if (trace->capturing) {
		fs_t* f = fs_create(trace->heap, 1);
		trace->capturing = false;
		int str_size = 0;
		str_size += snprintf(NULL, 0, "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\" : [\n");
		int first_line = str_size;
		//debug_print(k_print_info, "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\" : [\n");
		for (int i = 0; i < trace->event_buff_count; i++) {
			trace_event_t* ev = (trace_event_t*)(trace->event_buff + (i * sizeof(trace_event_t)));
			if (i == trace->event_buff_count - 1) {
				str_size += snprintf(NULL, 0, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"}\n",
					ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
				//debug_print(k_print_info, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"}\n",
				//	ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
			}
			else {
				str_size += snprintf(NULL, 0, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"},\n",
									ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
				//debug_print(k_print_info, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"},\n",
				//	ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
			}
		}
		int last_line = snprintf(NULL, 0, "\t]\n}\0");
		str_size += last_line;
		//debug_print(k_print_info, "\t]\n}\0");
		//debug_print(k_print_info, "str_size = %d", str_size);

		char* buffer = heap_alloc(trace->heap, str_size, 8);
		int current_pos = first_line;
		snprintf(buffer, (first_line + 1), "{\n\t\"displayTimeUnit\": \"ns\", \"traceEvents\" : [\n");
		for (int j = 0; j < trace->event_buff_count; j++) {
			trace_event_t* ev = (trace_event_t*)(trace->event_buff + (j * sizeof(trace_event_t)));
			int line_size = 0;

			if (j == trace->event_buff_count - 1) {
				line_size = snprintf(NULL, 0, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"}\n\0",
					ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
				snprintf((buffer + current_pos), (line_size + 1), 
					"\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"}\n\0",
					ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
			}
			else {
				line_size = snprintf(NULL, 0, "\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"},\n\0",
					ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
				snprintf((buffer + current_pos), (line_size + 1), 
					"\t\t{\"name\":\"%s\",\"ph\":\"%c\",\"pid\":%d,\"tid\":\"%d\",\"ts\":\"%d\"},\n\0",
					ev->name, ev->ph, ev->pid, ev->tid, (int)ev->ts);
			}
			current_pos += line_size;
		}
		snprintf((buffer + current_pos), (last_line + 1), "\t]\n}\0");

		fs_work_t* w = fs_write(f, trace->file_path, buffer, str_size, false);
		fs_work_wait(w);
		fs_work_destroy(w);
		fs_destroy(f);
		heap_free(trace->heap, buffer);
	}
}
