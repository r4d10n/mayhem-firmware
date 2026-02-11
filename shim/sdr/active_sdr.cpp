#include "active_sdr.hpp"

namespace shim {

static SDRInterface* g_active_sdr = nullptr;

SDRInterface* active_sdr() {
    return g_active_sdr;
}

void set_active_sdr(SDRInterface* sdr) {
    g_active_sdr = sdr;
}

} // namespace shim
