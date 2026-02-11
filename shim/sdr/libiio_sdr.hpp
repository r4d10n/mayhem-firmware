#pragma once

#include "sdr_interface.hpp"
#include <atomic>
#include <mutex>
#include <vector>

#if LIBIIO_AVAILABLE
struct iio_context;
struct iio_device;
struct iio_channel;
struct iio_buffer;
#endif

namespace shim {

class LibIIOSDR : public SDRInterface {
public:
    LibIIOSDR() = default;
    ~LibIIOSDR() override;

    // SDRInterface implementation
    bool open(const std::string& device_args = "") override;
    void close() override;
    bool is_open() const override;

    void set_frequency(double freq_hz) override;
    double get_frequency() const override;

    void set_lna_gain(int db) override;
    void set_vga_gain(int db) override;
    void set_rf_amp(bool enabled) override;
    void set_tx_gain(int db) override;

    void set_sample_rate(double rate_hz) override;
    double get_sample_rate() const override;
    void set_bandwidth(double bw_hz) override;

    void set_direction(int direction) override;
    int get_direction() const override;

    bool start_stream() override;
    void stop_stream() override;
    bool is_streaming() const override;

    int read_samples(iq_sample_t* buffer, size_t count, long timeout_us = 100000) override;
    int write_samples(const iq_sample_t* buffer, size_t count, long timeout_us = 100000) override;

    std::string get_driver_name() const override;
    std::string get_hardware_name() const override;

private:
#if LIBIIO_AVAILABLE
    iio_context* ctx_ = nullptr;
    iio_device* phy_ = nullptr;      // ad9361-phy
    iio_device* rx_dev_ = nullptr;    // cf-ad9361-lpc (RX)
    iio_device* tx_dev_ = nullptr;    // cf-ad9361-dds-core-lpc (TX)
    iio_channel* rx_i_ = nullptr;
    iio_channel* rx_q_ = nullptr;
    iio_channel* tx_i_ = nullptr;
    iio_channel* tx_q_ = nullptr;
    iio_buffer* rx_buf_ = nullptr;
    iio_buffer* tx_buf_ = nullptr;
#endif

    int direction_ = 0;  // 0=RX, 1=TX
    double frequency_ = 0;
    double sample_rate_ = 0;
    double bandwidth_ = 0;
    bool open_ = false;
    bool streaming_ = false;
    mutable std::mutex mutex_;
};

} // namespace shim
