#include "libiio_sdr.hpp"
#include <cstdio>
#include <cstring>

#if LIBIIO_AVAILABLE

#include <iio.h>

namespace shim {

LibIIOSDR::~LibIIOSDR() {
    close();
}

bool LibIIOSDR::open(const std::string& device_args) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (open_) {
        fprintf(stderr, "[IIO] Device already open\n");
        return true;
    }

    try {
        // Parse URI from device_args: "uri=ip:192.168.2.1" or "uri=local:"
        std::string uri = "local:";
        if (!device_args.empty()) {
            size_t pos = 0;
            std::string args = device_args;
            while (pos < args.size()) {
                size_t comma = args.find(',', pos);
                std::string token = (comma == std::string::npos)
                    ? args.substr(pos) : args.substr(pos, comma - pos);
                size_t eq = token.find('=');
                if (eq != std::string::npos) {
                    std::string key = token.substr(0, eq);
                    std::string val = token.substr(eq + 1);
                    if (key == "uri") uri = val;
                }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }

        fprintf(stderr, "[IIO] Connecting to %s...\n", uri.c_str());
        ctx_ = iio_create_context_from_uri(uri.c_str());
        if (!ctx_) {
            fprintf(stderr, "[IIO] Failed to create context from URI: %s\n", uri.c_str());
            return false;
        }

        // Find AD9361 PHY device (frequency/gain control)
        phy_ = iio_context_find_device(ctx_, "ad9361-phy");
        if (!phy_) {
            fprintf(stderr, "[IIO] ad9361-phy not found\n");
            close();
            return false;
        }

        // Find RX streaming device
        rx_dev_ = iio_context_find_device(ctx_, "cf-ad9361-lpc");
        if (!rx_dev_) {
            fprintf(stderr, "[IIO] cf-ad9361-lpc (RX) not found\n");
        }

        // Find TX streaming device
        tx_dev_ = iio_context_find_device(ctx_, "cf-ad9361-dds-core-lpc");
        if (!tx_dev_) {
            fprintf(stderr, "[IIO] cf-ad9361-dds-core-lpc (TX) not found\n");
        }

        // Enable RX I/Q channels
        if (rx_dev_) {
            rx_i_ = iio_device_find_channel(rx_dev_, "voltage0", false);
            rx_q_ = iio_device_find_channel(rx_dev_, "voltage1", false);
            if (rx_i_) iio_channel_enable(rx_i_);
            if (rx_q_) iio_channel_enable(rx_q_);
        }

        // Enable TX I/Q channels
        if (tx_dev_) {
            tx_i_ = iio_device_find_channel(tx_dev_, "voltage0", true);
            tx_q_ = iio_device_find_channel(tx_dev_, "voltage1", true);
            if (tx_i_) iio_channel_enable(tx_i_);
            if (tx_q_) iio_channel_enable(tx_q_);
        }

        open_ = true;
        fprintf(stderr, "[IIO] Opened PlutoSDR (AD9361)\n");
        return true;

    } catch (...) {
        fprintf(stderr, "[IIO] Open failed with exception\n");
        close();
        return false;
    }
}

void LibIIOSDR::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (streaming_) {
        if (rx_buf_) { iio_buffer_destroy(rx_buf_); rx_buf_ = nullptr; }
        if (tx_buf_) { iio_buffer_destroy(tx_buf_); tx_buf_ = nullptr; }
        streaming_ = false;
    }

    if (ctx_) {
        iio_context_destroy(ctx_);
        ctx_ = nullptr;
    }

    phy_ = nullptr;
    rx_dev_ = nullptr;
    tx_dev_ = nullptr;
    rx_i_ = rx_q_ = nullptr;
    tx_i_ = tx_q_ = nullptr;
    open_ = false;

    fprintf(stderr, "[IIO] Device closed\n");
}

bool LibIIOSDR::is_open() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_;
}

void LibIIOSDR::set_frequency(double freq_hz) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!phy_) return;

    long long freq = static_cast<long long>(freq_hz);

    // Set RX LO frequency
    iio_channel* rx_lo = iio_device_find_channel(phy_, "altvoltage0", true);
    if (rx_lo) {
        iio_channel_attr_write_longlong(rx_lo, "frequency", freq);
    }

    // Set TX LO frequency
    iio_channel* tx_lo = iio_device_find_channel(phy_, "altvoltage1", true);
    if (tx_lo) {
        iio_channel_attr_write_longlong(tx_lo, "frequency", freq);
    }

    frequency_ = freq_hz;
    fprintf(stderr, "[IIO] Frequency set to %.3f MHz\n", freq_hz / 1e6);
}

double LibIIOSDR::get_frequency() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frequency_;
}

