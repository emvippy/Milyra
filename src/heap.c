#include "heap.h"

#include "debug.h"
#include "tlsf/tlsf.h"

#include <stddef.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

typedef struct arena_t
{
	pool_t pool;
	struct arena_t* next;
} arena_t;

typedef struct heap_t
{
	tlsf_t tlsf;
	size_t grow_increment;
	arena_t* arena;
} heap_t;

//Struct to store the number of frames and the pointer to a callstack
typedef struct callstack_t
{
	int frames;
	void* stack[10];
} callstack_t;

void bt_print(int frames, void** stack)
{
	// Get the process and initialize the system
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	// Set up the symbol needed for SymFromAddr
	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	// For each frame, convert the address to printable symbols.
	for (int i = 0; i < frames; i++)
	{
		SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);

		// Print the deciphered symbols
		debug_print(k_print_error, "[%i] %s\n", i, symbol->Name);

		// If the current function name is main, stop printing
		if (strcmp(symbol->Name, "main") == 0) {
			break;
		}
	}

	// Clean up used memory
	free(symbol);
	SymCleanup(process);
}

heap_t* heap_create(size_t grow_increment)
{
	heap_t* heap = VirtualAlloc(NULL, sizeof(heap_t) + tlsf_size(),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!heap)
	{
		debug_print(
			k_print_error,
			"OUT OF MEMORY!\n");
		return NULL;
	}

	heap->grow_increment = grow_increment;
	heap->tlsf = tlsf_create(heap + 1);
	heap->arena = NULL;

	return heap;
}

void* heap_alloc(heap_t* heap, size_t size, size_t alignment)
{
	// Add more allocated memory for the callstack
	void* address = tlsf_memalign(heap->tlsf, alignment, size + sizeof(callstack_t)); 
	if (!address)
	{
		size_t arena_size =
			__max(heap->grow_increment, size * 2) +
			sizeof(arena_t);
		arena_t* arena = VirtualAlloc(NULL,
			arena_size + tlsf_pool_overhead(),
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!arena)
		{
			debug_print(
				k_print_error,
				"OUT OF MEMORY!\n");
			return NULL;
		}

		arena->pool = tlsf_add_pool(heap->tlsf, arena + 1, arena_size);

		arena->next = heap->arena;
		heap->arena = arena;

		address = tlsf_memalign(heap->tlsf, alignment, size + sizeof(callstack_t));
	}

	// If address was allocated, create a callstack for the allocation and store it
	// behind the actual chunk of memory
	if (address) {
		callstack_t* callstack = (callstack_t*)((char*)address + size);
		callstack->frames = debug_backtrace(callstack->stack, 10);
	}
	return address;
}

void heap_free(heap_t* heap, void* address)
{
	tlsf_free(heap->tlsf, address);
}

void check_pool(void* ptr, size_t size, int used, void* user)
{
	if (used) {
		//LEAK
		debug_print(k_print_error, "Memory leak of size %d bytes with callstack:\n", (int)size - (int)sizeof(callstack_t));
		//get callstack_t of ptr with pointer arithmetic (current place + size + callstack size)
		callstack_t* cs = (callstack_t*)((char*)ptr + size - sizeof(callstack_t));
		//use bt_print to output info
		bt_print(cs->frames, cs->stack);
	}
}

void heap_destroy(heap_t* heap)
{
	tlsf_destroy(heap->tlsf);

	arena_t* arena = heap->arena;
	while (arena)
	{
		//Check then free
		tlsf_walk_pool(arena->pool, check_pool, NULL);

		arena_t* next = arena->next;
		VirtualFree(arena, 0, MEM_RELEASE);
		arena = next;
	}

	VirtualFree(heap, 0, MEM_RELEASE);
}
