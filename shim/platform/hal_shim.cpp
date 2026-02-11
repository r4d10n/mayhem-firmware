/*
 * HAL Shim Implementation for Linux
 * Provides stub implementations for ChibiOS HAL functions.
 */

#include "hal.h"

#include <cstdio>
#include <cstring>

/* ---- Global driver instances ---- */

I2CDriver I2CD0 = {};
SPIDriver SPID2 = {};
SDCDriver SDCD1 = {0, 1, /* state=0, inserted=true */};

LPC_CGU_Type  lpc_cgu_instance = {};
LPC_CREG_Type lpc_creg_instance = {};

/* ---- SDC (SD Card) functions ---- */
/* SD card is "always present" on Linux — it's just a directory */

void sdcStart(SDCDriver* sdcp, const SDCConfig* config) {
    (void)config;
    sdcp->state = 1;
    sdcp->inserted = 1;
}

void sdcStop(SDCDriver* sdcp) {
    sdcp->state = 0;
}

bool sdcConnect(SDCDriver* sdcp) {
    (void)sdcp;
    return CH_SUCCESS; /* Always succeeds — filesystem is always available */
}

void sdcDisconnect(SDCDriver* sdcp) {
    (void)sdcp;
}

bool sdcIsCardInserted(SDCDriver* sdcp) {
    return sdcp->inserted;
}

bool sdcGetInfo(SDCDriver* sdcp, BlockDeviceInfo* bdip) {
    (void)sdcp;
    if (bdip) {
        bdip->blk_num = 1024 * 1024 * 2; /* 1GB in 512-byte blocks */
        bdip->blk_size = 512;
    }
    return CH_SUCCESS;
}

/* ---- Misc HAL stubs ---- */

void halLPCSetSystemClock(uint32_t freq) {
    (void)freq;
}

void systick_adjust_period(uint32_t count) {
    (void)count;
}
