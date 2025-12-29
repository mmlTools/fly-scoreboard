#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][plugin]"
#include "fly_score_log.hpp"

#include <obs-module.h>
#include <obs.h>

#include <QString>
#include <QDir>

#include "fly_score_paths.hpp"
#include "fly_score_state.hpp"
#include "fly_score_dock.hpp"
#include "fly_score_const.hpp"

OBS_DECLARE_MODULE();
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return PLUGIN_NAME;
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Fly Scoreboard real-time sports/e-sports streaming scoreboard plugin and overlay.";
}

bool obs_module_load(void)
{
	LOGI("Plugin loaded (version %s)", PLUGIN_VERSION);

	fly_create_dock();

	return true;
}

void obs_module_unload(void)
{
	LOGI("Plugin unloading...");

	fly_destroy_dock();
	LOGI("Plugin unloaded");
}
