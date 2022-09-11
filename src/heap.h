#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memoryapi.h>
#include <stddef.h>
// Main object, heap_t, represents dynamic memory heap
// Memory can be allocated and freed from the heap

typedef struct heap_t heap_t;

// Create new memory heap, with default size grow_increment
// by which the heap grows. Should be multiple of OS page size.
heap_t* heap_create(size_t grow_increment);

// Allocate memory from a heap.
void* heap_alloc(heap_t* heap, size_t size, size_t alignment);

// Free memory previously allocated.
void heap_free(heap_t* heap, void* address);

// Destroy previously created heap.
void heap_destroy(heap_t* heap);