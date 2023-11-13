#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t squirrelnoise5(uint32_t positionX, uint32_t seed);

uint64_t squirrelnoise5_u64(uint64_t positionX, uint64_t seed);

uint32_t noise_get_u32n(uint32_t position, uint32_t seed, uint32_t max_n);

uint64_t noise_get_u64n(uint64_t position, uint64_t seed, uint64_t max_n);

float noise_get_f32_zto(uint32_t index, uint32_t seed);

double noise_get_f64_zto(uint64_t index, uint64_t seed);

float noise_get_f32_xty(uint32_t index, uint32_t seed, float x, float y);

#ifdef __cplusplus
}
#endif
