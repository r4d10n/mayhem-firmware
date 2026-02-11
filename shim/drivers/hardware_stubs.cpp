/*
 * Hardware Stubs for Linux Shim
 * Provides stub implementations for all hardware-dependent symbols
 * that application-logic code references.
 */

#include "ch.h"
#include "hal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>

/* ============================================================
 * LPC43xx register instances
 * These are never actually used for register access on Linux —
 * they just satisfy symbol references.
 * ============================================================ */

LPC_SCU_Type    lpc_scu_instance;
/* lpc_cgu_instance, lpc_creg_instance defined in hal_shim.cpp */
LPC_GPIO_Type   lpc_gpio_instance;
LPC_SGPIO_Type  lpc_sgpio_instance;
LPC_GPDMA_Type  lpc_gpdma_instance;
LPC_RTC_Type    lpc_rtc_instance;
LPC_TIMER_Type  lpc_timer0_instance;
LPC_TIMER_Type  lpc_timer1_instance;
LPC_TIMER_Type  lpc_timer2_instance;
LPC_TIMER_Type  lpc_timer3_instance;
LPC_SSPx_Type   lpc_ssp0_instance;
LPC_SSPx_Type   lpc_ssp1_instance;
LPC_CCU1_Type   lpc_ccu1_instance;
LPC_CCU2_Type   lpc_ccu2_instance;
LPC_RGU_Type    lpc_rgu_instance;
LPC_SDMMC_Type  lpc_sdmmc_instance;
LPC_SPIFI_Type  lpc_spifi_instance;
LPC_I2Cx_Type   lpc_i2c0_instance;
LPC_GPIO_INT_Type lpc_gpio_int_instance;

/* ============================================================
 * HAL driver instances
 * ============================================================ */

/* I2CD0, SPID2, SDCD1 defined in hal_shim.cpp */
RTCDriver  RTCD1 = {};

/* Linker symbol stub (ARM linker script symbol, not meaningful on Linux) */
uint32_t _textend = 0;

/* sdcStart..sdcGetInfo, halLPCSetSystemClock, systick_adjust_period defined in hal_shim.cpp */

void sdio_cclk_set(const size_t divider_value) {
    (void)divider_value;
}

/* ============================================================
 * chprintf / chvprintf stubs
 * ============================================================ */

extern "C" {

void chvprintf(BaseSequentialStream* chp, const char* fmt, va_list ap) {
    (void)chp;
    vfprintf(stderr, fmt, ap);
}

int chsnprintf(char* str, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return n;
}

/* I/O queue stubs */
void chIQInit(InputQueue* iqp, uint8_t* bp, size_t size, void (*infy)(void*)) {
    (void)iqp; (void)bp; (void)size; (void)infy;
}

void chOQInit(OutputQueue* oqp, uint8_t* bp, size_t size, void (*onfy)(void*)) {
    (void)oqp; (void)bp; (void)size; (void)onfy;
}

msg_t chIQPutI(InputQueue* iqp, uint8_t b) {
    (void)iqp; (void)b;
    return 0;
}

size_t chIQReadTimeout(InputQueue* iqp, uint8_t* bp, size_t n, systime_t timeout) {
    (void)iqp; (void)bp; (void)n; (void)timeout;
    return 0;
}

size_t chOQWriteTimeout(OutputQueue* oqp, const uint8_t* bp, size_t n, systime_t timeout) {
    (void)oqp; (void)bp; (void)n; (void)timeout;
    return n;
}

msg_t chIQGetTimeout(InputQueue* iqp, systime_t timeout) {
    (void)iqp; (void)timeout;
    return -1;
}

msg_t chOQPutTimeout(OutputQueue* oqp, uint8_t b, systime_t timeout) {
    (void)oqp; (void)b; (void)timeout;
    return 0;
}

/* get_fattime for FatFs RTC support */
uint32_t get_fattime(void) {
    /* Return a fixed timestamp: 2024-01-01 00:00:00 */
    return ((uint32_t)(2024 - 1980) << 25) | ((uint32_t)1 << 21) |
           ((uint32_t)1 << 16) | ((uint32_t)0 << 11) |
           ((uint32_t)0 << 5) | ((uint32_t)0 >> 1);
}

/* FatFs memory allocation (for LFN support) */
void* ff_memalloc(size_t msize) {
    return malloc(msize);
}

void ff_memfree(void* mblock) {
    free(mblock);
}

/* FatFs sync object stubs (for _FS_REENTRANT) */
int ff_cre_syncobj(uint8_t vol, void** sobj) {
    (void)vol;
    Semaphore* sem = (Semaphore*)malloc(sizeof(Semaphore));
    chSemInit(sem, 1);
    *sobj = sem;
    return 1;
}

int ff_del_syncobj(void* sobj) {
    free(sobj);
    return 1;
}

int ff_req_grant(void* sobj) {
    chSemWait((Semaphore*)sobj);
    return 1;
}

void ff_rel_grant(void* sobj) {
    chSemSignal((Semaphore*)sobj);
}

} /* extern "C" */

