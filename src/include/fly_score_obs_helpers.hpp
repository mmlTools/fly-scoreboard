#pragma once
#include <QString>

bool fly_refresh_browser_source_by_name(const QString &name);
/**
 * Ensure a Browser Source named kBrowserSourceName exists in the current scene
 * and points to the given URL. If it exists, it's updated; otherwise it's created.
 *
 * Returns true on success, false if scene/browser-source is not available.
 */
bool fly_ensure_browser_source_in_current_scene(const QString &url);