#include "soapy_sdr_shim.hpp"
#include <cstdio>
#include <stdexcept>

#if SOAPY_SDR_AVAILABLE

namespace shim {

SoapySDRShim& SoapySDRShim::get() {
    static SoapySDRShim instance;
    return instance;
}

bool SoapySDRShim::open(const std::string& driver, const std::string& serial) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (device_) {
        fprintf(stderr, "[SDR] Device already open\n");
        return true;
    }

    try {
        // Build device args
        SoapySDR::Kwargs args;
        if (!driver.empty()) {
            args["driver"] = driver;
        }
        if (!serial.empty()) {
            args["serial"] = serial;
        }

        fprintf(stderr, "[SDR] Opening device...\n");
        device_ = SoapySDR::Device::make(args);

        if (!device_) {
            fprintf(stderr, "[SDR] Failed to create device\n");
            return false;
        }

        // Query native stream format
        double fullScale = 0.0;
        std::string nativeFormat = device_->getNativeStreamFormat(direction_, 0, fullScale);
        fprintf(stderr, "[SDR] Native format: %s (fullScale=%.1f)\n", nativeFormat.c_str(), fullScale);

        // Try CS8 first (native 8-bit IQ, matches firmware)
        try {
            stream_ = device_->setupStream(direction_, SOAPY_SDR_CS8);
            use_cs8_ = true;
            fprintf(stderr, "[SDR] Using CS8 format (native 8-bit IQ)\n");
        } catch (const std::exception& e) {
            fprintf(stderr, "[SDR] CS8 not supported, falling back to CS16: %s\n", e.what());
            stream_ = device_->setupStream(direction_, SOAPY_SDR_CS16);
            use_cs8_ = false;
        }

        // Get device info
        driver_name_ = device_->getDriverKey();
        hardware_name_ = device_->getHardwareKey();

        fprintf(stderr, "[SDR] Opened %s (%s)\n", hardware_name_.c_str(), driver_name_.c_str());

        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] Open failed: %s\n", e.what());
        if (device_) {
            SoapySDR::Device::unmake(device_);
            device_ = nullptr;
        }
        return false;
    }
}

void SoapySDRShim::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        if (streaming_) {
            device_->deactivateStream(stream_);
            streaming_ = false;
        }

        if (stream_) {
            device_->closeStream(stream_);
            stream_ = nullptr;
        }

        SoapySDR::Device::unmake(device_);
        device_ = nullptr;

        fprintf(stderr, "[SDR] Device closed\n");

    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] Close error: %s\n", e.what());
    }
}

bool SoapySDRShim::is_open() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return device_ != nullptr;
}

void SoapySDRShim::set_frequency(double freq_hz) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        fprintf(stderr, "[SDR] set_frequency: device not open\n");
        return;
    }

    try {
        device_->setFrequency(direction_, 0, freq_hz);
        frequency_ = freq_hz;
        fprintf(stderr, "[SDR] Frequency set to %.3f MHz\n", freq_hz / 1e6);
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] set_frequency failed: %s\n", e.what());
    }
}

double SoapySDRShim::get_frequency() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frequency_;
}

void SoapySDRShim::set_lna_gain(int db) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        // Try LNA first (HackRF, LimeSDR)
        device_->setGain(direction_, 0, "LNA", db);
        fprintf(stderr, "[SDR] LNA gain set to %d dB\n", db);
    } catch (const std::exception& e1) {
        // Fallback to TUNER (RTL-SDR)
        try {
            device_->setGain(direction_, 0, "TUNER", db);
            fprintf(stderr, "[SDR] TUNER gain set to %d dB\n", db);
        } catch (const std::exception& e2) {
            fprintf(stderr, "[SDR] set_lna_gain failed: %s\n", e2.what());
        }
    }
}

