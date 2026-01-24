#pragma once
#include <QString>
#include <QStringList>

#include "fly_score_const.hpp"
/**
 * Ensure a Browser Source named kBrowserSourceName exists in the current scene
 * and points to the given URL or local index.html path. If it exists, it's updated; otherwise it's created.
 *
 * Returns true on success, false if scene/browser-source is not available.
 */
bool fly_ensure_browser_source_in_current_scene(const QString &urlOrLocalIndex,
                                   const QString &browserSourceName = QString::fromUtf8(kBrowserSourceName));

// List all Browser Sources (type: browser_source) in the current OBS session.
QStringList fly_list_browser_sources();