void LibIIOSDR::set_lna_gain(int db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!phy_) return;

    iio_channel* ch = iio_device_find_channel(phy_, "voltage0", false);
    if (ch) {
        // AD9361 RX gain: set gain_control_mode to manual first
        iio_channel_attr_write(ch, "gain_control_mode", "manual");
        iio_channel_attr_write_longlong(ch, "hardwaregain", db);
        fprintf(stderr, "[IIO] RX gain set to %d dB\n", db);
    }
}

void LibIIOSDR::set_vga_gain(int db) {
    // AD9361 doesn't have separate VGA — map to RX gain
    set_lna_gain(db);
}

void LibIIOSDR::set_rf_amp(bool enabled) {
    (void)enabled;
    // AD9361 doesn't have a discrete RF amp switch
}

void LibIIOSDR::set_tx_gain(int db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!phy_) return;

    iio_channel* ch = iio_device_find_channel(phy_, "voltage0", true);
    if (ch) {
        // AD9361 TX attenuation is negative dB
        long long atten = -static_cast<long long>(db);
        iio_channel_attr_write_longlong(ch, "hardwaregain", atten);
        fprintf(stderr, "[IIO] TX gain set to %d dB (atten=%lld)\n", db, atten);
    }
}

void LibIIOSDR::set_sample_rate(double rate_hz) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!phy_) return;

    long long rate = static_cast<long long>(rate_hz);

    // Set both RX and TX sampling frequency
    iio_channel* rx_ch = iio_device_find_channel(phy_, "voltage0", false);
    if (rx_ch) {
        iio_channel_attr_write_longlong(rx_ch, "sampling_frequency", rate);
    }

    iio_channel* tx_ch = iio_device_find_channel(phy_, "voltage0", true);
    if (tx_ch) {
        iio_channel_attr_write_longlong(tx_ch, "sampling_frequency", rate);
    }

    sample_rate_ = rate_hz;
    fprintf(stderr, "[IIO] Sample rate set to %.3f MSPS\n", rate_hz / 1e6);
}

double LibIIOSDR::get_sample_rate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sample_rate_;
}

void LibIIOSDR::set_bandwidth(double bw_hz) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!phy_) return;

    long long bw = static_cast<long long>(bw_hz);

    iio_channel* rx_ch = iio_device_find_channel(phy_, "voltage0", false);
    if (rx_ch) {
        iio_channel_attr_write_longlong(rx_ch, "rf_bandwidth", bw);
    }

    iio_channel* tx_ch = iio_device_find_channel(phy_, "voltage0", true);
    if (tx_ch) {
        iio_channel_attr_write_longlong(tx_ch, "rf_bandwidth", bw);
    }

    bandwidth_ = bw_hz;
    fprintf(stderr, "[IIO] Bandwidth set to %.3f MHz\n", bw_hz / 1e6);
}

void LibIIOSDR::set_direction(int direction) {
    std::lock_guard<std::mutex> lock(mutex_);
    direction_ = direction;
    fprintf(stderr, "[IIO] Direction set to %s\n", direction == 0 ? "RX" : "TX");
}

int LibIIOSDR::get_direction() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return direction_;
}

bool LibIIOSDR::start_stream() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!open_) return false;
    if (streaming_) return true;

    static constexpr size_t BUF_SAMPLES = 1024 * 1024;

    if (direction_ == 0 && rx_dev_) {
        rx_buf_ = iio_device_create_buffer(rx_dev_, BUF_SAMPLES, false);
        if (!rx_buf_) {
            fprintf(stderr, "[IIO] Failed to create RX buffer\n");
            return false;
        }
    } else if (direction_ == 1 && tx_dev_) {
        tx_buf_ = iio_device_create_buffer(tx_dev_, BUF_SAMPLES, false);
        if (!tx_buf_) {
            fprintf(stderr, "[IIO] Failed to create TX buffer\n");
            return false;
        }
    } else {
        fprintf(stderr, "[IIO] No device for direction %d\n", direction_);
        return false;
    }

    streaming_ = true;
    fprintf(stderr, "[IIO] Stream started\n");
    return true;
}

void LibIIOSDR::stop_stream() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!streaming_) return;

    if (rx_buf_) { iio_buffer_destroy(rx_buf_); rx_buf_ = nullptr; }
    if (tx_buf_) { iio_buffer_destroy(tx_buf_); tx_buf_ = nullptr; }

    streaming_ = false;
    fprintf(stderr, "[IIO] Stream stopped\n");
}

bool LibIIOSDR::is_streaming() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

