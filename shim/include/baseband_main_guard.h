/*
 * Baseband main() guard for Linux shim
 *
 * Force-included for all baseband proc_*.cpp sources via CMake.
 * Renames each file's int main() to a weak unused symbol, preventing
 * conflict with the shim's actual main() in main_linux.cpp.
 */

#pragma once

#ifdef LINUX_SHIM
/* Each proc_*.cpp defines int main(). With this macro, it becomes
 * int __attribute__((weak)) baseband_proc_main_unused().
 * The weak attribute allows multiple definitions without linker errors. */
#define main __attribute__((weak)) baseband_proc_main_unused
#endif
