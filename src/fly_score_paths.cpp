#include "fly_score_paths.hpp"
#include "config.hpp"

#define LOG_TAG "[" PLUGIN_NAME "][paths]"
#include "fly_score_log.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QSettings>

// Keys used in QSettings
static inline QString org_name()  { return QStringLiteral("MMLTech"); }
static inline QString app_name()  { return QStringLiteral("fly-scoreboard"); }
static inline QString key_data()  { return QStringLiteral("paths/data_root"); }

// -----------------------------------------------------------------------------
// Default location (used as suggestion / fallback)
// -----------------------------------------------------------------------------

QString fly_default_data_root()
{
    // Use a standard app data location
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (base.isEmpty()) {
        // Last-resort fallback to home.
        base = QDir::home().filePath(QStringLiteral(".fly-scoreboard"));
    } else {
        // Put everything into a stable "fly-scoreboard" subfolder.
        QDir d(base);
        base = d.filePath(QStringLiteral("fly-scoreboard"));
    }

    QDir d(base);
    if (!d.exists()) {
        d.mkpath(QStringLiteral("."));
    }

    return QDir(base).absolutePath();
}

// -----------------------------------------------------------------------------
// Settings-backed helpers
// -----------------------------------------------------------------------------

QString fly_get_data_root_no_ui()
{
    QSettings s(org_name(), app_name());
    QString p = s.value(key_data()).toString();
    if (p.isEmpty())
        return QString();

    QDir d(p);
    if (!d.exists())
        d.mkpath(QStringLiteral("."));

    return d.absolutePath();
}

void fly_set_data_root(const QString &path)
{
    if (path.isEmpty())
        return;

    QDir d(path);
    if (!d.exists()) {
        d.mkpath(QStringLiteral("."));
    }

    const QString abs = d.absolutePath();

    QSettings s(org_name(), app_name());
    s.setValue(key_data(), abs);
    s.sync();

    LOGI("Global data root set to: %s", abs.toUtf8().constData());
}

// -----------------------------------------------------------------------------
// Public API with auto-default (no UI)
// -----------------------------------------------------------------------------

QString fly_get_data_root(QWidget * /*parentForDialogs*/)
{
    // 1) Already configured?
    QString existing = fly_get_data_root_no_ui();
    if (!existing.isEmpty())
        return existing;

    // 2) First run: silently use default
    const QString def = fly_default_data_root();
    fly_set_data_root(def);
    return fly_get_data_root_no_ui();
}
