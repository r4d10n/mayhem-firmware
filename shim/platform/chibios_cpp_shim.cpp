/*
 * ChibiOS C++ Shim for Linux
 * Replaces firmware/common/chibios_cpp.cpp
 * Uses standard malloc/free instead of ChibiOS heap.
 */

#include "chibios_cpp.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <new>

void* operator new(size_t size) {
    void* p = malloc(size);
    if (p == nullptr) {
        fprintf(stderr, "[SHIM] Out of memory (new %zu bytes)\n", size);
        abort();
    }
    return p;
}

void* operator new[](size_t size) {
    void* p = malloc(size);
    if (p == nullptr) {
        fprintf(stderr, "[SHIM] Out of memory (new[] %zu bytes)\n", size);
        abort();
    }
    return p;
}

void operator delete(void* p) noexcept {
    free(p);
}

void operator delete[](void* p) noexcept {
    free(p);
}

void operator delete(void* ptr, std::size_t) noexcept {
    ::operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    ::operator delete(ptr);
}

namespace chibios {

size_t heap_size() {
    return 256 * 1024 * 1024; /* Report 256MB available (Linux has plenty) */
}

size_t heap_used() {
    return 0; /* Can't easily track with malloc; report 0 */
}

} /* namespace chibios */
