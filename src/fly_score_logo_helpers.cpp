#include "fly_score_logo_helpers.hpp"

#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][dock-logo]"
#include "fly_score_log.hpp"

#include <QMimeDatabase>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>

QString fly_normalized_ext_from_mime(const QString &path)
{
    QMimeDatabase db;
    auto mt = db.mimeTypeForFile(path);
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty() || ext.size() > 5) {
        const QString name = mt.name();
        if (name.contains(QStringLiteral("png")))
            ext = QStringLiteral("png");
        else if (name.contains(QStringLiteral("jpeg")))
            ext = QStringLiteral("jpg");
        else if (name.contains(QStringLiteral("svg")))
            ext = QStringLiteral("svg");
        else if (name.contains(QStringLiteral("webp")))
            ext = QStringLiteral("webp");
    }
    if (ext == QStringLiteral("jpeg"))
        ext = QStringLiteral("jpg");
    return ext.isEmpty() ? QStringLiteral("png") : ext;
}

static QString fly_short_hash_of_file(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash h(QCryptographicHash::Sha256);
    while (!f.atEnd())
        h.addData(f.read(64 * 1024));
    return QString::fromLatin1(h.result().toHex().left(8));
}

QString fly_copy_logo_to_overlay(const QString &dataDir,
                                 const QString &srcAbs,
                                 const QString &baseName)
{
    if (srcAbs.isEmpty())
        return QString();

    const QString ovlDir = QDir(dataDir).filePath(QStringLiteral("overlay"));
    QDir().mkpath(ovlDir);

    // Clean existing with same basename
    QDir d(ovlDir);
    const auto files = d.entryList(QStringList{QString("%1*.*").arg(baseName)}, QDir::Files);
    for (const auto &fn : files)
        QFile::remove(d.filePath(fn));

    const QString ext = fly_normalized_ext_from_mime(srcAbs);
    const QString sh = fly_short_hash_of_file(srcAbs);
    const QString rel = sh.isEmpty()
                            ? QString("%1.%2").arg(baseName, ext)
                            : QString("%1-%2.%3").arg(baseName, sh, ext);
    const QString dst = QDir(ovlDir).filePath(rel);

    if (!QFile::copy(srcAbs, dst)) {
        LOGW("Failed to copy logo to overlay: %s -> %s",
             srcAbs.toUtf8().constData(), dst.toUtf8().constData());
        return QString();
    }
    return rel;
}

bool fly_delete_logo_if_exists(const QString &dataDir,
                               const QString &relPath)
{
    const QString trimmed = relPath.trimmed();
    if (trimmed.isEmpty())
        return false;

    const QString abs = QDir(QDir(dataDir).filePath(QStringLiteral("overlay"))).filePath(trimmed);
    if (QFile::exists(abs)) {
        if (!QFile::remove(abs)) {
            LOGW("Failed removing logo: %s", abs.toUtf8().constData());
            return false;
        }
        return true;
    }
    return false;
}

void fly_clean_overlay_prefix(const QString &dataDir,
                              const QString &basePrefix)
{
    const QString ovlDir = QDir(dataDir).filePath(QStringLiteral("overlay"));
    QDir d(ovlDir);
    const auto files = d.entryList(QStringList{QString("%1*.*").arg(basePrefix)}, QDir::Files);
    for (const auto &fn : files)
        QFile::remove(d.filePath(fn));
}
