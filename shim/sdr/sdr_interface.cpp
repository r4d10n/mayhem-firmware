#include "sdr_interface.hpp"
#include "soapy_sdr_shim.hpp"
#include "libiio_sdr.hpp"

#include <cstdio>

namespace shim {

std::unique_ptr<SDRInterface> SDRInterface::create(const std::string& backend, const std::string& device_args) {
    if (backend == "libiio" || backend == "plutosdr" || backend == "pluto") {
        auto sdr = std::make_unique<LibIIOSDR>();
        if (sdr->open(device_args)) return sdr;
        fprintf(stderr, "[SDR] libiio backend failed to open\n");
        return nullptr;
    }

    // Default: SoapySDR
    auto sdr = std::make_unique<SoapySDRShim>();
    if (sdr->open(device_args)) return sdr;
    fprintf(stderr, "[SDR] SoapySDR backend failed to open\n");
    return nullptr;
}

} // namespace shim
