/*
 *
 * Simple Memory Debugger
 * works by substituting the malloc and free call by a custom call
 * works with multithreading
 *
 * SHOULD NOT BE USED IN PRODUCTION, SHOULD ONLY USE FOR DEBUGING PURPOSES
 *
 */

#include "SMDBG.h"
#undef malloc
#undef free

// Defines the types correctly and makes sure the correct mutex is being used
// based on the library being used
#ifdef _OPENMP

uint64_t __total_memory_allocated = 0;
uint64_t __memory_not_cleaned = 0;

#define BEGIN_CRITICAL _Pragma("omp critical") {
#define END_CRITICAL }

#define ATOMIC_ADD(var, size) _Pragma("omp atomic") var += size
#define ATOMIC_SUB(var, size) _Pragma("omp atomic") var -= size

#else

_Atomic uint64_t __total_memory_allocated = 0;
_Atomic uint64_t __memory_not_cleaned = 0;
pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

#define ATOMIC_ADD(var, size) atomic_fetch_add(&var, size);
#define ATOMIC_SUB(var, size) atomic_fetch_sub(&var, size);

#define BEGIN_CRITICAL                                                         \
  pthread_mutex_lock(&alloc_mutex);                                            \
  {
#define END_CRITICAL                                                           \
  }                                                                            \
  pthread_mutex_unlock(&alloc_mutex);

#endif

AllocationTable allocation_table = {NULL};

// Internal function used to send warnings if any allocation can not be made
void *allocation_error(char *obj, void *alloc) {
  if (alloc == NULL) {
    fprintf(stderr, "ERROR: Not enough memory, tried to alloc for %s.\n", obj);
    exit(1);
  }

  return alloc;
}

// Need to look for a better way to write reports
void __report_allocations() {
  FILE *file = fopen("memdbg_allocations.txt", "w");
  if (!file)
    return;

  // Writes the total memory allocated and the memory not cleaned to the memory
  // report
  fprintf(file, "Total memory allocated: %lu\n", __total_memory_allocated);
  fprintf(file, "Total memory not cleaned: %lu\n", __memory_not_cleaned);

  char *last_position = NULL;

  if (allocation_table.ptr != NULL) {
    for (uint64_t i = 0; i < allocation_table.size; i++) {
      AllocatedData alloc = allocation_table.ptr[i];
      if (alloc.ptr != NULL) {
        // Write information about each not deallocated buffer, makes sure that
        // it does not make a thousand lines because of a loop, maybe add line
        // counting later
        if (last_position == NULL) {
          fprintf(file, "Not deallocated: %s\n", alloc.position);
        } else if (last_position != NULL &&
                   strcmp(last_position, alloc.position) != 0) {
          fprintf(file, "Not deallocated: %s\n", alloc.position);
        }

        // Frees and dups the next
        free(last_position);
        last_position = strdup(alloc.position);
      }
    }
  }
  fclose(file);
}