/* ============================================================
 * Platform detection
 * ============================================================ */

bool hackrf_r9 = false;

/* ============================================================
 * portapack namespace stubs
 * These provide the global objects declared in portapack.hpp
 * ============================================================ */

#include "portapack.hpp"

namespace portapack {

DeviceType device_type = DEV_PORTAPACK;

const char* init_error = nullptr;

/* Dummy GPIO objects for IO constructor — register accesses hit our stubs */
static constexpr Pin dummy_pin{0, 0};
static constexpr GPIO dummy_gpio{dummy_pin, 0, 0, 0};

portapack::IO io{
    dummy_gpio, dummy_gpio, dummy_gpio, dummy_gpio,
    dummy_gpio, dummy_gpio, dummy_gpio
};

lcd::ILI9341 display;

I2C i2c0(&I2CD0);
SPI ssp1(&SPID2);
portapack::USBSerial usb_serial;

si5351::Si5351 clock_generator{
    i2c0, 0x60
};

ClockManager clock_manager{
    i2c0, clock_generator
};

ReceiverModel receiver_model;
TransmitterModel transmitter_model;

uint32_t bl_tick_counter = 0;
bool antenna_bias = false;
uint16_t touch_threshold = 0;

TemperatureLogger temperature_logger;

bool async_tx_enabled = false;

void set_antenna_bias(const bool v) {
    antenna_bias = v;
}

bool get_antenna_bias() {
    return antenna_bias;
}

init_status_t init() {
    fprintf(stderr, "[SHIM] portapack::init() — stub (no hardware)\n");
    return init_status_t::INIT_SUCCESS;
}

void shutdown(const bool leave_screen_on) {
    (void)leave_screen_on;
    fprintf(stderr, "[SHIM] portapack::shutdown()\n");
}

static BacklightOnOff backlight_instance;

Backlight* backlight() {
    return &backlight_instance;
}

void setEventDispatcherToUSBSerial(EventDispatcher* evt) {
    (void)evt;
}

} /* namespace portapack */

/* ============================================================
 * LCD ILI9341 stubs
 * The display object needs its methods to exist.
 * In Phase 2, these will be replaced with WebSocket framebuffer.
 * ============================================================ */

