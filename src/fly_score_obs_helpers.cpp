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
#include <cstring>

// -----------------------------------------------------------------------------
// Refresh existing browser by name (unchanged)
// -----------------------------------------------------------------------------

bool fly_refresh_browser_source_by_name(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;

    obs_source_t *src = obs_get_source_by_name(trimmed.toUtf8().constData());
    if (!src) {
        LOGW("Browser source not found: %s", trimmed.toUtf8().constData());
        return false;
    }

    bool ok = false;
    const char *sid = obs_source_get_id(src);
    if (sid && strcmp(sid, "browser_source") == 0) {
        obs_data_t *settings = obs_source_get_settings(src);
        if (settings) {
            const bool isLocal = obs_data_get_bool(settings, "is_local_file");
            if (!isLocal) {
                const char *curl = obs_data_get_string(settings, "url");
                QString url = fly_cache_bust_url(QString::fromUtf8(curl ? curl : ""));
                obs_data_set_string(settings, "url", url.toUtf8().constData());
                obs_source_update(src, settings);
                ok = true;
            } else {
                bool shutdown = obs_data_get_bool(settings, "shutdown");
                obs_data_set_bool(settings, "shutdown", !shutdown);
                obs_source_update(src, settings);
                obs_data_set_bool(settings, "shutdown", shutdown);
                obs_source_update(src, settings);
                ok = true;
            }
            obs_data_release(settings);
        }
    } else {
        LOGW("Source '%s' is not a browser_source", trimmed.toUtf8().constData());
    }

    obs_source_release(src);
    return ok;
}

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

bool fly_ensure_browser_source_in_current_scene(const QString &url)
{
#ifdef ENABLE_FRONTEND_API
    obs_scene_t *scn = fly_get_current_scene();
    if (!scn) {
        LOGW("No current scene; cannot create Browser Source.");
        return false;
    }

    obs_data_t *settings = obs_data_create();
    const QByteArray u = url.toUtf8();

    obs_data_set_bool(settings, "is_local_file", false);
    obs_data_set_string(settings, "url", u.constData());
    obs_data_set_int(settings, "width",  kBrowserWidth);
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
        LOGI("Updated Browser Source '%s' -> %s (CSS cleared)",
             kBrowserSourceName, u.constData());
        obs_data_release(settings);
        return true;
    }

    // Create new browser source
    obs_source_t *br = obs_source_create(kBrowserSourceId, kBrowserSourceName, settings, nullptr);
    if (!br) {
        LOGW("Failed to create Browser Source (id=%s). Is the OBS Browser plugin installed?",
             kBrowserSourceId);
        obs_data_release(settings);
        return false;
    }

    obs_sceneitem_t *item = obs_scene_add(scn, br);
    if (!item) {
        LOGW("Failed to add Browser Source to scene.");
        obs_source_release(br);
        obs_data_release(settings);
        return false;
    }

    vec2 pos = {40.0f, 40.0f};
    obs_sceneitem_set_pos(item, &pos);

    LOGI("Created Browser Source '%s' -> %s (CSS cleared)", kBrowserSourceName, u.constData());
    obs_source_release(br);
    obs_data_release(settings);
    return true;
#else
    Q_UNUSED(url);
    LOGW("Frontend API not available; cannot create Browser Source.");
    return false;
#endif
}
