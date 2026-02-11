/*
 * ARM Cortex-M4 SIMD Intrinsics Shim for x86_64
 *
 * Pure C++ scalar implementations of ARM DSP intrinsics.
 * Performance is not critical - x86_64 at 3-5 GHz has 15-25x the clock speed
 * of the 204 MHz Cortex-M4.
 */

#pragma once

#ifdef LINUX_SHIM

#include <cstdint>
#include <algorithm>
#include <climits>

// ============================================================================
// SATURATION INSTRUCTIONS
// ============================================================================

/**
 * Signed saturate to N bits
 * @param val Value to saturate
 * @param bits Total bit width including sign bit (1 to 32)
 * @return Saturated value in range [-(1<<(bits-1)), (1<<(bits-1))-1]
 */
static inline int32_t __SSAT(int32_t val, uint32_t bits) {
    if (bits == 0 || bits > 32) return val;
    const int32_t max = (1 << (bits - 1)) - 1;
    const int32_t min = -(1 << (bits - 1));
    if (val > max) return max;
    if (val < min) return min;
    return val;
}

/**
 * Unsigned saturate to N bits
 * @param val Value to saturate (treated as signed input)
 * @param bits Bit width (0 to 31)
 * @return Saturated value in range [0, (1<<bits)-1]
 */
static inline uint32_t __USAT(int32_t val, uint32_t bits) {
    if (bits >= 32) return (val < 0) ? 0 : (uint32_t)val;
    const uint32_t max = (1U << bits) - 1;
    if (val < 0) return 0;
    if ((uint32_t)val > max) return max;
    return (uint32_t)val;
}

/**
 * Saturating 32-bit add
 */
static inline int32_t __QADD(int32_t a, int32_t b) {
    int64_t result = (int64_t)a + (int64_t)b;
    if (result > INT32_MAX) return INT32_MAX;
    if (result < INT32_MIN) return INT32_MIN;
    return (int32_t)result;
}

/**
 * Saturating 32-bit subtract
 */
static inline int32_t __QSUB(int32_t a, int32_t b) {
    int64_t result = (int64_t)a - (int64_t)b;
    if (result > INT32_MAX) return INT32_MAX;
    if (result < INT32_MIN) return INT32_MIN;
    return (int32_t)result;
}

/**
 * Parallel saturating 16-bit add (low and high halfwords independently)
 */
static inline uint32_t __QADD16(uint32_t a, uint32_t b) {
    int16_t a_lo = (int16_t)(a & 0xFFFF);
    int16_t a_hi = (int16_t)(a >> 16);
    int16_t b_lo = (int16_t)(b & 0xFFFF);
    int16_t b_hi = (int16_t)(b >> 16);

    int32_t sum_lo = (int32_t)a_lo + (int32_t)b_lo;
    int32_t sum_hi = (int32_t)a_hi + (int32_t)b_hi;

    if (sum_lo > 32767) sum_lo = 32767;
    if (sum_lo < -32768) sum_lo = -32768;
    if (sum_hi > 32767) sum_hi = 32767;
    if (sum_hi < -32768) sum_hi = -32768;

    return ((uint32_t)(uint16_t)sum_lo) | ((uint32_t)(uint16_t)sum_hi << 16);
}

/**
 * Parallel saturating 16-bit subtract (low and high halfwords independently)
 */
static inline uint32_t __QSUB16(uint32_t a, uint32_t b) {
    int16_t a_lo = (int16_t)(a & 0xFFFF);
    int16_t a_hi = (int16_t)(a >> 16);
    int16_t b_lo = (int16_t)(b & 0xFFFF);
    int16_t b_hi = (int16_t)(b >> 16);

    int32_t diff_lo = (int32_t)a_lo - (int32_t)b_lo;
    int32_t diff_hi = (int32_t)a_hi - (int32_t)b_hi;

    if (diff_lo > 32767) diff_lo = 32767;
    if (diff_lo < -32768) diff_lo = -32768;
    if (diff_hi > 32767) diff_hi = 32767;
    if (diff_hi < -32768) diff_hi = -32768;

    return ((uint32_t)(uint16_t)diff_lo) | ((uint32_t)(uint16_t)diff_hi << 16);
}

