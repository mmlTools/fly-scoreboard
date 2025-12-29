#include "fly_score_obs_helpers.hpp"

#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][dock-obs]"
#include "fly_score_log.hpp"

#include "fly_score_qt_helpers.hpp"
#include "fly_score_const.hpp"

#include <obs.h>

#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QUrl>
#include <QUrlQuery>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <cstring>

// -----------------------------------------------------------------------------
// Create/update Browser Source in current scene
// -----------------------------------------------------------------------------

#ifdef ENABLE_FRONTEND_API
static obs_scene_t *fly_get_current_scene()
{
    obs_source_t *cur = obs_frontend_get_current_scene();
    if (!cur)
        return nullptr;

    obs_scene_t *scn = obs_scene_from_source(cur);
    obs_source_release(cur);
    return scn;
}
#endif

bool fly_ensure_browser_source_in_current_scene(const QString &urlOrLocalIndex)
{
#ifdef ENABLE_FRONTEND_API
	obs_source_t *sceneSource = obs_frontend_get_current_scene();
	if (!sceneSource) {
		LOGW("No current scene (obs_frontend_get_current_scene returned null)");
		return false;
	}

    obs_scene_t *scene = obs_scene_from_source(sceneSource);
    if (!scene) {
	    LOGW("Current source is not a scene");
	    obs_source_release(sceneSource);
	    return false;
    }

    // If urlOrLocalIndex points to an existing file, use the Browser Source "Local File" mode.
    const bool isLocal = QFileInfo::exists(urlOrLocalIndex);
    const QString localIndex = isLocal ? QDir::cleanPath(urlOrLocalIndex) : QString();
    const QString url = isLocal ? QString() : urlOrLocalIndex;

    // Find existing browser source item by name
    obs_sceneitem_t *item = nullptr;
    obs_source_t *br = nullptr;

    obs_scene_enum_items(
	    scene,
	    [](obs_scene_t *, obs_sceneitem_t *it, void *param) {
		    auto **outItem = static_cast<obs_sceneitem_t **>(param);
		    obs_source_t *src = obs_sceneitem_get_source(it);
		    if (!src)
			    return true;
		    if (strcmp(obs_source_get_name(src), kBrowserSourceName) == 0 &&
			strcmp(obs_source_get_id(src), kBrowserSourceId) == 0) {
			    *outItem = it;
			    return false;
		    }
		    return true;
	    },
	    &item);

    // NOTE: obs_sceneitem_get_source returns a borrowed pointer; obtain a ref-counted one.
    if (item) {
	    obs_source_t *borrowed = obs_sceneitem_get_source(item);
	    if (borrowed)
		    br = obs_source_get_ref(borrowed);
    }

    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "width", kBrowserWidth);
    obs_data_set_int(settings, "height", kBrowserHeight);

    // Always clear CSS from plugin-managed source, to avoid stale user CSS
    obs_data_set_string(settings, "css", "");

    if (isLocal) {
	    obs_data_set_bool(settings, "is_local_file", true);
	    obs_data_set_string(settings, "local_file", localIndex.toUtf8().constData());
	    // Some OBS builds still read "url" even for local file; keep it empty.
	    obs_data_set_string(settings, "url", "");
    } else {
	    obs_data_set_bool(settings, "is_local_file", false);
	    obs_data_set_string(settings, "url", url.toUtf8().constData());
	    obs_data_set_string(settings, "local_file", "");
    }

    if (br) {
	    obs_source_update(br, settings);
	    LOGI("Updated Browser Source '%s' -> %s", kBrowserSourceName,
		 isLocal ? localIndex.toUtf8().constData() : url.toUtf8().constData());
	    obs_source_release(br);
	    obs_data_release(settings);
	    obs_source_release(sceneSource);
	    return true;
    }

    // Create
    br = obs_source_create_private(kBrowserSourceId, kBrowserSourceName, settings);
    if (!br) {
	    LOGW("Failed to create Browser Source");
	    obs_data_release(settings);
	    obs_source_release(sceneSource);
	    return false;
    }

    item = obs_scene_add(scene, br);

    // Position
    vec2 pos = {40.0f, 40.0f};
    obs_sceneitem_set_pos(item, &pos);

    LOGI("Created Browser Source '%s' -> %s", kBrowserSourceName,
	 isLocal ? localIndex.toUtf8().constData() : url.toUtf8().constData());
    obs_source_release(br);
    obs_data_release(settings);
    obs_source_release(sceneSource);
    return true;
#else
	Q_UNUSED(urlOrLocalIndex);
	LOGW("Frontend API not available; cannot create Browser Source.");
	return false;
#endif
}