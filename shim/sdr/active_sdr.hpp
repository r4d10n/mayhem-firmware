#pragma once

#include "sdr_interface.hpp"

namespace shim {

// Returns the currently active SDR backend, or nullptr if none
SDRInterface* active_sdr();

// Called by main_linux.cpp after opening a device
void set_active_sdr(SDRInterface* sdr);

} // namespace shim
