/*
 * Memory Map Shim for Linux
 * Provides stubs for firmware/common/memory_map.hpp
 * which references LPC43xx SRAM regions.
 */

#ifndef __MEMORY_MAP_SHIM_H__
#define __MEMORY_MAP_SHIM_H__

#include <cstdint>
#include <cstddef>

namespace portapack {
namespace memory {
namespace map {

struct MemoryRegion {
    uint32_t base_addr;
    size_t   region_size;

    constexpr uint32_t base() const { return base_addr; }
    constexpr size_t size() const { return region_size; }
    constexpr uint32_t end() const { return base_addr + region_size; }
};

/* These don't map to real addresses on Linux — just placeholders */
constexpr MemoryRegion shared_memory{0x10088000, 0x2000};     /* 8KB */
constexpr MemoryRegion m4_code{0x20000000, 0x10000};          /* 64KB */
constexpr MemoryRegion m4_code_hackrf{0x20000000, 0x10000};

} /* namespace map */
} /* namespace memory */
} /* namespace portapack */

#endif /* __MEMORY_MAP_SHIM_H__ */
