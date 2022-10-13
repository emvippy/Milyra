#pragma once

#include <stdlib.h>
/*
====    CODING STANDARD     ====
1. Functions and variables should be named using snake_case (all words in lowercase, separated by underscores).
2. Avoid win32 specific types in headers. Avoid "windows.h" in engine headers.
3. Document all functions and types defined in engine headers.
4. Avoid global and module-level variables.
*/

// Heap Memory Manager
// 
// Main object, heap_t, represents a dynamic memory heap.
// Once created, memory can be allocated and free from the heap.

// Handle to a heap.
typedef struct heap_t heap_t;

// Function to print out the current call stack.
// Given the number of frames and the pointer to the stack information.
void bt_print(int frames, void** stack);

// Creates a new memory heap.
// The grow increment is the default size with which the heap grows.
// Should be a multiple of OS page size.
heap_t* heap_create(size_t grow_increment);

// Destroy a previously created heap.
void heap_destroy(heap_t* heap);

// Allocate memory from a heap.
void* heap_alloc(heap_t* heap, size_t size, size_t alignment);

// Free memory previously allocated from a heap.
void heap_free(heap_t* heap, void* address);
