#pragma once

#include <QString>

class QWidget;

/**
 * Default data root used as suggestion/fallback.
 * Typically something like:
 *   %APPDATA%/fly-scoreboard   (Windows)
 *   ~/.local/share/fly-scoreboard (Linux/Mac)
 */
QString fly_default_data_root();

/**
 * Get the current data root from QSettings, without ever showing any UI.
 * Returns empty string if nothing is configured yet.
 */
QString fly_get_data_root_no_ui();

/**
 * Get the global data root for the plugin.
 *
 * Behavior:
 *   - If already configured in QSettings, returns that path.
 *   - If not configured, picks fly_default_data_root(), creates it, stores it in QSettings.
 *   - Never shows any dialogs (parent is currently ignored).
 */
QString fly_get_data_root(QWidget *parentForDialogs = nullptr);

/**
 * Explicitly set/override the global data root and persist it.
 * Ensures the directory exists.
 */
void fly_set_data_root(const QString &path);
