#pragma once
#include <QString>

/**
 * Ensure a Browser Source named kBrowserSourceName exists in the current scene
 * and points to the given URL or local index.html path. If it exists, it's updated; otherwise it's created.
 *
 * Returns true on success, false if scene/browser-source is not available.
 */
bool fly_ensure_browser_source_in_current_scene(const QString &urlOrLocalIndex);