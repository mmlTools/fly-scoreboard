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


static void refreshSourceSettings(obs_source_t *s)
{
	if (!s)
		return;

	obs_data_t *data = obs_source_get_settings(s);
	obs_source_update(s, data);
	obs_data_release(data);

	if (strcmp(obs_source_get_id(s), "browser_source") == 0) {
		obs_properties_t *sourceProperties = obs_source_properties(s);
		obs_property_t *property = obs_properties_get(sourceProperties, "refreshnocache");
		if (property)
			obs_property_button_clicked(property, s);
		obs_properties_destroy(sourceProperties);
	}
}

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

bool fly_ensure_browser_source_in_current_scene(const QString &urlOrLocalIndex, const QString &browserSourceName)
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

    QByteArray bsNameUtf8 = browserSourceName.toUtf8();

	struct FindCtx {
		const char *name = nullptr;
		obs_sceneitem_t **outItem = nullptr;
	};

	FindCtx ctx;
	ctx.name = bsNameUtf8.constData();
	ctx.outItem = &item;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *it, void *param) {
			auto *c = static_cast<FindCtx *>(param);
			obs_source_t *src = obs_sceneitem_get_source(it);
			if (!src)
				return true;
			if (strcmp(obs_source_get_id(src), kBrowserSourceId) != 0)
				return true;
			if (strcmp(obs_source_get_name(src), c->name) == 0) {
				*(c->outItem) = it;
				return false;
			}
			return true;
		},
		&ctx);
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
		refreshSourceSettings(br);
	    LOGI("Updated Browser Source '%s' -> %s", browserSourceName.toUtf8().constData(),
		 isLocal ? localIndex.toUtf8().constData() : url.toUtf8().constData());
	    obs_source_release(br);
	    obs_data_release(settings);
	    obs_source_release(sceneSource);
	    return true;
    }

    // Create
    br = obs_source_create_private(kBrowserSourceId, browserSourceName.toUtf8().constData(), settings);
    if (!br) {
	    LOGW("Failed to create Browser Source");
	    obs_data_release(settings);
	    obs_source_release(sceneSource);
	    return false;
    }

    item = obs_scene_add(scene, br);

    // Force refresh/no-cache for browser source
    refreshSourceSettings(br);

    // Position
    vec2 pos = {40.0f, 40.0f};
    obs_sceneitem_set_pos(item, &pos);

    LOGI("Created Browser Source '%s' -> %s", browserSourceName.toUtf8().constData(),
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

QStringList fly_list_browser_sources()
{
    QStringList names;

    obs_enum_sources(
        [](void *param, obs_source_t *src) {
            if (!src)
                return true;
            auto *out = static_cast<QStringList *>(param);
            const char *id = obs_source_get_id(src);
            if (!id)
                return true;
            if (strcmp(id, kBrowserSourceId) != 0)
                return true;

            const char *nm = obs_source_get_name(src);
            if (!nm || !*nm)
                return true;

            out->push_back(QString::fromUtf8(nm));
            return true;
        },
        &names);

    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}