// ============================================================================
// MULTIPLY-ACCUMULATE INSTRUCTIONS (CRITICAL FOR DSP)
// ============================================================================

/**
 * Dual 16-bit multiply-add (no accumulator)
 * Returns: lo(x)*lo(y) + hi(x)*hi(y)
 */
static inline int32_t __SMUAD(uint32_t x, uint32_t y) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return (int32_t)x_lo * (int32_t)y_lo + (int32_t)x_hi * (int32_t)y_hi;
}

/**
 * Dual 16-bit multiply-add cross variant (no accumulator)
 * Returns: lo(x)*hi(y) + hi(x)*lo(y)
 */
static inline int32_t __SMUADX(uint32_t x, uint32_t y) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return (int32_t)x_lo * (int32_t)y_hi + (int32_t)x_hi * (int32_t)y_lo;
}

/**
 * Dual 16-bit multiply-add with 32-bit accumulator
 * Returns: lo(x)*lo(y) + hi(x)*hi(y) + acc
 */
static inline int32_t __SMLAD(uint32_t x, uint32_t y, int32_t acc) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return (int32_t)x_lo * (int32_t)y_lo + (int32_t)x_hi * (int32_t)y_hi + acc;
}

/**
 * Dual 16-bit multiply-add cross variant with 32-bit accumulator
 * Returns: lo(x)*hi(y) + hi(x)*lo(y) + acc
 */
static inline int32_t __SMLADX(uint32_t x, uint32_t y, int32_t acc) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return (int32_t)x_lo * (int32_t)y_hi + (int32_t)x_hi * (int32_t)y_lo + acc;
}

/**
 * Dual 16-bit multiply-subtract (no accumulator)
 * Returns: lo(x)*lo(y) - hi(x)*hi(y)
 */
static inline int32_t __SMUSD(uint32_t x, uint32_t y) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);
    return (int32_t)x_lo * (int32_t)y_lo - (int32_t)x_hi * (int32_t)y_hi;
}

/**
 * Dual 16-bit multiply-subtract cross variant (no accumulator)
 * Returns: lo(x)*hi(y) - hi(x)*lo(y)
 */
static inline int32_t __SMUSDX(uint32_t x, uint32_t y) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);
    return (int32_t)x_lo * (int32_t)y_hi - (int32_t)x_hi * (int32_t)y_lo;
}

/**
 * Dual 16-bit multiply-subtract with 32-bit accumulator
 * Returns: lo(x)*lo(y) - hi(x)*hi(y) + acc
 */
static inline int32_t __SMLSD(uint32_t x, uint32_t y, int32_t acc) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return (int32_t)x_lo * (int32_t)y_lo - (int32_t)x_hi * (int32_t)y_hi + acc;
}

/**
 * Dual 16-bit multiply-add with 64-bit accumulator
 * Returns: acc64 + lo(x)*lo(y) + hi(x)*hi(y)
 */
static inline int64_t __SMLALD(uint32_t x, uint32_t y, int64_t acc64) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return acc64 + (int64_t)((int32_t)x_lo * (int32_t)y_lo) +
                   (int64_t)((int32_t)x_hi * (int32_t)y_hi);
}

/**
 * Dual 16-bit multiply-add cross variant with 64-bit accumulator
 * Returns: acc64 + lo(x)*hi(y) + hi(x)*lo(y)
 */
static inline int64_t __SMLALDX(uint32_t x, uint32_t y, int64_t acc64) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);

    return acc64 + (int64_t)((int32_t)x_lo * (int32_t)y_hi) +
                   (int64_t)((int32_t)x_hi * (int32_t)y_lo);
}

/**
 * Dual 16-bit multiply-subtract with 64-bit accumulator
 * Returns: acc64 + lo(x)*lo(y) - hi(x)*hi(y)
 */
