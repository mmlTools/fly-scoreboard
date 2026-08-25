#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QString>

class FlyScoreEventLog {
public:
	enum class TimestampMode { Relative, WallClock };

	bool startSession(const QString &directory, const QString &firstEvent);
	bool appendEvent(const QString &event);
	void stopSession();

	bool isActive() const;
	QString filePath() const;
	TimestampMode timestampMode() const;
	void setTimestampMode(TimestampMode mode);

private:
	QString timestamp() const;

	QString filePath_;
	QDateTime startedAt_;
	QElapsedTimer elapsed_;
	TimestampMode timestampMode_ = TimestampMode::Relative;
	bool active_ = false;
};
