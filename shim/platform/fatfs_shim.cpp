/*
 * FatFs POSIX Implementation
 * Maps FatFs API calls to POSIX filesystem operations.
 * All paths are relative to a configurable SD card root directory.
 *
 * TCHAR is char16_t (matching _LFN_UNICODE=1 in the firmware).
 * Internal POSIX calls use char (UTF-8/ASCII); conversion helpers bridge the gap.
 */

#include "ff.h"
/* Undo the DIR->FFDIR macro so POSIX <dirent.h> can define its own DIR */
#undef DIR

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <ctime>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <fnmatch.h>

/* After #undef, 'DIR' means POSIX DIR in this file.
 * FatFs directory objects use 'FFDIR' explicitly. */

/* SD card root directory — all firmware paths are relative to this.
 * Set by the shim init code. */
static std::string sdcard_root = ".";

/* ---- char16_t <-> char conversion helpers ---- */

/* Narrow a TCHAR (char16_t) string to std::string.
 * Firmware paths are ASCII, so simple narrowing is sufficient. */
static std::string tchar_to_str(const TCHAR* s) {
    if (!s) return "";
    std::string result;
    while (*s) {
        result += static_cast<char>(*s & 0xFF);
        s++;
    }
    return result;
}

/* Widen a char string into a TCHAR (char16_t) buffer. */
static void str_to_tchar(const char* s, TCHAR* buf, size_t bufcount) {
    size_t i = 0;
    if (s) {
        for (; s[i] && i < bufcount - 1; i++) {
            buf[i] = static_cast<TCHAR>(static_cast<unsigned char>(s[i]));
        }
    }
    buf[i] = 0;
}

extern "C" {

void fatfs_shim_set_root(const char* root) {
    sdcard_root = root;
    /* Ensure it ends without trailing slash */
    while (sdcard_root.size() > 1 && sdcard_root.back() == '/') {
        sdcard_root.pop_back();
    }
}

const char* fatfs_shim_get_root(void) {
    return sdcard_root.c_str();
}

}

/* Convert a firmware FatFs path (TCHAR*) to a full POSIX path */
static std::string to_posix_path(const TCHAR* fatfs_path) {
    std::string p = tchar_to_str(fatfs_path);

    /* Strip drive prefix if present (e.g., "0:/" or "/") */
    if (p.size() >= 2 && p[1] == ':') {
        p = p.substr(2);
    }

    /* Ensure leading slash */
    if (p.empty() || p[0] != '/') {
        p = "/" + p;
    }

    return sdcard_root + p;
}

/* Convert POSIX stat to FILINFO */
static void stat_to_filinfo(const struct stat& st, const char* name, FILINFO* fno) {
    fno->fsize = (FSIZE_t)st.st_size;
    fno->fattrib = S_ISDIR(st.st_mode) ? AM_DIR : 0;
    if (!(st.st_mode & S_IWUSR)) fno->fattrib |= AM_RDO;

    /* Convert time */
    struct tm* tm = localtime(&st.st_mtime);
    if (tm) {
        fno->fdate = (WORD)(((tm->tm_year - 80) << 9) |
                            ((tm->tm_mon + 1) << 5) |
                            tm->tm_mday);
        fno->ftime = (WORD)((tm->tm_hour << 11) |
                            (tm->tm_min << 5) |
                            (tm->tm_sec / 2));
    }

    if (name) {
        str_to_tchar(name, fno->fname, sizeof(fno->fname) / sizeof(TCHAR));
    }
    fno->altname[0] = 0;
}

/* ---- File operations ---- */

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    std::string full = to_posix_path(path);

    int flags = 0;
    if ((mode & (FA_READ | FA_WRITE)) == (FA_READ | FA_WRITE)) {
        flags = O_RDWR;
    } else if (mode & FA_WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }

    if (mode & FA_CREATE_ALWAYS) {
        flags |= O_CREAT | O_TRUNC;
    } else if (mode & FA_CREATE_NEW) {
        flags |= O_CREAT | O_EXCL;
    } else if (mode & FA_OPEN_ALWAYS) {
        flags |= O_CREAT;
    }

    if (mode & FA_OPEN_APPEND) {
        flags |= O_CREAT | O_APPEND;
    }

    int fd = open(full.c_str(), flags, 0666);
    if (fd < 0) {
        switch (errno) {
            case ENOENT: return FR_NO_FILE;
            case EACCES: return FR_DENIED;
            case EEXIST: return FR_EXIST;
            default:     return FR_DISK_ERR;
        }
    }

    memset(fp, 0, sizeof(FIL));
    fp->fd = fd;
    fp->flag = mode;
    fp->err = 0;

    /* Get file size */
    struct stat st;
    if (fstat(fd, &st) == 0) {
        fp->obj_size = (FSIZE_t)st.st_size;
    }

    /* Handle append */
    if (mode & FA_OPEN_APPEND) {
        fp->fptr = fp->obj_size;
    }

    return FR_OK;
}

