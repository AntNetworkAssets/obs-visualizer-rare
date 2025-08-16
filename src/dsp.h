#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct band_meter {
	float level;
	float peak;
} band_meter_t;

typedef struct filter_state {
	float b0,b1,b2,a1,a2;
	float z1,z2;
} filter_state_t;

typedef struct band_filter {
	filter_state_t lp; // low-pass for envelope
	filter_state_t bp; // band-pass biquad
	float env_decay;
} band_filter_t;

void biquad_reset(filter_state_t* s);
float biquad_process(filter_state_t* s, float b0,float b1,float b2,float a1,float a2, float x);
void band_init(band_filter_t* b, float fs, float f_center, float q);
float band_process(band_filter_t* b, float sample);
void compute_bands(const float* interleaved, size_t frames, size_t channels,
                   float sample_rate, band_filter_t* bands, size_t band_count,
                   band_meter_t* out);

#ifdef __cplusplus
}
#endif
