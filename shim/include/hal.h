/*
 * ChibiOS/HAL Shim for Linux
 * Provides minimal HAL types, functions, and LPC43xx register stubs
 * needed by the firmware. Includes the REAL lpc43xx.inc for type
 * definitions, then overrides the peripheral pointer macros to
 * point to dummy instances rather than hardware addresses.
 */

#ifndef _HAL_H_
#define _HAL_H_

#include "ch.h"

#include <cstdint>
#include <cstddef>

/* ---- CMSIS-style qualifiers (must precede lpc43xx.inc) ---- */
#ifndef __IO
#define __IO volatile
#endif
/* __I is normally "volatile const" but on Linux these are dummy RAM instances,
 * not real hardware registers. Dropping const lets C++ default-construct them. */
#ifndef __I
#define __I  volatile
#endif
#ifndef __O
#define __O  volatile
#endif

/* ---- INLINE macro (used by chprintf.h) ---- */
#ifndef INLINE
#define INLINE inline
#endif

/* ---- HAL feature flags ---- */
#ifndef HAL_USE_RTC
#define HAL_USE_RTC 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Memory barriers (ARM → x86) ---- */
#ifndef __DMB
#define __DMB() __sync_synchronize()
#endif
#ifndef __DSB
#define __DSB() __sync_synchronize()
#endif
#ifndef __ISB
#define __ISB() __sync_synchronize()
#endif
#ifndef __WFI
#define __WFI() do { usleep(1000); } while(0)
#endif
#ifndef __WFE
#define __WFE() do { usleep(1000); } while(0)
#endif

/* ---- NVIC stubs ---- */
#define NVIC_EnableIRQ(n)       ((void)0)
#define NVIC_DisableIRQ(n)      ((void)0)
#define NVIC_SetPriority(n, p)  ((void)0)
#define NVIC_ClearPendingIRQ(n) ((void)0)
#define nvicEnableVector(n, p)  ((void)0)
#define nvicDisableVector(n)    ((void)0)

/* ---- ARM intrinsic stubs ---- */
#ifndef __SEV
#define __SEV() ((void)0)
#endif
#ifndef __get_APSR
#define __get_APSR() (0U)
#endif

/* ---- ChibiOS/ARM priority macros ---- */
#ifndef CORTEX_PRIORITY_MASK
#define CORTEX_PRIORITY_MASK(n) (0)
#endif
#ifndef LPC43XX_M4TXEVENT_IRQ_PRIORITY
#define LPC43XX_M4TXEVENT_IRQ_PRIORITY 0
#endif

/* IRQ types */
typedef int IRQn_Type;
#define M0_M4CORE_IRQn  1
#define M4CORE_IRQn     1
#define DMA_IRQn        2
#define USB0_IRQn       3
#define PIN_INT4_IRQn   4
#define SGPIO_IRQn      5
#define DAC_IRQn        6
#define TIMER0_IRQn     7
#define LCD_IRQn        8

#ifdef __cplusplus
}
#endif

/* ==============================================================
 * Include REAL LPC43xx register type definitions from the firmware.
 * This gives us exact struct layouts matching the static_asserts
 * in lpc43xx_cpp.hpp.
 * ============================================================== */
#include "lpc43xx.inc"

/* ==============================================================
 * Override peripheral pointer macros.
 * lpc43xx.inc defines e.g. LPC_SCU as ((LPC_SCU_Type*)0x40086000)
 * which would segfault on Linux. Redefine to point to extern
 * dummy instances allocated in hardware_stubs.cpp.
 * ============================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* Extern dummy instances (defined in hardware_stubs.cpp) */
extern LPC_SCU_Type       lpc_scu_instance;
extern LPC_CGU_Type       lpc_cgu_instance;
extern LPC_CREG_Type      lpc_creg_instance;
extern LPC_GPIO_Type      lpc_gpio_instance;
extern LPC_SGPIO_Type     lpc_sgpio_instance;
extern LPC_GPDMA_Type     lpc_gpdma_instance;
extern LPC_RTC_Type       lpc_rtc_instance;
extern LPC_TIMER_Type     lpc_timer0_instance;
extern LPC_TIMER_Type     lpc_timer1_instance;
extern LPC_TIMER_Type     lpc_timer2_instance;
extern LPC_TIMER_Type     lpc_timer3_instance;
extern LPC_SSPx_Type      lpc_ssp0_instance;
extern LPC_SSPx_Type      lpc_ssp1_instance;
extern LPC_CCU1_Type      lpc_ccu1_instance;
extern LPC_CCU2_Type      lpc_ccu2_instance;
extern LPC_RGU_Type       lpc_rgu_instance;
extern LPC_SDMMC_Type     lpc_sdmmc_instance;
extern LPC_SPIFI_Type     lpc_spifi_instance;
extern LPC_I2Cx_Type      lpc_i2c0_instance;