namespace lcd {

bool ILI9341::read_display_status() { return true; }
uint32_t ILI9341::lcd_read_display_id() { return 0x9341; }
void ILI9341::init() {}
void ILI9341::shutdown() {}
void ILI9341::sleep(bool) {}
void ILI9341::wake(bool) {}

void ILI9341::fill_rectangle(ui::Rect r, const ui::Color c) {
    (void)r; (void)c;
    /* Phase 2: render to framebuffer */
}

void ILI9341::fill_rectangle_unrolled8(ui::Rect r, const ui::Color c) {
    fill_rectangle(r, c);
}

void ILI9341::draw_line(const ui::Point start, const ui::Point end, const ui::Color color) {
    (void)start; (void)end; (void)color;
}

void ILI9341::fill_circle(
    const ui::Point center,
    const ui::Dim radius,
    const ui::Color foreground,
    const ui::Color background) {
    (void)center; (void)radius; (void)foreground; (void)background;
}

void ILI9341::draw_pixel(const ui::Point p, const ui::Color color) {
    (void)p; (void)color;
}

void ILI9341::draw_bmp_from_bmp_hex_arr(const ui::Point p, const uint8_t* bitmap, const uint8_t* transparency_color) {
    (void)p; (void)bitmap; (void)transparency_color;
}

bool ILI9341::draw_bmp_from_sdcard_file(const ui::Point p, const std::filesystem::path& file) {
    (void)p; (void)file;
    return false;
}

void ILI9341::render_line(const ui::Point p, const uint16_t count, const ui::Color* line_buffer) {
    (void)p; (void)count; (void)line_buffer;
}

void ILI9341::render_box(const ui::Point p, const ui::Size s, const ui::Color* line_buffer) {
    (void)p; (void)s; (void)line_buffer;
}

void ILI9341::draw_pixels(const ui::Rect r, const ui::Color* const colors, const size_t count) {
    (void)r; (void)colors; (void)count;
}

void ILI9341::read_pixels(const ui::Rect r, ui::ColorRGB888* const colors, const size_t count) {
    (void)r; (void)colors; (void)count;
}

void ILI9341::draw_bitmap(
    const ui::Point p,
    const ui::Size size,
    const uint8_t* const data,
    const ui::Color foreground,
    const ui::Color background,
    uint8_t zoom_level) {
    (void)p; (void)size; (void)data; (void)foreground; (void)background; (void)zoom_level;
}

void ILI9341::draw_glyph(
    const ui::Point p,
    const ui::Glyph& glyph,
    const ui::Color foreground,
    const ui::Color background,
    uint8_t zoom_level) {
    (void)p; (void)glyph; (void)foreground; (void)background; (void)zoom_level;
}

void ILI9341::scroll_set_area(const ui::Coord top_y, const ui::Coord bottom_y) {
    (void)top_y; (void)bottom_y;
}

ui::Coord ILI9341::scroll_set_position(const ui::Coord position) {
    (void)position;
    return 0;
}

void ILI9341::scroll_disable() {
}

ui::Coord ILI9341::scroll(const int32_t delta) {
    (void)delta;
    return 0;
}

ui::Coord ILI9341::scroll_area_y(const ui::Coord y) const {
    (void)y;
    return 0;
}

} /* namespace lcd */

/* ============================================================
 * Radio stubs
 * ============================================================ */

namespace radio {

void set_direction(const rf::Direction new_direction) { (void)new_direction; }
bool set_tuning_frequency(const rf::Frequency frequency) { (void)frequency; return true; }
void set_rf_amp(const bool enabled) { (void)enabled; }
void set_lna_gain(const int_fast8_t db) { (void)db; }
void set_vga_gain(const int_fast8_t db) { (void)db; }
void set_tx_gain(const int_fast8_t db) { (void)db; }
void set_baseband_filter_bandwidth(const uint32_t bandwidth_minimum) { (void)bandwidth_minimum; }
void set_baseband_rate(const uint32_t rate) { (void)rate; }
void set_antenna_bias(const bool on) { (void)on; }
void set_fm_deviation(const uint32_t deviation) { (void)deviation; }
void disable() {}
void enable(const void*) {}

} /* namespace radio */

/* ============================================================
 * Audio stubs
 * ============================================================ */

namespace audio {
namespace output {
    void start() {}
    void stop() {}
    void mute() {}
    void unmute() {}
} /* namespace output */
namespace input {
    void start(void*) {}
    void stop() {}
} /* namespace input */

void set_rate(const uint32_t rate) { (void)rate; }
bool speaker_disable_supported() { return false; }
void headphone_volume(float v) { (void)v; }
void speaker_volume(float v) { (void)v; }
void set_speaker_disable(bool v) { (void)v; }
bool speaker_disable() { return false; }

} /* namespace audio */

/* ============================================================
 * IRQ controls stubs
 * ============================================================ */

extern "C" {

void lcd_frame_sync_configure(void) {}
void rtc_interrupt_enable(void) {}
void configure_pins_portapack(void) {}

} /* extern "C" */