// used to clean the switch case inside the signal handler
#define MEM_DBG_SIG_CASE(sig)                                                  \
  case sig:                                                                    \
    fprintf(stderr, "ERROR: exited with signal: %s\n", #sig);                  \
    break;

void __mem_dbg_signal_handler(int sig) {
  // Reports and the handles the signal case
  __report_allocations();
  switch (sig) {
    MEM_DBG_SIG_CASE(SIGINT)
    MEM_DBG_SIG_CASE(SIGILL)
    MEM_DBG_SIG_CASE(SIGABRT)
    MEM_DBG_SIG_CASE(SIGFPE)
    MEM_DBG_SIG_CASE(SIGSEGV)
    MEM_DBG_SIG_CASE(SIGTERM)
    MEM_DBG_SIG_CASE(SIGHUP)
    MEM_DBG_SIG_CASE(SIGQUIT)
    MEM_DBG_SIG_CASE(SIGTRAP)
    MEM_DBG_SIG_CASE(SIGPIPE)
    MEM_DBG_SIG_CASE(SIGALRM)
  default:
    break;
  }
  _Exit(sig);
}

// Extends the freelist whenever is needed
void __extend_allocations_freelist() {
  FreeList *freelist = allocation_table.__freelist;

  if (freelist == NULL) {
    freelist = allocation_error("freelist", malloc(sizeof(FreeList)));

    freelist->ptr =
        allocation_error("freelist->ptr", malloc(sizeof(uint64_t) * 32));

    freelist->cap = 32;
    freelist->size = 0;
    allocation_table.__freelist = freelist;
  } else if (freelist->size == freelist->cap) {
    freelist->cap *= 2;
    freelist->ptr = allocation_error(
        "freelist->ptr",
        realloc(freelist->ptr, freelist->cap * sizeof(uint64_t)));
  }
}

void __extend_allocations() {
  // Makes sure there is space in the freelist
  __extend_allocations_freelist();
  if (allocation_table.ptr == NULL) {
    allocation_table.ptr = allocation_error(
        "allocation_table.ptr", malloc(sizeof(AllocatedData) * 1024));
    allocation_table.cap = 1024;
    allocation_table.size = 0;
  } else if (allocation_table.size == allocation_table.cap) {
    allocation_table.cap *= 2;
    allocation_table.ptr =
        allocation_error("allocation_table.ptr",
                         realloc(allocation_table.ptr,
                                 allocation_table.cap * sizeof(AllocatedData)));
  }
}

void __free_allocation_table() {
  // Saves the report on a file
  __report_allocations();

  if (allocation_table.ptr != NULL) {
    for (uint64_t i = 0; i < allocation_table.size; i++) {
      AllocatedData alloc = allocation_table.ptr[i];
      if (alloc.ptr != NULL) {
        // Free the pointer and then the position char*
        free(alloc.ptr);
        free(alloc.position);
      }
    }

    // Frees the entire AllocatedData buffer
    free(allocation_table.ptr);
    allocation_table.ptr = NULL;
    allocation_table.cap = 0;
    allocation_table.size = 0;

    // Free the entire freelist
    FreeList *freelist = allocation_table.__freelist;
    if (freelist != NULL) {
      free(freelist->ptr);
      free(freelist);
      allocation_table.__freelist = NULL;
    }
  }
}

void *__my_malloc(size_t size, char *position) {
  // Makes sure the program does not crash when calling malloc
  static uint8_t is_initialized = 0;

  if (!is_initialized) {
    BEGIN_CRITICAL
    // Double checks to prevent race conditions
    if (!is_initialized) {
      // Makes sure the allocation table is cleaned at the end, only works when
      // the programn terminates normally
      atexit(__free_allocation_table);
      // Make sure that when the program exits cleaning with signals
      signal(SIGINT, __mem_dbg_signal_handler);
      signal(SIGILL, __mem_dbg_signal_handler);
      signal(SIGABRT, __mem_dbg_signal_handler);
      signal(SIGFPE, __mem_dbg_signal_handler);
      signal(SIGSEGV, __mem_dbg_signal_handler);
      signal(SIGTERM, __mem_dbg_signal_handler);
      signal(SIGHUP, __mem_dbg_signal_handler);
      signal(SIGQUIT, __mem_dbg_signal_handler);
      signal(SIGTRAP, __mem_dbg_signal_handler);
      signal(SIGPIPE, __mem_dbg_signal_handler);
      signal(SIGALRM, __mem_dbg_signal_handler);
      // sets the flag to true
      is_initialized = 1;
    }

    END_CRITICAL
  }

  ATOMIC_ADD(__memory_not_cleaned, size);
  ATOMIC_ADD(__total_memory_allocated, size);

  void *ptr = malloc(size);

  if (ptr != NULL) {
    BEGIN_CRITICAL

    // Makes sure there is enough space in the buffers
    __extend_allocations();
    FreeList *freelist = allocation_table.__freelist;
    AllocatedData *alloc;
    // Checks if there is space in the freelist if not get a new position in the
    // AllocatedData buffer
    if (freelist->size > 0) {
      freelist->size -= 1;
      // Get the free indices and uses for the alloc
      uint64_t free_index = freelist->ptr[freelist->size];
      alloc = &allocation_table.ptr[free_index];
    } else {
      alloc = &allocation_table.ptr[allocation_table.size];
      allocation_table.size += 1;
    }

    alloc->ptr = ptr;
    alloc->size = size;

    // Checks if its NULL, because maybe the user called the raw function for
    // whatever reason
    if (position != NULL) {
      alloc->position = strdup(position);
    } else {
      alloc->position = NULL;
    }

    END_CRITICAL

    return ptr;
  }

  fprintf(stderr, "ERROR: out of memory, tried to alloc %lu\n", size);
  exit(1);
}

void __my_free(void *ptr) {
  uint8_t found = 0;

  if (ptr != NULL) {
    BEGIN_CRITICAL
    // Checks every position for the correct pointer, need to find a way to make
    // this more cleaner O(n) is not good enough
    for (uint64_t i = 0; i < allocation_table.size; i++) {
      AllocatedData *alloc = &allocation_table.ptr[i];
      if (alloc->ptr == ptr) {

        ATOMIC_SUB(__memory_not_cleaned, alloc->size);

        alloc->size = 0;
        free(alloc->ptr);
        free(alloc->position);
        alloc->position = NULL;
        alloc->ptr = NULL;

        FreeList *freelist = allocation_table.__freelist;

        // Guarantees that exists space in the freelist before inserting
        if (freelist->size == freelist->cap) {
          __extend_allocations_freelist();
        }

        // Stores the indices
        freelist->ptr[freelist->size] = i;
        freelist->size += 1;

        found = 1;
        break;
      }
    }
    END_CRITICAL
  }

  if (!found) {
    fprintf(stderr, "ERROR: Tried to free() a pointer that is not present or "
                    "is already freed!\n");
    exit(1);
  }
}
