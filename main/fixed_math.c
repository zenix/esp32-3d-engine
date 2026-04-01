#include "fixed_math.h"
#include <math.h>

fp_t sin_lut[256];

// Float is only used here, once at boot, to populate the LUT.
// All runtime engine math uses the integer LUT exclusively.
void fp_lut_init(void)
{
    for (int i = 0; i < 256; i++) {
        sin_lut[i] = (fp_t)(sinf(i * 2.0f * 3.14159265358979f / 256.0f) * (float)FP_ONE);
    }
}
