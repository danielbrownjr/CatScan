#include "adc_math.h"
#include <math.h>
_Static_assert(CATSCAN_DATA_RATE == 128, "Update register encoding and scan budget before changing data rate");
static const double ranges[] = {6.144, 4.096, 2.048, 1.024, 0.512, 0.256};
bool catscan_gain_valid(unsigned gain) { return gain < 6; }
double catscan_full_scale(unsigned gain) { return catscan_gain_valid(gain) ? ranges[gain] : NAN; }
double catscan_voltage(int16_t raw, unsigned gain) { return raw * catscan_full_scale(gain) / 32768.0; }
int16_t catscan_decode(uint16_t bits) {
    return (int16_t)(bits <= 32767 ? (int32_t)bits : (int32_t)bits - 65536);
}
bool catscan_conversion_config(unsigned channel, unsigned gain, uint16_t *reg) {
    if (!reg || channel >= CATSCAN_CHANNELS || !catscan_gain_valid(gain)) return false;
    /* OS=start, MUX=AINx/GND, MODE=single-shot, DR=128, comparator disabled. */
    *reg = (uint16_t)(0x8183u | ((4u + channel) << 12) | (gain << 9));
    return true;
}
