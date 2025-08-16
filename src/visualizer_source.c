#include <obs-module.h>
#include <graphics/graphics.h>
#include <util/circlebuf.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <math.h>
#include "dsp.h"

#define BANDS_MAX 96
#define BANDS_DEF 64

struct viz_data {
	obs_source_t*                     self;
	obs_source_t*                     audio_src;   // selected audio source to listen to
	char*                             audio_src_name;

	// Audio capture ring buffer (float interleaved)
	circlebuf                         audio_rb;
	size_t                            channels;
	uint32_t                          sample_rate;

	// DSP
	size_t                            band_count;
	band_filter_t*                    bands;
	band_meter_t*                     meters;

	// UI
	uint32_t                          color_primary;
	uint32_t                          color_secondary;
	int                               mirror;
	float                             gap_px;
	float                             smoothing;   // not used (envelope decay covers most)
	float                             glow;        // 0..1 visual
	float                             gravity;     // visual fall
};

static const char* viz_get_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return "Desktop Audio Visualizer (Source)";
}

static void audio_capture_cb(void* vptr, obs_source_t* src, const struct audio_data* data, bool muted)
{
	UNUSED_PARAMETER(src);
	if(muted) return;
	struct viz_data* v = (struct viz_data*)vptr;
	if(!data || !data->frames) return;
	if(!v->sample_rate) v->sample_rate = data->samples_per_sec;
	if(!v->channels) v->channels = data->speakers == SPEAKERS_STEREO ? 2 : 2;

	// Interleave float32 into ring buffer
	size_t frames = data->frames;
	size_t ch = (size_t)v->channels;
	size_t total = frames * ch * sizeof(float);
	// If incoming audio is planar, convert to interleaved float
	float* tmp = (float*)bmalloc(total);
	for(size_t i=0;i<frames;i++){
		for(size_t c=0;c<ch;c++){
			float s = 0.f;
			if(data->data[c])
				s = ((float*)data->data[c])[i];
			tmp[i*ch + c] = s;
		}
	}
	circlebuf_push_back(&v->audio_rb, tmp, total);
	bfree(tmp);
	// keep <= 200 ms
	size_t max_bytes = (size_t)(0.2 * v->sample_rate) * ch * sizeof(float);
	if(v->audio_rb.size > max_bytes){
		size_t drop = v->audio_rb.size - max_bytes;
		circlebuf_pop_front(&v->audio_rb, NULL, drop);
	}
}

static void reconnect_audio(struct viz_data* v)
{
	if(v->audio_src){
		obs_source_remove_audio_capture_callback(v->audio_src, audio_capture_cb, v);
		obs_source_release(v->audio_src);
		v->audio_src = NULL;
	}
	if(v->audio_src_name && *v->audio_src_name){
		obs_source_t* src = obs_get_source_by_name(v->audio_src_name);
		if(src){
			v->audio_src = src;
			obs_source_add_audio_capture_callback(v->audio_src, audio_capture_cb, v);
		}
	}
}

static void* viz_create(obs_data_t* settings, obs_source_t* source)
{
	struct viz_data* v = bzalloc(sizeof(struct viz_data));
	v->self = source;
	circlebuf_init(&v->audio_rb);
	v->band_count = (size_t)obs_data_get_int(settings, "bands");
	if(v->band_count < 12) v->band_count = 12;
	if(v->band_count > BANDS_MAX) v->band_count = BANDS_MAX;

	v->bands = bzalloc(sizeof(band_filter_t) * v->band_count);
	v->meters = bzalloc(sizeof(band_meter_t) * v->band_count);
	for(size_t i=0;i<v->band_count;i++){
		v->meters[i].level = 0.f;
		v->meters[i].peak = 0.f;
	}

	v->color_primary = (uint32_t)obs_data_get_int(settings, "color_primary");
	v->color_secondary = (uint32_t)obs_data_get_int(settings, "color_secondary");
	v->mirror = obs_data_get_bool(settings, "mirror");
	v->gap_px = (float)obs_data_get_double(settings, "gap_px");
	v->glow = (float)obs_data_get_double(settings, "glow");
	v->gravity = (float)obs_data_get_double(settings, "gravity");

	const char* srcname = obs_data_get_string(settings, "audio_source");
	if(srcname) v->audio_src_name = bstrdup(srcname);
	reconnect_audio(v);
	return v;
}

