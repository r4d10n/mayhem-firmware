/*
 * FatFs Shim for Linux
 * Provides FatFs types and functions mapped to POSIX filesystem operations.
 * Drop-in replacement for the real ff.h.
 */

#ifndef _FF_H_
#define _FF_H_

#include <cstdint>
#include <cstddef>

#include "ffconf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- FatFs result codes ---- */
typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;

/* ---- FatFs types ---- */
/* Use unsigned short (not char16_t) to match the real FatFs integer.h.
 * The firmware's file.hpp defines separate path constructors for char16_t
 * and TCHAR; these must be distinct types to avoid overload ambiguity. */
typedef unsigned short TCHAR;
typedef unsigned short WCHAR;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef unsigned UINT;
typedef uint64_t FSIZE_t;
typedef DWORD    LBA_t;

/* TCHAR string literal macro */
#ifndef _T
#define _T(x) (const TCHAR*)(u ## x)
#endif

/* File access mode flags */
#define FA_READ          0x01
#define FA_WRITE         0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW    0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS   0x10
#define FA_OPEN_APPEND   0x30

/* File attribute bits */
#define AM_RDO  0x01
#define AM_HID  0x02
#define AM_SYS  0x04
#define AM_DIR  0x10
#define AM_ARC  0x20

/* FIL structure - wraps a POSIX file descriptor.
 * Must be at least the size the firmware expects.
 * Original FatFs FIL is 556 bytes — we pad to match. */
typedef struct {
    int     fd;          /* POSIX file descriptor */
    FSIZE_t fptr;        /* File read/write pointer */
    FSIZE_t obj_size;    /* File size */
    BYTE    flag;        /* File status flags */
    BYTE    err;         /* Error code */
    /* Padding to match FatFs FIL size (~556 bytes) */
    uint8_t _pad[544];
} FIL;

/* Directory object — named FFDIR to avoid clash with POSIX DIR from <dirent.h>.
 * Firmware code sees 'DIR' via the macro below. */
typedef struct {
    void*   dirp;        /* POSIX DIR* */
    char    pattern[256]; /* Search pattern */
    char    path[256];    /* Directory path */
    BYTE    err;
} FFDIR;
#define DIR FFDIR

/* File information structure */
typedef struct {
    FSIZE_t fsize;       /* File size */
    WORD    fdate;       /* Modified date */
    WORD    ftime;       /* Modified time */
    BYTE    fattrib;     /* Attribute */
    TCHAR   fname[256];  /* File name */
    TCHAR   altname[14]; /* Short file name */
} FILINFO;

/* Filesystem type constants */
#define FS_FAT12  1
#define FS_FAT16  2
#define FS_FAT32  3
#define FS_EXFAT  4

/* Filesystem object (minimal) */
typedef struct {
    BYTE    fs_type;
    DWORD   n_fatent;
    DWORD   csize;       /* Cluster size in sectors */
    DWORD   free_clst;
    DWORD   last_clst;
    DWORD   fsize;
} FATFS;

/* ---- FatFs function prototypes ---- */

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_close(FIL* fp);
FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br);
FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw);
FRESULT f_lseek(FIL* fp, FSIZE_t ofs);
FRESULT f_truncate(FIL* fp);
FRESULT f_sync(FIL* fp);
FRESULT f_opendir(DIR* dp, const TCHAR* path);
FRESULT f_closedir(DIR* dp);
FRESULT f_readdir(DIR* dp, FILINFO* fno);
FRESULT f_findfirst(DIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern);
FRESULT f_findnext(DIR* dp, FILINFO* fno);
FRESULT f_mkdir(const TCHAR* path);
FRESULT f_unlink(const TCHAR* path);
FRESULT f_rename(const TCHAR* path_old, const TCHAR* path_new);
FRESULT f_stat(const TCHAR* path, FILINFO* fno);
FRESULT f_chmod(const TCHAR* path, BYTE attr, BYTE mask);
FRESULT f_utime(const TCHAR* path, const FILINFO* fno);
FRESULT f_getfree(const TCHAR* path, DWORD* nclst, FATFS** fatfs);
FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt);

/* File access helpers */
#define f_eof(fp)   ((fp)->fptr >= (fp)->obj_size)
#define f_error(fp) ((fp)->err)
#define f_tell(fp)  ((fp)->fptr)
#define f_size(fp)  ((fp)->obj_size)

/* f_printf - simplified version */
int f_printf(FIL* fp, const TCHAR* fmt, ...);
int f_puts(const TCHAR* str, FIL* fp);
int f_putc(TCHAR c, FIL* fp);
TCHAR* f_gets(TCHAR* buff, int len, FIL* fp);

#ifdef __cplusplus
}
#endif

#endif /* _FF_H_ */
