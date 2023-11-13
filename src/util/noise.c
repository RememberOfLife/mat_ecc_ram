#include <stdint.h>

#include "util/noise.h"

uint32_t squirrelnoise5(uint32_t positionX, uint32_t seed)
{
    /* squirrelnoise5 by Squirrel Eiserloh (SquirrelEiserloh at gmail.com)
    SquirrelNoise5 - Squirrel's Raw Noise utilities (version 5)

    This code is made available under the Creative Commons attribution 3.0 license (CC-BY-3.0 US):
    Attribution in source code comments (even closed-source/commercial code) is sufficient.
    License summary and text available at: https://creativecommons.org/licenses/by/3.0/us/
    */
    const uint32_t SQ7_BIT_NOISE1 = 0xd2a80a3f; // 11010010101010000000101000111111
    const uint32_t SQ5_BIT_NOISE2 = 0xa884f197; // 10101000100001001111000110010111
    const uint32_t SQ5_BIT_NOISE3 = 0x6C736F4B; // 01101100011100110110111101001011
    const uint32_t SQ5_BIT_NOISE4 = 0xB79F3ABB; // 10110111100111110011101010111011
    const uint32_t SQ5_BIT_NOISE5 = 0x1b56c4f5; // 00011011010101101100010011110101
    uint32_t mangledBits = positionX;
    mangledBits *= SQ7_BIT_NOISE1;
    mangledBits += seed;
    mangledBits ^= (mangledBits >> 9);
    mangledBits += SQ5_BIT_NOISE2;
    mangledBits ^= (mangledBits >> 11);
    mangledBits *= SQ5_BIT_NOISE3;
    mangledBits ^= (mangledBits >> 13);
    mangledBits += SQ5_BIT_NOISE4;
    mangledBits ^= (mangledBits >> 15);
    mangledBits *= SQ5_BIT_NOISE5;
    mangledBits ^= (mangledBits >> 17);
    return mangledBits;
}

uint64_t squirrelnoise5_u64(uint64_t position, uint64_t seed)
{
    // because squirrelnoise depends heavily on a good choice of noise primes, this is NOT a propper 64 bit equivalent
    uint32_t p_fold = (position >> 32) ^ position;
    uint32_t s_fold = (seed >> 32) ^ seed;
    uint64_t n_low = squirrelnoise5(p_fold, s_fold);
    uint64_t n_high = squirrelnoise5(~p_fold, ~s_fold);
    return (n_high << 32) | n_low;
}

uint32_t noise_get_uintn(int32_t position, uint32_t seed, uint32_t max_n)
{
    // https://funloop.org/post/2015-02-27-removing-modulo-bias-redux.html
    uint32_t r;
    uint32_t threshold = -max_n % max_n;
    do {
        r = squirrelnoise5(position, seed);
    } while (r < threshold);
    return r % max_n;
}

uint64_t noise_get_u64n(uint64_t position, uint64_t seed, uint64_t max_n)
{
    // https://funloop.org/post/2015-02-27-removing-modulo-bias-redux.html
    uint64_t r;
    uint64_t threshold = -max_n % max_n;
    do {
        r = squirrelnoise5_u64(position, seed);
    } while (r < threshold);
    return r % max_n;
}

float noise_get_f32_zto(uint32_t index, uint32_t seed)
{
    const double ONE_OVER_MAX_UINT = (1.0 / (double)0xFFFFFFFF);
    return (float)(ONE_OVER_MAX_UINT * (double)squirrelnoise5(index, seed));
}

double noise_get_f64_zto(uint64_t index, uint64_t seed)
{
    // https://stackoverflow.com/a/51883387
    // the 0x3FF sets the exponent to the 0..1 range.
    uint64_t vv = (squirrelnoise5_u64(index, seed) >> 11) | (0x3FFL << 53);
    return *(double*)&vv;
}

float noise_get_f32_xty(uint32_t index, uint32_t seed, float x, float y)
{
    return x + noise_get_f32_zto(index, seed) * (y - x);
}