#ifdef __cplusplus
}
#endif

/* Undef hardware-address macros from lpc43xx.inc */
#undef LPC_SCU
#undef LPC_CGU
#undef LPC_CREG
#undef LPC_GPIO
#undef LPC_SGPIO
#undef LPC_GPDMA
#undef LPC_RTC
#undef LPC_TIMER0
#undef LPC_TIMER1
#undef LPC_TIMER2
#undef LPC_TIMER3
#undef LPC_SSP0
#undef LPC_SSP1
#undef LPC_CCU1
#undef LPC_CCU2
#undef LPC_RGU
#undef LPC_SDMMC
#undef LPC_SPIFI
#undef LPC_I2C0
#undef LPC_GPIO_INT

/* Redefine to point to dummy instances */
#define LPC_SCU       (&lpc_scu_instance)
#define LPC_CGU       (&lpc_cgu_instance)
#define LPC_CREG      (&lpc_creg_instance)
#define LPC_GPIO      (&lpc_gpio_instance)
#define LPC_SGPIO     (&lpc_sgpio_instance)
#define LPC_GPDMA     (&lpc_gpdma_instance)
#define LPC_RTC       (&lpc_rtc_instance)
#define LPC_TIMER0    (&lpc_timer0_instance)
#define LPC_TIMER1    (&lpc_timer1_instance)
#define LPC_TIMER2    (&lpc_timer2_instance)
#define LPC_TIMER3    (&lpc_timer3_instance)
#define LPC_SSP0      (&lpc_ssp0_instance)
#define LPC_SSP1      (&lpc_ssp1_instance)
#define LPC_CCU1      (&lpc_ccu1_instance)
#define LPC_CCU2      (&lpc_ccu2_instance)
#define LPC_RGU       (&lpc_rgu_instance)
#define LPC_SDMMC     (&lpc_sdmmc_instance)
#define LPC_SPIFI     (&lpc_spifi_instance)
#define LPC_I2C0      (&lpc_i2c0_instance)

/* GPIO interrupt - extern instance defined in hardware_stubs.cpp
 * (can't use static here because LPC_GPIO_INT_Type has __I members
 *  that make the default constructor deleted in C++) */
extern LPC_GPIO_INT_Type lpc_gpio_int_instance;
#define LPC_GPIO_INT  (&lpc_gpio_int_instance)

/* ---- PAL (Port Abstraction Layer) stubs ---- */
typedef uint32_t ioportmask_t;
typedef uint32_t ioportid_t;
typedef uint32_t iopadid_t;

#define PAL_MODE_INPUT              0
#define PAL_MODE_OUTPUT_PUSHPULL    1
#define PAL_MODE_RESET              2

#define palSetPad(port, pad)            ((void)0)
#define palClearPad(port, pad)          ((void)0)
#define palTogglePad(port, pad)         ((void)0)
#define palReadPad(port, pad)           0
#define palWritePad(port, pad, val)     ((void)0)
#define palSetPadMode(port, pad, mode)  ((void)0)
#define halPolledDelay(ticks)           ((void)0)
#define palSetGroupMode(port, mask, offset, mode) ((void)0)
#define palReadPort(port)               (0)
#define palSetPort(port, bits)          ((void)0)
#define palClearPort(port, bits)        ((void)0)

/* ---- RTC types (for HAL_USE_RTC) ---- */
struct RTCTime {
    uint32_t tv_date;
    uint32_t tv_time;
};

typedef LPC_RTC_Type RTC_TypeDef;

struct RTCDriver {
    RTC_TypeDef* rtc;
};

extern RTCDriver RTCD1;

#ifdef __cplusplus
extern "C" {
#endif

static inline void rtcGetTime(RTCDriver* rtcp, RTCTime* timespec) {
    (void)rtcp;
    if (timespec) { timespec->tv_date = 0; timespec->tv_time = 0; }
}
static inline void rtcSetTime(RTCDriver* rtcp, const RTCTime* timespec) {
    (void)rtcp; (void)timespec;
}

#ifdef __cplusplus
}
#endif

/* ---- I2C types ---- */
typedef struct {
    uint32_t high_count;
    uint32_t low_count;
} I2CConfig;

