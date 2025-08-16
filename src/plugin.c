#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("desktop_audio_visualizer", "en-US")

extern struct obs_source_info viz_source_info;

bool obs_module_load(void)
{
	obs_register_source(&viz_source_info);
	blog(LOG_INFO, "[DesktopAudioVisualizer] loaded");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[DesktopAudioVisualizer] unloaded");
}