/* irq_controls.cpp stubs */
void controls_init() {}
void controls_disable() {}

/* ============================================================
 * Baseband API stubs
 * ============================================================ */

#include "message.hpp"

/* baseband_api.cpp is compiled — only stub symbols it doesn't define */
namespace baseband {

void set_image(const uint32_t image_tag) { (void)image_tag; }
void request_beep() {}

} /* namespace baseband */

/* ============================================================
 * Touch namespace - forward declare to avoid duplicate Samples
 * ============================================================ */

namespace touch {
    struct Frame;
    struct DigitizerPoint;
    struct Metrics;
    class Calibration;
    class Manager;
}

namespace ui {
    struct Point;
}

/* ============================================================
 * Core control stubs
 * ============================================================ */

void m4_init(const uint32_t image_tag, const uint32_t memory_addr, const bool run) {
    (void)image_tag; (void)memory_addr; (void)run;
}

void m4_request_shutdown() {}
bool m4_is_running() { return false; }

/* ============================================================
 * Misc stubs
 * ============================================================ */

/* firmware_info.c stub */
extern "C" {

struct firmware_info_t {
    uint32_t magic;
    uint32_t struct_version;
    uint32_t version_string_addr;
    uint32_t version_md5;
};

const firmware_info_t firmware_info = {
    .magic = 0,
    .struct_version = 1,
    .version_string_addr = 0,
    .version_md5 = 0
};

/* debug logging stub */
void __debug_log(const char* msg) {
    fprintf(stderr, "[DEBUG] %s\n", msg);
}

/* LZ4 decompression stub */
int LZ4_decompress_safe(const char* src, char* dst, int compressedSize, int dstCapacity) {
    (void)src; (void)dst; (void)compressedSize; (void)dstCapacity;
    return -1; /* Decompression not available in shim */
}

} /* extern "C" */

/* ============================================================
 * Additional audio stubs
 * ============================================================ */

#include <string>
#include <functional>
#include "audio.hpp"

namespace audio {

namespace headphone {
    void set_volume(const volume_t volume) { (void)volume; }
    volume_range_t volume_range() {
        return {volume_t::decibel(-127), volume_t::decibel(0)};
    }
}

namespace debug {
    std::string codec_name() { return "WM8731-stub"; }
    size_t reg_count() { return 0; }
    size_t reg_bits() { return 9; }
    uint32_t reg_read(const size_t register_number) { (void)register_number; return 0; }
    void reg_write(const size_t register_number, const uint32_t value) { (void)register_number; (void)value; }
}

void output::update_audio_mute() {}

namespace input {
    void start(int8_t alc_mode, bool agc_enable) { (void)alc_mode; (void)agc_enable; }
    void loopback_mic_to_hp_enable() {}
    void loopback_mic_to_hp_disable() {}
}

void set_rate(Rate rate) { (void)rate; }

} /* namespace audio */

/* ============================================================
 * Touch interface stubs
 * ============================================================ */

#include "touch.hpp"

namespace touch {

Metrics calculate_metrics(const Frame& frame) {
    (void)frame;
    return Metrics{0.0f, 0.0f, 0.0f};
}

ui::Point Calibration::translate(const DigitizerPoint& p) const {
    (void)p;
    return {0, 0};
}

void Manager::feed(const Frame& frame) {
    (void)frame;
}

} /* namespace touch */

touch::Frame get_touch_frame() {
    touch::Frame f = {{0, 0, 0, 0}};
    return f;
}

/* ============================================================
 * Control/switch stubs
 * ============================================================ */

#include <bitset>

enum class Switch : uint8_t {
    Right = 0, Left = 1, Down = 2, Up = 3, Sel = 4, Dfu = 5, RotA = 6, RotB = 7
};

uint8_t get_switches_state() { return 0; }
uint32_t swizzled_switches() { return 0; }
uint8_t get_encoder_position() { return 0; }
bool switch_is_long_pressed(Switch s) { (void)s; return false; }

