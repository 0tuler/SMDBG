/*
 *
 * Simple Memory Debugger
 * works by substituting the malloc and free call by a custom call
 * works with multithreading
 *
 * SHOULD NOT BE USED IN PRODUCTION, SHOULD ONLY USE FOR DEBUGING PURPOSES
 *
 */

#pragma once
#ifdef _OPENMP
#include <omp.h>
#else
#include <pthread.h>
#include <stdatomic.h>
#endif // _OPENMP
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_DBG_STRINGFY(x) #x

#define MEM_DBG_TO_STRING(x) MEM_DBG_STRINGFY(x)

// Gets the file and the line number as a string literal
#define MEM_DBG_GET_LOG()                                                      \
  "file " __FILE__ " at line " MEM_DBG_TO_STRING(__LINE__)

// Allocation structure
typedef struct AllocatedData {
  void *ptr;
  uint64_t size;
  char *position;
} AllocatedData;

// FreeList structure, reduces the need to increase the AllocatedData buffer
// inside AllocationTable
typedef struct FreeList {
  uint64_t *ptr;
  uint64_t cap;
  uint64_t size;
} FreeList;

// AllocationTable structure, the entire library is based on this
typedef struct AllocationTable {
  AllocatedData *ptr;
  uint64_t size;
  uint64_t cap;
  FreeList *__freelist;
} AllocationTable;

#ifdef _OPENMP
extern uint64_t __total_memory_allocated;
extern uint64_t __memory_not_cleaned;
#else
_Atomic extern uint64_t __total_memory_allocated;
_Atomic extern uint64_t __memory_not_cleaned;
#endif

#ifndef _OPENMP
extern pthread_mutex_t alloc_mutex;
#endif

// Increase the freelist
void __extend_allocations_freelist();

// Increase the allocations in the AllocationTable
void __extend_allocations();

// Needed to be called at the end of your program to make sure all the
// resources are cleaned
void __free_allocation_table();

// Custom malloc function, receives the size of the allocation and the position
// and in which file
void *__my_malloc(size_t size, char *position);

// Custom free function, looks for the address solicited in the allocation list,
// free the memory and put the pointer in the allocation list
void __my_free(void *ptr);

// malloc and free hijacking
#define malloc(size) __my_malloc(size, MEM_DBG_GET_LOG())
#define free(ptr) __my_free(ptr)
