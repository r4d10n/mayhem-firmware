#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace shim {

// Matches firmware's complex8_t layout — shared across all SDR backends
struct iq_sample_t {
    int8_t i;
    int8_t q;
};

class SDRInterface {
public:
    virtual ~SDRInterface() = default;

    // Device management
    virtual bool open(const std::string& device_args = "") = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    // Frequency
    virtual void set_frequency(double freq_hz) = 0;
    virtual double get_frequency() const = 0;

    // Gain control
    virtual void set_lna_gain(int db) = 0;
    virtual void set_vga_gain(int db) = 0;
    virtual void set_rf_amp(bool enabled) = 0;
    virtual void set_tx_gain(int db) = 0;

    // Sample rate & bandwidth
    virtual void set_sample_rate(double rate_hz) = 0;
    virtual double get_sample_rate() const = 0;
    virtual void set_bandwidth(double bw_hz) = 0;

    // Direction (0=RX, 1=TX)
    virtual void set_direction(int direction) = 0;
    virtual int get_direction() const = 0;

    // Streaming
    virtual bool start_stream() = 0;
    virtual void stop_stream() = 0;
    virtual bool is_streaming() const = 0;

    // IQ sample I/O — returns number of samples, or -1 on error
    virtual int read_samples(iq_sample_t* buffer, size_t count, long timeout_us = 100000) = 0;
    virtual int write_samples(const iq_sample_t* buffer, size_t count, long timeout_us = 100000) = 0;

    // Device info
    virtual std::string get_driver_name() const = 0;
    virtual std::string get_hardware_name() const = 0;

    // Factory — creates a backend by name ("soapy", "libiio", "plutosdr")
    static std::unique_ptr<SDRInterface> create(const std::string& backend, const std::string& device_args = "");
};

} // namespace shim