static void viz_destroy(void* vptr)
{
	struct viz_data* v = (struct viz_data*)vptr;
	if(v->audio_src){
		obs_source_remove_audio_capture_callback(v->audio_src, audio_capture_cb, v);
		obs_source_release(v->audio_src);
	}
	bfree(v->audio_src_name);
	circlebuf_free(&v->audio_rb);
	bfree(v->bands);
	bfree(v->meters);
	bfree(v);
}

static void viz_update(void* vptr, obs_data_t* settings)
{
	struct viz_data* v = (struct viz_data*)vptr;
	size_t new_bands = (size_t)obs_data_get_int(settings, "bands");
	if(new_bands != v->band_count && new_bands >= 12 && new_bands <= BANDS_MAX){
		bfree(v->bands);
		bfree(v->meters);
		v->band_count = new_bands;
		v->bands = bzalloc(sizeof(band_filter_t) * v->band_count);
		v->meters = bzalloc(sizeof(band_meter_t) * v->band_count);
	}

	v->color_primary = (uint32_t)obs_data_get_int(settings, "color_primary");
	v->color_secondary = (uint32_t)obs_data_get_int(settings, "color_secondary");
	v->mirror = obs_data_get_bool(settings, "mirror");
	v->gap_px = (float)obs_data_get_double(settings, "gap_px");
	v->glow = (float)obs_data_get_double(settings, "glow");
	v->gravity = (float)obs_data_get_double(settings, "gravity");

	const char* srcname = obs_data_get_string(settings, "audio_source");
	if((!v->audio_src_name && srcname) || (v->audio_src_name && strcmp(v->audio_src_name, srcname)!=0)){
		bfree(v->audio_src_name);
		v->audio_src_name = bstrdup(srcname ? srcname : "");
		reconnect_audio(v);
	}
}