std::bitset<6> get_switches_long_press_config() { return 0; }
void set_switches_long_press_config(std::bitset<6> cfg) { (void)cfg; }
std::bitset<6> get_switches_repeat_config() { return 0; }
void set_switches_repeat_config(std::bitset<6> cfg) { (void)cfg; }

namespace control {
namespace debug {
    uint32_t switches() { return 0; }
}
}

/* ============================================================
 * I2C device stubs — implement virtual methods from real headers
 * so the vtables get emitted.
 * ============================================================ */

#include "i2cdev_ads1110.hpp"
#include "i2cdev_bh1750.hpp"
#include "i2cdev_bmx280.hpp"
#include "i2cdev_sht3x.hpp"
#include "i2cdev_sht4x.hpp"

namespace i2cdev {

/* ADS1110 — vtable anchor (virtual methods from header) */
bool I2cDev_ADS1110::init(uint8_t addr_) { (void)addr_; return false; }
void I2cDev_ADS1110::update() {}
uint16_t I2cDev_ADS1110::readVoltage() { return 0; }
bool I2cDev_ADS1110::write(const uint8_t value) { (void)value; return false; }
bool I2cDev_ADS1110::detect() { return false; }

/* BH1750 — vtable anchor */
bool I2cDev_BH1750::init(uint8_t addr_) { (void)addr_; return false; }
void I2cDev_BH1750::update() {}
uint16_t I2cDev_BH1750::readLight() { return 0; }

/* BMX280 — vtable anchor */
bool I2cDev_BMX280::init(uint8_t addr_) { (void)addr_; return false; }
void I2cDev_BMX280::update() {}
void I2cDev_BMX280::read_coeff() {}
void I2cDev_BMX280::set_sampling() {}
float I2cDev_BMX280::read_temperature() { return 0.0f; }
float I2cDev_BMX280::read_pressure() { return 0.0f; }

/* SHT3x — vtable anchor */
bool I2cDev_SHT3x::init(uint8_t addr_) { (void)addr_; return false; }
void I2cDev_SHT3x::update() {}
float I2cDev_SHT3x::read_temperature() { return 0.0f; }
float I2cDev_SHT3x::read_humidity() { return 0.0f; }

/* SHT4x — vtable anchor */
bool I2cDev_SHT4x::init(uint8_t addr_) { (void)addr_; return false; }
void I2cDev_SHT4x::update() {}

} /* namespace i2cdev */

/* ============================================================
 * Battery management stubs
 * ============================================================ */

#include "battery.hpp"

namespace battery {

bool BatteryManagement::isDetected() { return false; }
void BatteryManagement::set_calc_override(bool v) { (void)v; }
void BatteryManagement::getBatteryInfo(uint8_t& valid_mask, uint8_t& percent, uint16_t& voltage, int32_t& current) {
    valid_mask = 0; percent = 0; voltage = 0; current = 0;
}
float BatteryManagement::get_tte() { return 0.0f; }
float BatteryManagement::get_ttf() { return 0.0f; }

} /* namespace battery */

/* ============================================================
 * Radio debug stubs
 * ============================================================ */

namespace radio {
namespace debug {
    namespace first_if {
        uint32_t register_read(const size_t register_number) { (void)register_number; return 0; }
        void register_write(const size_t register_number, uint32_t value) { (void)register_number; (void)value; }
    }
    namespace second_if {
        uint32_t register_read(const size_t register_number) { (void)register_number; return 0; }
        void register_write(const size_t register_number, uint32_t value) { (void)register_number; (void)value; }
        int8_t temp_sense() { return 0; }
    }
}
void set_baseband_filter_bandwidth_rx(const uint32_t bandwidth_minimum) { (void)bandwidth_minimum; }
void set_baseband_filter_bandwidth_tx(const uint32_t bandwidth_minimum) { (void)bandwidth_minimum; }
void set_rx_max283x_iq_phase_calibration(const size_t v) { (void)v; }
void set_tx_max283x_iq_phase_calibration(const size_t v) { (void)v; }
} /* namespace radio */

/* ============================================================
 * Guru meditation / debug display stubs
 * ============================================================ */

