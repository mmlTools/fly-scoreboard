#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][plugin]"
#include "fly_score_log.hpp"

#include <obs-module.h>
#include <obs.h>

#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QString>
#include <QDir>

#include "fly_score_paths.hpp"
#include "fly_score_server.hpp"
#include "fly_score_state.hpp"
#include "fly_score_dock.hpp"
#include "fly_score_const.hpp"
#include "fly_score_hotkeys.hpp"

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

#ifdef ENABLE_FRONTEND_API
static void frontend_event_cb(enum obs_frontend_event event, void *)
{
    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        // Just restore hotkeys when OBS finishes loading.
        fly_hotkeys_apply_deferred_restore();
        LOGI("Finished loading: hotkeys restored.");
        break;
    default:
        break;
    }
}
#endif

bool obs_module_load(void)
{
    LOGI("Plugin loaded (version %s)", PLUGIN_VERSION);

    // Global data root: silently default on first run, then stick to it.
    const QString dataRoot    = fly_get_data_root(nullptr);
    const QString overlayRoot = QDir(dataRoot).filePath(QStringLiteral("overlay"));

    int port = fly_server_start(overlayRoot, 8089);
    if (port == 0)
        LOGW("Web server failed to bind");
    else
        LOGI("Web server listening on :%d (docRoot=%s)", port, overlayRoot.toUtf8().constData());

    fly_hotkeys_init();
    fly_create_dock();

#ifdef ENABLE_FRONTEND_API
    obs_frontend_add_event_callback(frontend_event_cb, nullptr);
#else
    LOGW("Frontend API not available; hotkey restore callback disabled.");
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
