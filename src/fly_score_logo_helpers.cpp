#include "fly_score_logo_helpers.hpp"

#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][dock-logo]"
#include "fly_score_log.hpp"

#include <QMimeDatabase>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

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

// Small helper: normalize document root
static QString fly_doc_root(const QString &dataDir)
{
	return QDir(dataDir).absolutePath();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

QString fly_copy_logo_to_overlay(const QString &dataDir, const QString &srcAbs, const QString &baseName)
{
	if (srcAbs.isEmpty())
		return QString();

	// ❗ dataDir is now treated as the *docRoot* directly, no "overlay" subfolder
	const QString rootDirPath = fly_doc_root(dataDir);
	QDir rootDir(rootDirPath);
	if (!rootDir.exists()) {
		if (!rootDir.mkpath(QStringLiteral("."))) {
			LOGW("Failed to create docRoot for logos: %s", rootDirPath.toUtf8().constData());
			return QString();
		}
	}

	// Clean existing with same basename in docRoot (baseName*.*)
	const auto files = rootDir.entryList(QStringList{QString("%1*.*").arg(baseName)}, QDir::Files);
	for (const auto &fn : files)
		QFile::remove(rootDir.filePath(fn));

	const QString ext = fly_normalized_ext_from_mime(srcAbs);
	const QString sh = fly_short_hash_of_file(srcAbs);

	const QString rel = sh.isEmpty() ? QString("%1.%2").arg(baseName, ext)
					 : QString("%1-%2.%3").arg(baseName, sh, ext);

	const QString dst = rootDir.filePath(rel);

	if (!QFile::copy(srcAbs, dst)) {
		LOGW("Failed to copy logo to docRoot: %s -> %s", srcAbs.toUtf8().constData(), dst.toUtf8().constData());
		return QString();
	}

	LOGI("Logo copied to docRoot: %s (rel=%s)", dst.toUtf8().constData(), rel.toUtf8().constData());

	// Return path *relative to docRoot* (e.g. "home-1234abcd.png")
	return rel;
}

bool fly_delete_logo_if_exists(const QString &dataDir, const QString &relPath)
{
	const QString trimmed = relPath.trimmed();
	if (trimmed.isEmpty())
		return false;

	// Previously: dataDir/overlay/relPath
	// Now:       dataDir/relPath  (docRoot)
	const QString abs = QDir(fly_doc_root(dataDir)).filePath(trimmed);
	if (QFile::exists(abs)) {
		if (!QFile::remove(abs)) {
			LOGW("Failed removing logo: %s", abs.toUtf8().constData());
			return false;
		}
		LOGI("Removed logo: %s", abs.toUtf8().constData());
		return true;
	}
	return false;
}

void fly_clean_overlay_prefix(const QString &dataDir, const QString &basePrefix)
{
	// Name stays the same for compatibility, but now cleans inside docRoot
	const QString rootDirPath = fly_doc_root(dataDir);
	QDir d(rootDirPath);
	if (!d.exists())
		return;

	const auto files = d.entryList(QStringList{QString("%1*.*").arg(basePrefix)}, QDir::Files);
	for (const auto &fn : files) {
		const QString abs = d.filePath(fn);
		if (QFile::remove(abs)) {
			LOGI("Cleaned logo with prefix '%s': %s", basePrefix.toUtf8().constData(),
			     abs.toUtf8().constData());
		}
	}
}