FRESULT f_close(FIL* fp) {
    if (fp->fd >= 0) {
        close(fp->fd);
        fp->fd = -1;
    }
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br) {
    if (br) *br = 0;
    if (fp->fd < 0) return FR_INVALID_OBJECT;

    ssize_t n = read(fp->fd, buff, btr);
    if (n < 0) return FR_DISK_ERR;

    fp->fptr += (FSIZE_t)n;
    if (br) *br = (UINT)n;
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw) {
    if (bw) *bw = 0;
    if (fp->fd < 0) return FR_INVALID_OBJECT;

    ssize_t n = write(fp->fd, buff, btw);
    if (n < 0) return FR_DISK_ERR;

    fp->fptr += (FSIZE_t)n;
    if (fp->fptr > fp->obj_size) {
        fp->obj_size = fp->fptr;
    }
    if (bw) *bw = (UINT)n;
    return FR_OK;
}

FRESULT f_lseek(FIL* fp, FSIZE_t ofs) {
    if (fp->fd < 0) return FR_INVALID_OBJECT;

    off_t result = lseek(fp->fd, (off_t)ofs, SEEK_SET);
    if (result < 0) return FR_DISK_ERR;

    fp->fptr = (FSIZE_t)result;
    return FR_OK;
}

FRESULT f_truncate(FIL* fp) {
    if (fp->fd < 0) return FR_INVALID_OBJECT;

    if (ftruncate(fp->fd, (off_t)fp->fptr) != 0) {
        return FR_DISK_ERR;
    }
    fp->obj_size = fp->fptr;
    return FR_OK;
}

FRESULT f_sync(FIL* fp) {
    if (fp->fd < 0) return FR_INVALID_OBJECT;
    fsync(fp->fd);
    return FR_OK;
}

/* ---- Directory operations ---- */

FRESULT f_opendir(FFDIR* dp, const TCHAR* path) {
    std::string full = to_posix_path(path);

    ::DIR* dirp = opendir(full.c_str());
    if (!dirp) {
        return (errno == ENOENT) ? FR_NO_PATH : FR_DISK_ERR;
    }

    dp->dirp = dirp;
    strncpy(dp->path, full.c_str(), sizeof(dp->path) - 1);
    dp->path[sizeof(dp->path) - 1] = '\0';
    dp->pattern[0] = '\0';
    dp->err = 0;
    return FR_OK;
}

FRESULT f_closedir(FFDIR* dp) {
    if (dp->dirp) {
        closedir((::DIR*)dp->dirp);
        dp->dirp = nullptr;
    }
    return FR_OK;
}

FRESULT f_readdir(FFDIR* dp, FILINFO* fno) {
    if (!dp->dirp) return FR_INVALID_OBJECT;

    /* NULL fno means rewind */
    if (!fno) {
        rewinddir((::DIR*)dp->dirp);
        return FR_OK;
    }

    struct dirent* entry;
    while ((entry = readdir((::DIR*)dp->dirp)) != nullptr) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* If pattern is set, filter */
        if (dp->pattern[0] && fnmatch(dp->pattern, entry->d_name, FNM_CASEFOLD) != 0) {
            continue;
        }

        /* Get file info */
        std::string fullpath = std::string(dp->path) + "/" + entry->d_name;
        struct stat st;
        if (::stat(fullpath.c_str(), &st) != 0) {
            continue;
        }

        stat_to_filinfo(st, entry->d_name, fno);
        return FR_OK;
    }

    /* End of directory */
    fno->fname[0] = 0;
    return FR_OK;
}

FRESULT f_findfirst(FFDIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern) {
    FRESULT res = f_opendir(dp, path);
    if (res != FR_OK) return res;

    if (pattern) {
        std::string pat = tchar_to_str(pattern);
        strncpy(dp->pattern, pat.c_str(), sizeof(dp->pattern) - 1);
        dp->pattern[sizeof(dp->pattern) - 1] = '\0';
    }

    return f_readdir(dp, fno);
}

FRESULT f_findnext(FFDIR* dp, FILINFO* fno) {
    return f_readdir(dp, fno);
}

/* ---- Path operations ---- */