static inline int64_t __SMLSLD(uint32_t x, uint32_t y, int64_t acc64) {
    int16_t x_lo = (int16_t)(x & 0xFFFF);
    int16_t x_hi = (int16_t)(x >> 16);
    int16_t y_lo = (int16_t)(y & 0xFFFF);
    int16_t y_hi = (int16_t)(y >> 16);
    return acc64 + (int64_t)((int32_t)x_lo * (int32_t)y_lo) -
                   (int64_t)((int32_t)x_hi * (int32_t)y_hi);
}

// ============================================================================
// 16-BIT MULTIPLY INSTRUCTIONS (bottom/top halfword selection)
// ============================================================================

/** Multiply bottom halves: (int16_t)x * (int16_t)y */
static inline int32_t __SMULBB(uint32_t x, uint32_t y) {
    return (int32_t)(int16_t)(x & 0xFFFF) * (int32_t)(int16_t)(y & 0xFFFF);
}

/** Multiply bottom x, top y: (int16_t)x * (int16_t)(y>>16) */
static inline int32_t __SMULBT(uint32_t x, uint32_t y) {
    return (int32_t)(int16_t)(x & 0xFFFF) * (int32_t)(int16_t)(y >> 16);
}

/** Multiply top x, bottom y: (int16_t)(x>>16) * (int16_t)y */
static inline int32_t __SMULTB(uint32_t x, uint32_t y) {
    return (int32_t)(int16_t)(x >> 16) * (int32_t)(int16_t)(y & 0xFFFF);
}

/** Multiply top halves: (int16_t)(x>>16) * (int16_t)(y>>16) */
static inline int32_t __SMULTT(uint32_t x, uint32_t y) {
    return (int32_t)(int16_t)(x >> 16) * (int32_t)(int16_t)(y >> 16);
}

/** Multiply bottom×bottom + accumulate */
static inline int32_t __SMLABB(uint32_t x, uint32_t y, int32_t acc) {
    return (int32_t)(int16_t)(x & 0xFFFF) * (int32_t)(int16_t)(y & 0xFFFF) + acc;
}

/** Multiply bottom×top + accumulate */
static inline int32_t __SMLABT(uint32_t x, uint32_t y, int32_t acc) {
    return (int32_t)(int16_t)(x & 0xFFFF) * (int32_t)(int16_t)(y >> 16) + acc;
}

/** Multiply top×bottom + accumulate */
static inline int32_t __SMLATB(uint32_t x, uint32_t y, int32_t acc) {
    return (int32_t)(int16_t)(x >> 16) * (int32_t)(int16_t)(y & 0xFFFF) + acc;
}

/** Multiply top×top + accumulate */
static inline int32_t __SMLATT(uint32_t x, uint32_t y, int32_t acc) {
    return (int32_t)(int16_t)(x >> 16) * (int32_t)(int16_t)(y >> 16) + acc;
}

// ============================================================================
// SIGN EXTEND AND ADD
// ============================================================================

/**
 * Sign-extend halfword and add
 * @param rd Base value
 * @param rm Value containing halfword to extend (after rotation)
 * @param rot Rotation in bits (0, 8, 16, 24)
 * @return rd + sign_extend((rm ROR rot)[15:0])
 */
static inline int32_t __SXTAH(int32_t rd, uint32_t rm, uint32_t rot) {
    uint32_t rotated = rm;
    if (rot != 0 && rot < 32) {
        rotated = (rm >> rot) | (rm << (32 - rot));
    }
    return rd + (int32_t)(int16_t)(rotated & 0xFFFF);
}

// ============================================================================
// BIT FIELD INSTRUCTIONS
// ============================================================================

/**
 * Bit Field Insert: inserts width bits from rn into rd starting at lsb
 */
static inline uint32_t __BFI(uint32_t rd, uint32_t rn, uint32_t lsb, uint32_t width) {
    uint32_t mask = ((1U << width) - 1) << lsb;
    return (rd & ~mask) | ((rn << lsb) & mask);
}

/**
 * High-word multiply with rounding
 * Returns: (x * y + 0x80000000) >> 32
 */
static inline int32_t __SMMULR(int32_t x, int32_t y) {
    int64_t product = (int64_t)x * (int64_t)y;
    // Add rounding constant 0x80000000
    product += 0x80000000LL;
    // Return upper 32 bits
    return (int32_t)(product >> 32);
}

