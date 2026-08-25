#include "fly_score_event_log.hpp"

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <algorithm>

bool FlyScoreEventLog::startSession(const QString &directory, const QString &firstEvent)
{
	QDir dir(directory);
	if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
		return false;

	startedAt_ = QDateTime::currentDateTime();
	elapsed_.start();
	const QString baseName =
		QStringLiteral("fly-scoreboard-%1").arg(startedAt_.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")));
	filePath_ = dir.filePath(baseName + QStringLiteral(".txt"));
	for (int suffix = 2; QFile::exists(filePath_); ++suffix)
		filePath_ = dir.filePath(QStringLiteral("%1-%2.txt").arg(baseName).arg(suffix));

	QFile file(filePath_);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		filePath_.clear();
		return false;
	}
	file.close();
	active_ = true;

	if (!firstEvent.trimmed().isEmpty() && !appendEvent(firstEvent)) {
		stopSession();
		return false;
	}
	return true;
}

bool FlyScoreEventLog::appendEvent(const QString &event)
{
	if (!active_ || filePath_.isEmpty())
		return false;

	QString clean = event.simplified();
	if (clean.isEmpty())
		return false;

	QFile file(filePath_);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
		return false;

	QTextStream stream(&file);
	stream << timestamp() << ' ' << clean << '\n';
	stream.flush();
	return stream.status() == QTextStream::Ok;
}

void FlyScoreEventLog::stopSession()
{
	active_ = false;
}

bool FlyScoreEventLog::isActive() const
{
	return active_;
}

QString FlyScoreEventLog::filePath() const
{
	return filePath_;
}

FlyScoreEventLog::TimestampMode FlyScoreEventLog::timestampMode() const
{
	return timestampMode_;
}

void FlyScoreEventLog::setTimestampMode(TimestampMode mode)
{
	timestampMode_ = mode;
}

QString FlyScoreEventLog::timestamp() const
{
	const QDateTime now = QDateTime::currentDateTime();
	if (timestampMode_ == TimestampMode::WallClock)
		return now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

	qint64 seconds = std::max<qint64>(0, elapsed_.elapsed() / 1000);
	const qint64 hours = seconds / 3600;
	seconds %= 3600;
	const qint64 minutes = seconds / 60;
	const qint64 remainder = seconds % 60;
	return QStringLiteral("%1:%2:%3")
		.arg(hours)
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(remainder, 2, 10, QLatin1Char('0'));
}
