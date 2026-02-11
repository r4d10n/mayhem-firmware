/*
 * CMSIS core_cm4_simd.h Override for Linux Shim
 *
 * This file intercepts the ARM CMSIS core_cm4_simd.h header via -I ordering.
 * The real header provides ARM Cortex-M4 SIMD intrinsics — we redirect to
 * our scalar C++ implementations for x86_64.
 */

#pragma once

#include "arm_intrinsics_shim.h"