void draw_guru_meditation(uint8_t source, const char* hint) {
    (void)source; (void)hint;
    fprintf(stderr, "[GURU] source=%d hint=%s\n", source, hint ? hint : "NULL");
}

void draw_guru_meditation(uint8_t source, const char* hint, struct extctx* ctxp, uint32_t cfsr) {
    (void)source; (void)hint; (void)ctxp; (void)cfsr;
    fprintf(stderr, "[GURU] source=%d hint=%s cfsr=0x%x\n", source, hint ? hint : "NULL", cfsr);
}

void m0_halt() {
    fprintf(stderr, "[STUB] m0_halt() called\n");
}

bool memory_dump(uint32_t* addr, uint32_t size, bool include_stack) {
    (void)addr; (void)size; (void)include_stack;
    return false;
}

bool stack_dump() {
    fprintf(stderr, "[STUB] stack_dump() called\n");
    return false;
}

/* ============================================================
 * Linker symbols (ARM process stack)
 * ============================================================ */

extern "C" {
uint32_t __process_stack_base__ = 0;
uint32_t __process_stack_end__ = 0;
}

/* ============================================================
 * Threads (Capture, Replay, UsbSerial) — use real headers
 * ============================================================ */

#include "capture_thread.hpp"
#include "usb_serial_thread.hpp"

/* CaptureThread — capture_thread.cpp not compiled, provide stubs */
CaptureThread::CaptureThread(
    std::unique_ptr<stream::Writer> writer,
    size_t write_size,
    size_t buffer_count,
    std::function<void()> success_callback,
    std::function<void(File::Error)> error_callback
) : config{write_size, buffer_count} {
    (void)writer;
    (void)success_callback; (void)error_callback;
}
CaptureThread::~CaptureThread() {}

/* UsbSerialThread — usb_serial.cpp not compiled */
UsbSerialThread::UsbSerialThread() {}
UsbSerialThread::~UsbSerialThread() {}
void UsbSerialThread::stop() {}
msg_t UsbSerialThread::static_fn(void* arg) { (void)arg; return 0; }
void UsbSerialThread::run() {}
void UsbSerialThread::create_thread() {}

/* ============================================================
 * BufferExchange — use real header
 * ============================================================ */

#include "buffer_exchange.hpp"

BufferExchange* BufferExchange::obj = nullptr;

BufferExchange::BufferExchange(CaptureConfig* const config) {
    (void)config;
    obj = this;
}

BufferExchange::BufferExchange(ReplayConfig* const config) {
    (void)config;
    obj = this;
}

BufferExchange::~BufferExchange() {
    obj = nullptr;
}

StreamBuffer* BufferExchange::get(FIFO<StreamBuffer*>* fifo) {
    (void)fifo;
    return nullptr;
}

StreamBuffer* BufferExchange::get_prefill(FIFO<StreamBuffer*>* fifo) {
    (void)fifo;
    return nullptr;
}

/* ============================================================
 * UsbSerialAsyncmsg stub
 * ============================================================ */

#include "usb_serial_asyncmsg.hpp"

/* Provide the generic template body (not defined in the header) */
template<typename STRINGCOVER>
void UsbSerialAsyncmsg::asyncmsg(const STRINGCOVER& data) {
    (void)data;
}

/* Explicit instantiation for std::string */
template void UsbSerialAsyncmsg::asyncmsg<std::string>(const std::string&);

void UsbSerialAsyncmsg::asyncmsg(const char* data) {
    (void)data;
}

/* ============================================================
 * CPLD data blocks
 * ============================================================ */

#include <array>

namespace portapack {
namespace cpld {
namespace rev_20150901 {
    extern const std::array<uint16_t, 3328> block_0 = {};
    extern const std::array<uint16_t, 512> block_1 = {};
}
namespace rev_20170522 {
    extern const std::array<uint16_t, 3328> block_0 = {};
    extern const std::array<uint16_t, 512> block_1 = {};
}
}
} /* namespace portapack */

/* ============================================================
 * I2C transmit stub
 * ============================================================ */

bool I2C::transmit(uint8_t addr, const uint8_t* data, size_t len, uint32_t timeout) {
    (void)addr; (void)data; (void)len; (void)timeout;
    return false;
}