FRESULT f_mkdir(const TCHAR* path) {
    std::string full = to_posix_path(path);
    if (mkdir(full.c_str(), 0755) != 0) {
        if (errno == EEXIST) return FR_EXIST;
        return FR_DISK_ERR;
    }
    return FR_OK;
}

FRESULT f_unlink(const TCHAR* path) {
    std::string full = to_posix_path(path);
    if (remove(full.c_str()) != 0) {
        if (errno == ENOENT) return FR_NO_FILE;
        return FR_DISK_ERR;
    }
    return FR_OK;
}

FRESULT f_rename(const TCHAR* path_old, const TCHAR* path_new) {
    std::string full_old = to_posix_path(path_old);
    std::string full_new = to_posix_path(path_new);
    if (rename(full_old.c_str(), full_new.c_str()) != 0) {
        return FR_DISK_ERR;
    }
    return FR_OK;
}

FRESULT f_stat(const TCHAR* path, FILINFO* fno) {
    std::string full = to_posix_path(path);
    struct stat st;
    if (::stat(full.c_str(), &st) != 0) {
        if (errno == ENOENT) return FR_NO_FILE;
        return FR_DISK_ERR;
    }

    if (fno) {
        /* Extract filename from the POSIX path */
        const char* cstr = full.c_str();
        const char* name = strrchr(cstr, '/');
        name = name ? name + 1 : cstr;
        stat_to_filinfo(st, name, fno);
    }
    return FR_OK;
}

FRESULT f_chmod(const TCHAR* path, BYTE attr, BYTE mask) {
    (void)path; (void)attr; (void)mask;
    return FR_OK; /* No-op on Linux */
}

FRESULT f_utime(const TCHAR* path, const FILINFO* fno) {
    (void)path; (void)fno;
    return FR_OK; /* No-op on Linux — file times set by OS */
}

FRESULT f_getfree(const TCHAR* path, DWORD* nclst, FATFS** fatfs) {
    static const TCHAR root_path[] = { '/', 0 };
    std::string full = to_posix_path(path ? path : root_path);

    struct statvfs svfs;
    if (statvfs(full.c_str(), &svfs) != 0) {
        return FR_DISK_ERR;
    }

    static FATFS dummy_fs;
    dummy_fs.csize = 1; /* 1 sector per cluster for simplicity */
    dummy_fs.free_clst = (DWORD)(svfs.f_bavail);

    if (nclst) *nclst = dummy_fs.free_clst;
    if (fatfs) *fatfs = &dummy_fs;
    return FR_OK;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)path; (void)opt;
    return FR_OK; /* Always mounted on Linux */
}

/* ---- Printf/puts ---- */

int f_printf(FIL* fp, const TCHAR* fmt, ...) {
    /* Narrow the format string for vsnprintf */
    std::string cfmt = tchar_to_str(fmt);
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), cfmt.c_str(), args);
    va_end(args);

    if (n > 0 && fp->fd >= 0) {
        UINT bw;
        f_write(fp, buf, (UINT)n, &bw);
        return (int)bw;
    }
    return -1;
}

int f_putc(TCHAR c, FIL* fp) {
    if (fp->fd < 0) return -1;
    char ch = static_cast<char>(c & 0xFF);
    UINT bw;
    f_write(fp, &ch, 1, &bw);
    return (bw == 1) ? (int)c : -1;
}

TCHAR* f_gets(TCHAR* buff, int len, FIL* fp) {
    if (!buff || len <= 0 || fp->fd < 0) return nullptr;
    int i = 0;
    while (i < len - 1) {
        char ch;
        ssize_t n = read(fp->fd, &ch, 1);
        if (n <= 0) break;
        fp->fptr++;
        buff[i++] = static_cast<TCHAR>(static_cast<unsigned char>(ch));
        if (ch == '\n') break;
    }
    if (i == 0) return nullptr;
    buff[i] = 0;
    return buff;
}

int f_puts(const TCHAR* str, FIL* fp) {
    std::string s = tchar_to_str(str);
    UINT bw;
    UINT len = (UINT)s.size();
    f_write(fp, s.c_str(), len, &bw);
    return (int)bw;
}

/* ---- Low-level disk I/O stubs ---- */
/* These are only used by sd_wipe (destructive disk write).
 * On Linux, raw disk I/O is a no-op — we don't expose raw sectors. */

#include "diskio.h"

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    (void)pdrv; (void)buff; (void)sector; (void)count;
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    (void)pdrv; (void)buff; (void)sector; (void)count;
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    (void)pdrv; (void)cmd; (void)buff;
    return RES_OK;
}
