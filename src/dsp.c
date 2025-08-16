#include "dsp.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void biquad_reset(filter_state_t* s){ s->z1=0.f; s->z2=0.f; }

static void design_biquad_bandpass(float fs, float f0, float Q,
                                   float* b0,float* b1,float* b2,float* a1,float* a2)
{
	const float w0 = 2.f*(float)M_PI*f0/fs;
	const float alpha = sinf(w0)/(2.f*Q);
	const float cosw0 = cosf(w0);

	const float b0n =   alpha;
	const float b1n =   0.f;
	const float b2n =  -alpha;
	const float a0  =   1.f + alpha;
	const float a1n =  -2.f*cosw0;
	const float a2n =   1.f - alpha;

	*b0 = b0n / a0;
	*b1 = b1n / a0;
	*b2 = b2n / a0;
	*a1 = a1n / a0;
	*a2 = a2n / a0;
}

float biquad_process(filter_state_t* s, float b0,float b1,float b2,float a1,float a2, float x)
{
	const float y = b0*x + s->z1;
	s->z1 = b1*x + s->z2 - a1*y;
	s->z2 = b2*x - a2*y;
	return y;
}

void band_init(band_filter_t* b, float fs, float f_center, float q)
{
	float b0,b1,b2,a1,a2;
	design_biquad_bandpass(fs, f_center, q, &b0,&b1,&b2,&a1,&a2);
	// bake coefficients into bp state
	b->bp.b0 = b0; b->bp.b1 = b1; b->bp.b2 = b2; b->bp.a1 = a1; b->bp.a2 = a2;
	biquad_reset(&b->bp);

	// envelope low-pass ~10 Hz
	const float f_env = 10.f;
	float be0,be1,be2,ae1,ae2;
	design_biquad_bandpass(fs, f_env, 0.5f, &be0,&be1,&be2,&ae1,&ae2);
	// Use simple one-pole LP instead (more stable)
	const float rc = 1.f/(2.f*(float)M_PI*f_env);
	const float dt = 1.f/fs;
	const float alpha = dt/(rc+dt);
	b->lp.b0 = alpha; b->lp.b1 = 0.f; b->lp.b2 = 0.f; b->lp.a1 = (1.f-alpha); b->lp.a2 = 0.f;
	biquad_reset(&b->lp);

	b->env_decay = 0.98f;
}

float band_process(band_filter_t* b, float sample)
{
	// band-pass to isolate
	float y = biquad_process(&b->bp, b->bp.b0,b->bp.b1,b->bp.b2,b->bp.a1,b->bp.a2, sample);
	// rectification
	y = fabsf(y);
	// envelope (one-pole)
	float env = b->lp.z1 + b->lp.b0*(y - b->lp.z1);
	b->lp.z1 = env;
	return env;
}

void compute_bands(const float* interleaved, size_t frames, size_t channels,
                   float sample_rate, band_filter_t* bands, size_t band_count,
                   band_meter_t* out)
{
	(void)sample_rate;
	for(size_t b=0;b<band_count;b++){
		out[b].level *= 0.92f; // decay
	}

	for(size_t i=0;i<frames;i++){
		// mixdown to mono
		float s = 0.f;
		for(size_t c=0;c<channels;c++) s += interleaved[i*channels + c];
		s /= (float)channels;
		for(size_t b=0;b<band_count;b++){
			float env = band_process(&bands[b], s);
			if(env > out[b].level) out[b].level = env;
			if(out[b].level > out[b].peak) out[b].peak = out[b].level;
		}
	}
}
