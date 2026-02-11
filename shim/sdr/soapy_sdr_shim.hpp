#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>

// Forward declare SoapySDR types to avoid requiring the header when SOAPY_SDR_AVAILABLE=0
#if SOAPY_SDR_AVAILABLE
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Version.hpp>
#endif

namespace shim {

// Matches firmware's complex8_t layout
struct iq_sample_t {
    int8_t i;
    int8_t q;
};

class SoapySDRShim {
public:
    static SoapySDRShim& get();  // Singleton

    // Device management
    bool open(const std::string& driver = "", const std::string& serial = "");
    void close();
    bool is_open() const;

    // Frequency
    void set_frequency(double freq_hz);
    double get_frequency() const;

    // Gain control (maps to SoapySDR gain elements)
    void set_lna_gain(int db);
    void set_vga_gain(int db);
    void set_rf_amp(bool enabled);
    void set_tx_gain(int db);

    // Sample rate & bandwidth
    void set_sample_rate(double rate_hz);
    double get_sample_rate() const;
    void set_bandwidth(double bw_hz);

    // Direction (RX or TX)
    void set_direction(int direction);  // 0=RX, 1=TX
    int get_direction() const;

    // Streaming
    bool start_stream();
    void stop_stream();
    bool is_streaming() const;

    // Read IQ samples (RX) — returns number of samples read, or -1 on error
    int read_samples(iq_sample_t* buffer, size_t count, long timeout_us = 100000);
    // Write IQ samples (TX)
    int write_samples(const iq_sample_t* buffer, size_t count, long timeout_us = 100000);

    // Device info
    std::string get_driver_name() const;
    std::string get_hardware_name() const;
    static std::vector<std::string> list_devices();

private:
    SoapySDRShim() = default;

#if SOAPY_SDR_AVAILABLE
    SoapySDR::Device* device_ = nullptr;
    SoapySDR::Stream* stream_ = nullptr;
#endif

    int direction_ = 0;  // SOAPY_SDR_RX
    double frequency_ = 0;
    double sample_rate_ = 0;
    double bandwidth_ = 0;
    bool streaming_ = false;
    std::string driver_name_;
    std::string hardware_name_;
    mutable std::mutex mutex_;

    // Format negotiation
    bool use_cs8_ = true;  // prefer CS8, fall back to CS16
    std::vector<int16_t> convert_buf_;  // temp buffer for CS16→CS8 conversion
};

} // namespace shim