/* ============================================================
 * Si5351 register read stub
 * ============================================================ */

namespace si5351 {
uint8_t Si5351::read_register(uint8_t addr) {
    (void)addr;
    return 0;
}
} /* namespace si5351 */

/* ============================================================
 * WM8731 audio codec vtable stub
 * Provide vtable symbols by declaring weak symbols
 * ============================================================ */

extern "C" {
/* Weak vtable symbol for WM8731 - satisfies linker but never called */
__attribute__((weak)) void _ZN7wolfson6wm87316WM8731D2Ev() {}
__attribute__((weak)) void _ZN7wolfson6wm87316WM8731D0Ev() {}
__attribute__((weak)) const void* _ZTVN7wolfson6wm87316WM8731E[16] = {0};
}

/* ============================================================
 * SDCardDebugView constructor stub
 * ============================================================ */

#include "ui_sd_card_debug.hpp"

namespace ui {
SDCardDebugView::SDCardDebugView(NavigationView& nav) : View() {
    (void)nav;
}
void SDCardDebugView::on_show() {}
void SDCardDebugView::on_hide() {}
void SDCardDebugView::focus() {}
void SDCardDebugView::on_status(const sd_card::Status status) { (void)status; }
void SDCardDebugView::on_test() {}
}

/* ============================================================
 * Additional m4_init overload for spi_flash types
 * ============================================================ */

#include "spi_image.hpp"

void m4_init(const portapack::spi_flash::image_tag_t image_tag,
             const portapack::memory::region_t memory_region,
             const bool run) {
    (void)image_tag; (void)memory_region; (void)run;
}

void m4_init_prepared(const uint32_t m4_code, const bool full_reset) {
    (void)m4_code; (void)full_reset;
}

/* ============================================================
 * I2C::receive stub
 * ============================================================ */

bool I2C::receive(
    const address_t slave_address,
    uint8_t* const data,
    const size_t count,
    const systime_t timeout) {
    (void)slave_address; (void)data; (void)count; (void)timeout;
    return false;
}

/* ============================================================
 * Si5351 additional stubs
 * ============================================================ */

namespace si5351 {
void Si5351::reset() {}
void Si5351::set_ms_frequency(
    const size_t ms_number,
    const uint32_t frequency,
    const uint32_t vco_frequency,
    const size_t r_div) {
    (void)ms_number; (void)frequency; (void)vco_frequency; (void)r_div;
}
} /* namespace si5351 */

/* ============================================================
 * portapack::IO::reference_oscillator stub
 * ============================================================ */

namespace portapack {
void IO::reference_oscillator(const bool enable) {
    (void)enable;
}
void IO::lcd_backlight(const bool enable) {
    (void)enable;
}
} /* namespace portapack */

/* I2C::probe stub */
bool I2C::probe(i2caddr_t addr, systime_t timeout) {
    (void)addr; (void)timeout;
    return false;
}

/* ============================================================
 * portapack::USBSerial method stubs
 * ============================================================ */

namespace portapack {
void USBSerial::initialize() {}
void USBSerial::dispatch() {}
void USBSerial::dispatch_transfer() {}
void USBSerial::on_channel_opened() {}
void USBSerial::on_channel_closed() {}
void USBSerial::enable_xtal() {}
void USBSerial::disable_pll0() {}
void USBSerial::setup_pll0() {}
void USBSerial::enable_pll0() {}
} /* namespace portapack */

/* ============================================================
 * Battery management statics
 * ============================================================ */

namespace battery {
bool BatteryManagement::calcOverride = false;
uint8_t BatteryManagement::calc_percent_voltage(uint16_t voltage) {
    (void)voltage;
    return 0;
}
} /* namespace battery */

/* ============================================================
 * create_shell_i2c / complete_i2chost_to_device_transfer stubs
 * ============================================================ */

class EventDispatcher;

extern "C" {
void create_shell_i2c(EventDispatcher* evtd) { (void)evtd; }
void complete_i2chost_to_device_transfer(uint8_t* data, size_t length) { (void)data; (void)length; }
}
