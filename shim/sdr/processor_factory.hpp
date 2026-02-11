#pragma once

/*
 * Processor Factory — maps image_tag_t to BasebandProcessor instances
 *
 * On real hardware, each baseband processor is a separate M4 binary
 * loaded via SPI flash. On Linux, all processors are compiled into the
 * main binary and instantiated here by tag lookup.
 */

#include "spi_image.hpp"
#include "baseband_processor.hpp"
#include <memory>

namespace shim {

/* Create a BasebandProcessor for the given image tag.
 * Returns nullptr for unknown or unsupported tags. */
std::unique_ptr<BasebandProcessor> create_processor(
    const portapack::spi_flash::image_tag_t& tag);

/* Return a human-readable name for the processor tag (for logging). */
const char* processor_name(const portapack::spi_flash::image_tag_t& tag);

} // namespace shim