void SoapySDRShim::set_vga_gain(int db) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        // Try VGA first (HackRF)
        device_->setGain(direction_, 0, "VGA", db);
        fprintf(stderr, "[SDR] VGA gain set to %d dB\n", db);
    } catch (const std::exception& e1) {
        // Fallback to IF (RTL-SDR)
        try {
            device_->setGain(direction_, 0, "IF", db);
            fprintf(stderr, "[SDR] IF gain set to %d dB\n", db);
        } catch (const std::exception& e2) {
            fprintf(stderr, "[SDR] set_vga_gain failed: %s\n", e2.what());
        }
    }
}

void SoapySDRShim::set_rf_amp(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        device_->setGain(direction_, 0, "AMP", enabled ? 14.0 : 0.0);
        fprintf(stderr, "[SDR] RF amp %s\n", enabled ? "enabled" : "disabled");
    } catch (const std::exception& e) {
        // AMP element doesn't exist on all devices — ignore silently
    }
}

void SoapySDRShim::set_tx_gain(int db) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        device_->setGain(SOAPY_SDR_TX, 0, db);
        fprintf(stderr, "[SDR] TX gain set to %d dB\n", db);
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] set_tx_gain failed: %s\n", e.what());
    }
}

void SoapySDRShim::set_sample_rate(double rate_hz) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        device_->setSampleRate(direction_, 0, rate_hz);
        sample_rate_ = rate_hz;
        fprintf(stderr, "[SDR] Sample rate set to %.3f MSPS\n", rate_hz / 1e6);
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] set_sample_rate failed: %s\n", e.what());
    }
}

double SoapySDRShim::get_sample_rate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sample_rate_;
}

void SoapySDRShim::set_bandwidth(double bw_hz) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_) {
        return;
    }

    try {
        device_->setBandwidth(direction_, 0, bw_hz);
        bandwidth_ = bw_hz;
        fprintf(stderr, "[SDR] Bandwidth set to %.3f MHz\n", bw_hz / 1e6);
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] set_bandwidth failed: %s\n", e.what());
    }
}

void SoapySDRShim::set_direction(int direction) {
    std::lock_guard<std::mutex> lock(mutex_);
    direction_ = direction;
    fprintf(stderr, "[SDR] Direction set to %s\n", direction == SOAPY_SDR_RX ? "RX" : "TX");
}

int SoapySDRShim::get_direction() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return direction_;
}

bool SoapySDRShim::start_stream() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_ || !stream_) {
        fprintf(stderr, "[SDR] start_stream: device or stream not initialized\n");
        return false;
    }

    if (streaming_) {
        return true;
    }

    try {
        device_->activateStream(stream_);
        streaming_ = true;
        fprintf(stderr, "[SDR] Stream started\n");
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] start_stream failed: %s\n", e.what());
        return false;
    }
}

void SoapySDRShim::stop_stream() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_ || !stream_ || !streaming_) {
        return;
    }

    try {
        device_->deactivateStream(stream_);
        streaming_ = false;
        fprintf(stderr, "[SDR] Stream stopped\n");
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] stop_stream failed: %s\n", e.what());
    }
}

bool SoapySDRShim::is_streaming() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

int SoapySDRShim::read_samples(iq_sample_t* buffer, size_t count, long timeout_us) {
    // DO NOT hold mutex during streaming read — SoapySDR handles thread safety internally
    // and blocking here would prevent configuration changes

    if (!device_ || !stream_ || !streaming_) {
        return -1;
    }

    try {
        if (use_cs8_) {
            // Direct read into CS8 buffer
            void* buffs[] = {buffer};
            int flags = 0;
            long long time_ns = 0;
            return device_->readStream(stream_, buffs, count, flags, time_ns, timeout_us);
        } else {
            // CS16 → CS8 conversion
            convert_buf_.resize(count * 2);
            void* buffs[] = {convert_buf_.data()};
            int flags = 0;
            long long time_ns = 0;
            int ret = device_->readStream(stream_, buffs, count, flags, time_ns, timeout_us);

            if (ret > 0) {
                for (int i = 0; i < ret; i++) {
                    buffer[i].i = static_cast<int8_t>(convert_buf_[i*2] >> 8);
                    buffer[i].q = static_cast<int8_t>(convert_buf_[i*2+1] >> 8);
                }
            }
            return ret;
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] read_samples failed: %s\n", e.what());
        return -1;
    }
}