typedef uint8_t i2caddr_t;

typedef struct I2CDriver {
    int state;
} I2CDriver;

extern I2CDriver I2CD0;

#ifdef __cplusplus
extern "C" {
#endif

static inline void i2cStart(I2CDriver* i2cp, const I2CConfig* config) { (void)i2cp; (void)config; }
static inline void i2cStop(I2CDriver* i2cp) { (void)i2cp; }
static inline int i2cMasterTransmitTimeout(I2CDriver* i2cp, i2caddr_t addr,
    const uint8_t* txbuf, size_t txbytes, uint8_t* rxbuf, size_t rxbytes,
    systime_t timeout) {
    (void)i2cp; (void)addr; (void)txbuf; (void)txbytes;
    (void)rxbuf; (void)rxbytes; (void)timeout;
    return 0;
}
static inline void i2cAcquireBus(I2CDriver* i2cp) { (void)i2cp; }
static inline void i2cReleaseBus(I2CDriver* i2cp) { (void)i2cp; }

#ifdef __cplusplus
}
#endif

/* ---- SPI types ---- */
typedef struct {
    uint32_t cr0;
    uint32_t cpsr;
} SPIConfig;

typedef struct SPIDriver {
    int state;
} SPIDriver;

extern SPIDriver SPID2;

#ifdef __cplusplus
extern "C" {
#endif

static inline void spiStart(SPIDriver* spip, const SPIConfig* config) { (void)spip; (void)config; }
static inline void spiStop(SPIDriver* spip) { (void)spip; }
static inline void spiAcquireBus(SPIDriver* spip) { (void)spip; }
static inline void spiReleaseBus(SPIDriver* spip) { (void)spip; }
static inline void spiSelect(SPIDriver* spip) { (void)spip; }
static inline void spiUnselect(SPIDriver* spip) { (void)spip; }
static inline void spiExchange(SPIDriver* spip, size_t n, const void* txbuf, void* rxbuf) {
    (void)spip; (void)n; (void)txbuf; (void)rxbuf;
}
static inline void spiSend(SPIDriver* spip, size_t n, const void* txbuf) {
    (void)spip; (void)n; (void)txbuf;
}
static inline void spiReceive(SPIDriver* spip, size_t n, void* rxbuf) {
    (void)spip; (void)n; (void)rxbuf;
}

#ifdef __cplusplus
}
#endif

/* ---- SDC (SD Card) types ---- */
typedef struct {
    int dummy;
} SDCConfig;

typedef struct SDCDriver {
    int state;
    int inserted;
} SDCDriver;

typedef struct {
    uint32_t blk_num;
    uint32_t blk_size;
} BlockDeviceInfo;

extern SDCDriver SDCD1;

#ifdef __cplusplus
extern "C" {
#endif

void sdcStart(SDCDriver* sdcp, const SDCConfig* config);
void sdcStop(SDCDriver* sdcp);
bool sdcConnect(SDCDriver* sdcp);
void sdcDisconnect(SDCDriver* sdcp);
bool sdcIsCardInserted(SDCDriver* sdcp);
bool sdcGetInfo(SDCDriver* sdcp, BlockDeviceInfo* bdip);

void halLPCSetSystemClock(uint32_t freq);
void systick_adjust_period(uint32_t count);
void sdio_cclk_set(const size_t divider_value);
void configure_pins_portapack(void);

#ifdef __cplusplus
}
#endif

/* ---- BaseSequentialStream (for chprintf) ---- */
#ifdef __cplusplus
extern "C" {
#endif

struct BaseSequentialStreamVMT {
    int (*put)(void* instance, uint8_t b);
    int (*get)(void* instance);
    size_t (*write)(void* instance, const uint8_t* bp, size_t n);
    size_t (*read)(void* instance, uint8_t* bp, size_t n);
};

typedef struct {
    const struct BaseSequentialStreamVMT* vmt;
} BaseSequentialStream;

static inline size_t chSequentialStreamRead(BaseSequentialStream* ip, uint8_t* bp, size_t n) {
    (void)ip; (void)bp; (void)n; return 0;
}
static inline int chSequentialStreamPut(BaseSequentialStream* ip, uint8_t b) {
    (void)ip; (void)b; return 0;
}
static inline size_t chSequentialStreamWrite(BaseSequentialStream* ip, const uint8_t* bp, size_t n) {
    (void)ip; (void)bp; (void)n; return n;
}
static inline int chSequentialStreamGet(BaseSequentialStream* ip) {
    (void)ip; return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* _HAL_H_ */
