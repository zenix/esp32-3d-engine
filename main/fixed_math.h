#pragma once
#include <stdint.h>

// Q16.16 fixed-point type: upper 16 bits = integer, lower 16 bits = fraction.
// Range: -32768.0 to +32767.99998, precision: ~0.0000153
typedef int32_t fp_t;

#define FP_SHIFT        16
#define FP_ONE          (1 << FP_SHIFT)

// Convert between plain integer and Q16.16
#define INT_FP(x)       ((fp_t)(x) << FP_SHIFT)
#define FP_INT(x)       ((int32_t)(x) >> FP_SHIFT)

// Arithmetic — add/sub are plain int32 ops (no overhead).
// Mul: use int64 intermediate to avoid overflow, then shift back.
// Div: shift numerator up before dividing to preserve precision.
#define FP_MUL(a, b)    ((fp_t)(((int64_t)(a) * (b)) >> FP_SHIFT))
#define FP_DIV(a, b)    ((fp_t)(((int64_t)(a) << FP_SHIFT) / (b)))

// 256-entry sine LUT (Q16.16). Angle is uint8_t: 0-255 maps to 0-360°.
// Natural uint8_t overflow gives free modulo (256 wraps to 0).
extern fp_t sin_lut[256];

// Must be called once at startup (uses float only during init).
void fp_lut_init(void);

// sin and cos using the LUT. cos(a) = sin(a + 64) since 64/256 = 90°.
static inline fp_t fp_sin(uint8_t a) { return sin_lut[a]; }
static inline fp_t fp_cos(uint8_t a) { return sin_lut[(uint8_t)(a + 64u)]; }