// ============================================================================
// PACKING INSTRUCTIONS
// ============================================================================

/**
 * Pack halfwords bottom-top
 * Takes bottom halfword from x, top halfword from (y << sh)
 * @param x Source for bits[15:0]
 * @param y Source for bits[31:16] after left shift
 * @param sh Left shift amount for y (0-31)
 */
static inline uint32_t __PKHBT(uint32_t x, uint32_t y, uint32_t sh) {
    if (sh == 0) {
        return (x & 0xFFFF) | (y & 0xFFFF0000);
    }
    return (x & 0xFFFF) | ((y << sh) & 0xFFFF0000);
}

/**
 * Pack halfwords top-bottom
 * Takes top halfword from x, bottom halfword from (y >> sh) with arithmetic shift
 * @param x Source for bits[31:16]
 * @param y Source for bits[15:0] after arithmetic right shift
 * @param sh Right shift amount for y (0-31)
 */
static inline uint32_t __PKHTB(uint32_t x, uint32_t y, uint32_t sh) {
    if (sh == 0) {
        return (x & 0xFFFF0000) | (y & 0xFFFF);
    }
    // Arithmetic shift: sign-extend from bit 31
    int32_t y_signed = (int32_t)y;
    int32_t shifted = y_signed >> sh;
    return (x & 0xFFFF0000) | ((uint32_t)shifted & 0xFFFF);
}

/**
 * Sign-extend two bytes to halfwords
 * Extracts bytes at positions [7:0] and [23:16] after rotating, sign-extends each to 16 bits
 * @param x Source value
 * @param rot Rotation amount in bits (0, 8, 16, 24)
 * @return byte0 sign-extended in [15:0], byte2 sign-extended in [31:16]
 */
static inline uint32_t __SXTB16(uint32_t x, uint32_t rot) {
    uint32_t rotated = x;
    if (rot != 0 && rot < 32) {
        rotated = (x >> rot) | (x << (32 - rot));
    }

    int8_t byte0 = (int8_t)(rotated & 0xFF);
    int8_t byte2 = (int8_t)((rotated >> 16) & 0xFF);

    uint16_t ext0 = (uint16_t)(int16_t)byte0;
    uint16_t ext2 = (uint16_t)(int16_t)byte2;

    return ((uint32_t)ext0) | ((uint32_t)ext2 << 16);
}

/**
 * Sign-extend halfword
 * @param x Source value
 * @param rot Rotation amount in bits (0, 8, 16, 24)
 * @return Sign-extended halfword
 */
static inline int32_t __SXTH(uint32_t x, uint32_t rot) {
    uint32_t rotated = x;
    if (rot != 0 && rot < 32) {
        rotated = (x >> rot) | (x << (32 - rot));
    }
    return (int32_t)(int16_t)(rotated & 0xFFFF);
}

// ============================================================================
// BIT MANIPULATION
// ============================================================================

/**
 * Reverse all 32 bits
 * Used for FFT bit-reversal
 */
static inline uint32_t __RBIT(uint32_t x) {
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        if (x & (1U << i)) {
            result |= 1U << (31 - i);
        }
    }
    return result;
}

/**
 * Byte-swap within each halfword
 * Swaps bytes [7:0] with [15:8], and [23:16] with [31:24]
 */
static inline uint32_t __REV16(uint32_t x) {
    return ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
}

/**
 * Count leading zeros
 * Returns number of leading zero bits (0-32)
 */
static inline uint32_t __CLZ(uint32_t x) {
    if (x == 0) return 32;
    return __builtin_clz(x);
}

// ============================================================================
// MEMORY ACCESS
// ============================================================================

/**
 * SIMD 32-bit pointer cast — must produce an lvalue for *__SIMD32(p)++ patterns.
 * Matches ARM CMSIS: #define __SIMD32(addr) (*(__SIMD32_TYPE**)&(addr))
 */
#define __SIMD32_TYPE  int32_t
#define __SIMD32(addr) (*(int32_t**)&(addr))

#endif /* LINUX_SHIM */
