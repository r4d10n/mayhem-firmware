/*
 * External App Registry — compile-time registry of all external apps
 *
 * On ARM, external apps are loaded from .ppma files on the SD card.
 * On Linux, they're compiled directly into the binary. This registry
 * collects all their application_information_t structs so the menu
 * loader can enumerate them without SD card scanning.
 */

#ifndef __EXTERNAL_APP_REGISTRY_HPP__
#define __EXTERNAL_APP_REGISTRY_HPP__

#include "standalone_app.hpp"  // app_location_t — must come before external_app.hpp
#include "external_app.hpp"
#include <vector>

namespace shim {

/* Returns pointers to all compiled-in external app info structs. */
const std::vector<application_information_t*>& get_external_apps();

} // namespace shim

#endif /* __EXTERNAL_APP_REGISTRY_HPP__ */