int LibIIOSDR::read_samples(iq_sample_t* buffer, size_t count, long timeout_us) {
    (void)timeout_us;

    if (!rx_buf_ || !streaming_ || !rx_i_ || !rx_q_) return -1;

    ssize_t nbytes = iio_buffer_refill(rx_buf_);
    if (nbytes < 0) {
        fprintf(stderr, "[IIO] Buffer refill failed: %zd\n", nbytes);
        return -1;
    }

    // AD9361 produces 12-bit samples in int16 format.
    // Read interleaved I/Q, shift right 4 bits to get int8.
    ptrdiff_t step = iio_buffer_step(rx_buf_);
    const char* start = static_cast<const char*>(iio_buffer_first(rx_buf_, rx_i_));
    const char* end = static_cast<const char*>(iio_buffer_end(rx_buf_));

    size_t available = static_cast<size_t>(end - start) / static_cast<size_t>(step);
    size_t n = (count < available) ? count : available;

    for (size_t i = 0; i < n; i++) {
        const int16_t* sample = reinterpret_cast<const int16_t*>(start + i * step);
        buffer[i].i = static_cast<int8_t>(sample[0] >> 4);
        buffer[i].q = static_cast<int8_t>(sample[1] >> 4);
    }

    return static_cast<int>(n);
}

int LibIIOSDR::write_samples(const iq_sample_t* buffer, size_t count, long timeout_us) {
    (void)timeout_us;

    if (!tx_buf_ || !streaming_ || !tx_i_ || !tx_q_) return -1;

    ptrdiff_t step = iio_buffer_step(tx_buf_);
    char* start = static_cast<char*>(iio_buffer_first(tx_buf_, tx_i_));
    const char* end = static_cast<const char*>(iio_buffer_end(tx_buf_));

    size_t available = static_cast<size_t>(end - start) / static_cast<size_t>(step);
    size_t n = (count < available) ? count : available;

    for (size_t i = 0; i < n; i++) {
        int16_t* sample = reinterpret_cast<int16_t*>(start + i * step);
        // Scale int8 up to int16 (shift left 4 for AD9361 12-bit)
        sample[0] = static_cast<int16_t>(buffer[i].i) << 4;
        sample[1] = static_cast<int16_t>(buffer[i].q) << 4;
    }

    ssize_t nbytes = iio_buffer_push(tx_buf_);
    if (nbytes < 0) {
        fprintf(stderr, "[IIO] Buffer push failed: %zd\n", nbytes);
        return -1;
    }

    return static_cast<int>(n);
}

std::string LibIIOSDR::get_driver_name() const {
    return "libiio";
}

std::string LibIIOSDR::get_hardware_name() const {
    return "PlutoSDR (AD9361)";
}

} // namespace shim

#else  // LIBIIO_AVAILABLE == 0

// Stub implementations when libiio is not available

namespace shim {

LibIIOSDR::~LibIIOSDR() {}

bool LibIIOSDR::open(const std::string& device_args) {
    (void)device_args;
    fprintf(stderr, "[IIO] libiio not available (compiled without LIBIIO_AVAILABLE)\n");
    return false;
}

void LibIIOSDR::close() {}

bool LibIIOSDR::is_open() const { return false; }

void LibIIOSDR::set_frequency(double freq_hz) { (void)freq_hz; }
double LibIIOSDR::get_frequency() const { return 0.0; }

void LibIIOSDR::set_lna_gain(int db) { (void)db; }
void LibIIOSDR::set_vga_gain(int db) { (void)db; }
void LibIIOSDR::set_rf_amp(bool enabled) { (void)enabled; }
void LibIIOSDR::set_tx_gain(int db) { (void)db; }

void LibIIOSDR::set_sample_rate(double rate_hz) { (void)rate_hz; }
double LibIIOSDR::get_sample_rate() const { return 0.0; }
void LibIIOSDR::set_bandwidth(double bw_hz) { (void)bw_hz; }

void LibIIOSDR::set_direction(int direction) { (void)direction; }
int LibIIOSDR::get_direction() const { return 0; }

bool LibIIOSDR::start_stream() { return false; }
void LibIIOSDR::stop_stream() {}
bool LibIIOSDR::is_streaming() const { return false; }

int LibIIOSDR::read_samples(iq_sample_t* buffer, size_t count, long timeout_us) {
    (void)buffer; (void)count; (void)timeout_us;
    return 0;
}

int LibIIOSDR::write_samples(const iq_sample_t* buffer, size_t count, long timeout_us) {
    (void)buffer; (void)count; (void)timeout_us;
    return 0;
}

std::string LibIIOSDR::get_driver_name() const { return "none"; }
std::string LibIIOSDR::get_hardware_name() const { return "none"; }

} // namespace shim

#endif  // LIBIIO_AVAILABLE
