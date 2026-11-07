# Simple Memory Debugger (SMDBG)

## Overview
Simple Memory Debugger is a lightweight, thread-safe development tool for C programs. It intercepts standard memory allocation calls to track memory usage and detect leaks, making it easier to find where memory was allocated but never freed.

This tool is strictly for debugging and development purposes and should not be used in production environments.

## How It Works
The library uses preprocessor macros to override standard `malloc` and `free` calls. When you include the header file, any call to `malloc` is redirected to a custom function (`__my_malloc`) that records the pointer, the size of the allocation, and the specific file and line number where the allocation occurred.

When the program terminates normally or crashes due to a system signal, such as a Segmentation Fault or a Ctrl+C interrupt, the debugger automatically generates a report file named `memdbg_allocations.txt`.

## Features
*   **Leak Detection:** Identifies exactly which file and line number forgot to call `free`.
*   **Thread-Safety:** Supports concurrent allocations across multiple threads, falling back on either OpenMP or standard POSIX threads (pthreads) depending on your compiler flags.
*   **Crash Handling:** Intercepts common POSIX signals like SIGSEGV, SIGINT or SIGABRT. to ensure a memory report is still dumped even if the program terminates unexpectedly.
*   **O(1) Free-List:** Uses an internal freelist to reuse slots in the allocation table efficiently.

## Usage
1. Include `SMDBG.h` in the source files where you want to track memory.
2. Compile your program alongside `SMDBG.c`.
3. Run your executable. 
4. Check `memdbg_allocations.txt` in your working directory after the program exits to see your total memory usage and any uncleaned allocations.

## Todo(s)
* Search for a better data structure to hold the AllocatedData to increase the free calls speed, currently it uses O(n) at most.

## Warning
Because this library intercepts signals and maintains global state using synchronization locks, it introduces performance overhead. It must be disabled or removed before compiling your software for production release.

