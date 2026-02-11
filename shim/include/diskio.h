/*
 * FatFs Disk I/O Shim for Linux
 * Provides the diskio.h types and function prototypes.
 * Actual disk I/O is handled by the POSIX fatfs_shim.
 */

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Status of Disk Functions */
typedef BYTE DSTATUS;

/* Results of Disk Functions */
typedef enum {
    RES_OK = 0,
    RES_ERROR,
    RES_WRPRT,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

/* Prototypes for disk control functions */
DSTATUS disk_initialize(BYTE pdrv);
DSTATUS disk_status(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff);

/* Disk Status Bits (DSTATUS) */
#define STA_NOINIT  0x01
#define STA_NODISK  0x02
#define STA_PROTECT 0x04

/* Command codes for disk_ioctl */
#define CTRL_SYNC       0
#define GET_SECTOR_COUNT 1
#define GET_SECTOR_SIZE  2
#define GET_BLOCK_SIZE   3
#define CTRL_TRIM        4

#ifdef __cplusplus
}
#endif

#endif /* _DISKIO_DEFINED */