int SoapySDRShim::write_samples(const iq_sample_t* buffer, size_t count, long timeout_us) {
    if (!device_ || !stream_ || !streaming_) {
        return -1;
    }

    try {
        if (use_cs8_) {
            // Direct write from CS8 buffer
            const void* buffs[] = {buffer};
            int flags = 0;
            long long time_ns = 0;
            return device_->writeStream(stream_, buffs, count, flags, time_ns, timeout_us);
        } else {
            // CS8 → CS16 conversion
            convert_buf_.resize(count * 2);
            for (size_t i = 0; i < count; i++) {
                convert_buf_[i*2] = static_cast<int16_t>(buffer[i].i) << 8;
                convert_buf_[i*2+1] = static_cast<int16_t>(buffer[i].q) << 8;
            }

            const void* buffs[] = {convert_buf_.data()};
            int flags = 0;
            long long time_ns = 0;
            return device_->writeStream(stream_, buffs, count, flags, time_ns, timeout_us);
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] write_samples failed: %s\n", e.what());
        return -1;
    }
}

std::string SoapySDRShim::get_driver_name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return driver_name_;
}

std::string SoapySDRShim::get_hardware_name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hardware_name_;
}

std::vector<std::string> SoapySDRShim::list_devices() {
    std::vector<std::string> result;

    try {
        SoapySDR::KwargsList devices = SoapySDR::Device::enumerate();

        for (const auto& dev : devices) {
            std::string desc;
            for (const auto& kv : dev) {
                if (!desc.empty()) desc += ", ";
                desc += kv.first + "=" + kv.second;
            }
            result.push_back(desc);
        }

        fprintf(stderr, "[SDR] Found %zu device(s)\n", result.size());

    } catch (const std::exception& e) {
        fprintf(stderr, "[SDR] list_devices failed: %s\n", e.what());
    }

    return result;
}

} // namespace shim

#else  // SOAPY_SDR_AVAILABLE == 0

// Stub implementations when SoapySDR is not available

namespace shim {

SoapySDRShim& SoapySDRShim::get() {
    static SoapySDRShim instance;
    return instance;
}

bool SoapySDRShim::open(const std::string& driver, const std::string& serial) {
    fprintf(stderr, "[SDR] SoapySDR not available (compiled without SOAPY_SDR_AVAILABLE)\n");
    return false;
}

void SoapySDRShim::close() {
}

bool SoapySDRShim::is_open() const {
    return false;
}

void SoapySDRShim::set_frequency(double freq_hz) {
    fprintf(stderr, "[SDR] set_frequency: SoapySDR not available\n");
}

double SoapySDRShim::get_frequency() const {
    return 0.0;
}

void SoapySDRShim::set_lna_gain(int db) {
}

void SoapySDRShim::set_vga_gain(int db) {
}

void SoapySDRShim::set_rf_amp(bool enabled) {
}

void SoapySDRShim::set_tx_gain(int db) {
}

void SoapySDRShim::set_sample_rate(double rate_hz) {
}

double SoapySDRShim::get_sample_rate() const {
    return 0.0;
}

void SoapySDRShim::set_bandwidth(double bw_hz) {
}

void SoapySDRShim::set_direction(int direction) {
}

int SoapySDRShim::get_direction() const {
    return 0;
}

bool SoapySDRShim::start_stream() {
    return false;
}

void SoapySDRShim::stop_stream() {
}

bool SoapySDRShim::is_streaming() const {
    return false;
}

int SoapySDRShim::read_samples(iq_sample_t* buffer, size_t count, long timeout_us) {
    return 0;  // No samples available
}

int SoapySDRShim::write_samples(const iq_sample_t* buffer, size_t count, long timeout_us) {
    return 0;
}

std::string SoapySDRShim::get_driver_name() const {
    return "none";
}

std::string SoapySDRShim::get_hardware_name() const {
    return "none";
}

std::vector<std::string> SoapySDRShim::list_devices() {
    return {};
}

} // namespace shim

#endif  // SOAPY_SDR_AVAILABLE
