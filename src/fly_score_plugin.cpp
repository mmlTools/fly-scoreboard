#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][plugin]"
#include "fly_score_log.hpp"

#include <obs-module.h>
#include <obs.h>

#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <cstring>
#include <QString>

#include "seed_defaults.hpp"
#include "fly_score_server.hpp"
#include "fly_score_state.hpp"
#include "fly_score_dock.hpp"
#include "fly_score_const.hpp"
#include "fly_score_hotkeys.hpp"

static obs_scene_t *get_current_scene()
{
#ifdef ENABLE_FRONTEND_API
	obs_source_t *cur = obs_frontend_get_current_scene();
	if (!cur)
		return nullptr;

	obs_scene_t *scn = obs_scene_from_source(cur);
	obs_source_release(cur);
	return scn;
#else
	return nullptr;
#endif
}

static void create_or_update_browser_source(const char *url)
{
	obs_scene_t *scn = get_current_scene();
	if (!scn) {
		LOGW("No current scene; skipping browser source ensure.");
		return;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "url", url);
	obs_data_set_int(settings, "width", kBrowserWidth);
	obs_data_set_int(settings, "height", kBrowserHeight);
	obs_data_set_bool(settings, "shutdown", false);
	obs_data_set_bool(settings, "restart_when_active", false);
	obs_data_set_bool(settings, "refresh_browser_when_scene_becomes_active", false);
	obs_data_set_string(settings, "css", "");
	obs_data_set_string(settings, "custom_css", "");

	struct UpdateCtx {
		obs_data_t *settings;
		bool updated;
	};
	UpdateCtx ctx{settings, false};

	obs_scene_enum_items(
		scn,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
			auto *ctx = static_cast<UpdateCtx *>(param);
			obs_source_t *src = obs_sceneitem_get_source(item);
			const char *n = obs_source_get_name(src);
			if (n && std::strcmp(n, kBrowserSourceName) == 0) {
				obs_source_update(src, ctx->settings);
				ctx->updated = true;
				return false;
			}
			return true;
		},
		&ctx);

	if (ctx.updated) {
		LOGI("Updated Browser Source '%s' -> %s (CSS cleared)", kBrowserSourceName, url);
		obs_data_release(settings);
		return;
	}

	obs_source_t *br = obs_source_create(kBrowserSourceId, kBrowserSourceName, settings, nullptr);
	if (!br) {
		LOGW("Failed to create Browser Source (id=%s). Is the OBS Browser plugin installed?", kBrowserSourceId);
		obs_data_release(settings);
		return;
	}

	obs_sceneitem_t *item = obs_scene_add(scn, br);
	if (!item) {
		LOGW("Failed to add Browser Source to scene.");
		obs_source_release(br);
		obs_data_release(settings);
		return;
	}

	vec2 pos = {40.0f, 40.0f};
	obs_sceneitem_set_pos(item, &pos);

	LOGI("Created Browser Source '%s' -> %s (CSS cleared)", kBrowserSourceName, url);
	obs_source_release(br);
	obs_data_release(settings);
}

static void ensure_browser_for_current_scene_url(const char *url)
{
#ifdef ENABLE_FRONTEND_API
	create_or_update_browser_source(url);
#else
	UNUSED_PARAMETER(url);
#endif
}

#ifdef ENABLE_FRONTEND_API
static void frontend_event_cb(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		fly_hotkeys_apply_deferred_restore();

		int port = fly_server_port();
		if (port <= 0)
			port = 8089;
		char url[256];
		snprintf(url, sizeof(url), "http://127.0.0.1:%d/overlay/index.html", port);
		ensure_browser_for_current_scene_url(url);

		LOGI("Finished loading: hotkeys restored, browser ensured.");
	} break;

	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED: {
		int port = fly_server_port();
		if (port <= 0)
			port = 8089;
		char url[256];
		snprintf(url, sizeof(url), "http://127.0.0.1:%d/overlay/index.html", port);
		ensure_browser_for_current_scene_url(url);
	} break;

	default:
		break;
	}
}
#endif

// ---------------------------------------------------------------------------
// OBS module entry
// ---------------------------------------------------------------------------

OBS_DECLARE_MODULE();
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return PLUGIN_NAME;
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Fly Scoreboard — real-time sports/e-sports overlay with dock + HTTP server.";
}

bool obs_module_load(void)
{
	LOGI("Plugin loaded (version %s)", PLUGIN_VERSION);

	QString baseDir = seed_defaults_if_needed();

	int port = fly_server_start(baseDir, 8089);
	if (port == 0)
		LOGW("Web server failed to bind");
	else
		LOGI("Web server listening on :%d", port);

	fly_hotkeys_init();
	fly_create_dock();

#ifdef ENABLE_FRONTEND_API
	obs_frontend_add_event_callback(frontend_event_cb, nullptr);
#else
	LOGW("Frontend API not available; browser auto-setup and event callbacks disabled.");
#endif

	return true;
}

void obs_module_unload(void)
{
	LOGI("Plugin unloading...");

#ifdef ENABLE_FRONTEND_API
	obs_frontend_remove_event_callback(frontend_event_cb, nullptr);
#endif

	fly_hotkeys_shutdown();
	fly_destroy_dock();
	fly_server_stop();
	LOGI("Plugin unloaded");
}