static obs_properties_t* viz_properties(void* vptr)
{
	UNUSED_PARAMETER(vptr);
	obs_properties_t* props = obs_properties_create();

	// Audio source dropdown
	obs_property_t* list = obs_properties_add_list(props, "audio_source", "Audio Source",
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	// enumerate sources
	obs_source_t* src = NULL;
	obs_enum_sources([](void* data, obs_source_t* s){
		obs_property_t* list = (obs_property_t*)data;
		enum obs_source_type type = obs_source_get_type(s);
		if(type == OBS_SOURCE_TYPE_INPUT || type == OBS_SOURCE_TYPE_FILTER){
			// Only list those with audio output
			uint32_t caps = obs_source_get_output_flags(s);
			if(caps & OBS_SOURCE_AUDIO){
				const char* name = obs_source_get_name(s);
				obs_property_list_add_string(list, name, name);
			}
		}
		return true;
	}, list);

	obs_properties_add_int(props, "bands", "Bars", 12, BANDS_MAX, 1);
	obs_properties_add_bool(props, "mirror", "Mirror");
	obs_properties_add_float(props, "gap_px", "Gap (px)", 0.0, 20.0, 0.5);
	obs_properties_add_float(props, "glow", "Glow", 0.0, 1.0, 0.01);
	obs_properties_add_float(props, "gravity", "Gravity", 0.0, 1.0, 0.01);
	obs_properties_add_color(props, "color_primary", "Primary Color");
	obs_properties_add_color(props, "color_secondary", "Secondary Color");
	return props;
}

static void viz_defaults(obs_data_t* settings)
{
	obs_data_set_default_int(settings, "bands", BANDS_DEF);
	obs_data_set_default_bool(settings, "mirror", true);
	obs_data_set_default_double(settings, "gap_px", 3.0);
	obs_data_set_default_double(settings, "glow", 0.55);
	obs_data_set_default_double(settings, "gravity", 0.22);
	obs_data_set_default_int(settings, "color_primary", 0xFF1e90ff);
	obs_data_set_default_int(settings, "color_secondary", 0xFF10b981);
	obs_data_set_default_string(settings, "audio_source", "Desktop Audio");
}

static void viz_tick(void* vptr, float seconds)
{
	struct viz_data* v = (struct viz_data*)vptr;
	UNUSED_PARAMETER(seconds);
	// initialize band filters when sample_rate known
	if(v->sample_rate && v->bands && v->bands[0].lp.b0 == 0.f){
		// setup bands logarithmically from 60 Hz to 16 kHz
		float fmin = 60.f, fmax = 16000.f;
		for(size_t i=0;i<v->band_count;i++){
			float t = (float)i / (float)(v->band_count-1);
			float f = fmin * powf(fmax/fmin, t);
			band_init(&v->bands[i], (float)v->sample_rate, f, 2.0f);
		}
	}
	// drain samples and compute meters
	if(v->audio_rb.size){
		size_t bytes = v->audio_rb.size;
		uint8_t* tmp = (uint8_t*)bmalloc(bytes);
		circlebuf_pop_front(&v->audio_rb, tmp, bytes);
		compute_bands((const float*)tmp, bytes/(sizeof(float)*v->channels), v->channels,
			(float)v->sample_rate, v->bands, v->band_count, v->meters);
		bfree(tmp);
	}
	// decay gravity visual (peak fall)
	for(size_t i=0;i<v->band_count;i++){
		v->meters[i].peak *= (1.0f - v->gravity*0.5f);
		if(v->meters[i].peak < v->meters[i].level) v->meters[i].peak = v->meters[i].level;
	}
}

static void viz_render(void* vptr, gs_effect_t* effect)
{
	UNUSED_PARAMETER(effect);
	struct viz_data* v = (struct viz_data*)vptr;
	if(!v->band_count) return;

	uint32_t w = obs_source_get_base_width(v->self);
	uint32_t h = obs_source_get_base_height(v->self);
	if(w==0 || h==0) return;

	gs_matrix_push();
	gs_reset_blend_state();
	gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f);

	float total_bars = (float)v->band_count * (v->mirror ? 2.0f : 1.0f);
	float total_gap = (total_bars - 1.0f) * v->gap_px;
	float usable = (float)w * 0.86f - total_gap;
	float bar_w = usable / total_bars;
	if(bar_w < 2.f) bar_w = 2.f;
	float start_x = ((float)w - (usable + total_gap)) * 0.5f;
	float base_y = (float)h * 0.32f;

	for(size_t i=0;i<v->band_count;i++){
		float t = (float)i / (float)(v->band_count-1);
		uint8_t pr = (uint8_t)(( (v->color_primary>>16)&0xFF )*(1.0f-t) + ((v->color_secondary>>16)&0xFF)*t);
		uint8_t pg = (uint8_t)(( (v->color_primary>>8 )&0xFF )*(1.0f-t) + ((v->color_secondary>>8 )&0xFF)*t);
		uint8_t pb = (uint8_t)(( (v->color_primary    )&0xFF )*(1.0f-t) + ((v->color_secondary    )&0xFF)*t);
		gs_effect_t* col = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t* color = gs_effect_get_param_by_name(col, "color");
		vec4 c; vec4_from_rgba(&c, pr,pg,pb,255);
		gs_effect_set_vec4(color, &c);

		float level = v->meters[i].level;
		if(level > 1.0f) level = 1.0f;
		float height = powf(level, 1.2f) * (float)h * 0.42f;

		float x = start_x + (float)i * (bar_w + v->gap_px);
		gs_matrix_push();
		gs_matrix_translate3f(x, base_y, 0.0f);
		gs_matrix_scale3f(bar_w, height, 1.0f);
		while (gs_effect_loop(col, "Solid")) {
			gs_draw_sprite(NULL, 0, 1, 1);
		}
		gs_matrix_pop();

		if(v->mirror){
			float xr = (float)w - (x + bar_w);
			gs_matrix_push();
			gs_matrix_translate3f(xr, base_y, 0.0f);
			gs_matrix_scale3f(bar_w, height, 1.0f);
			while (gs_effect_loop(col, "Solid")) {
				gs_draw_sprite(NULL, 0, 1, 1);
			}
			gs_matrix_pop();
		}
	}

	gs_matrix_pop();
}

static uint32_t viz_width(void* vptr)
{
	UNUSED_PARAMETER(vptr);
	return 1920;
}
static uint32_t viz_height(void* vptr)
{
	UNUSED_PARAMETER(vptr);
	return 1080;
}

struct obs_source_info viz_source_info = {
	.id = "desktop_audio_visualizer_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = viz_get_name,
	.create = viz_create,
	.destroy = viz_destroy,
	.update = viz_update,
	.get_width = viz_width,
	.get_height = viz_height,
	.video_render = viz_render,
	.video_tick = viz_tick,
	.get_properties = viz_properties,
	.get_defaults = viz_defaults,
};
