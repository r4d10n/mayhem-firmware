/*
 * SharedMemory Shim for Linux
 * Allocates SharedMemory on the heap instead of at a fixed hardware address.
 * Replaces firmware/common/portapack_shared_memory.cpp
 */

#include "portapack_shared_memory.hpp"

/* Heap-allocate SharedMemory instead of mapping to fixed SRAM address */
static SharedMemory shared_memory_instance;
SharedMemory& shared_memory = shared_memory_instance;
