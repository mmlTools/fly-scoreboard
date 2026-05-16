#pragma once

#include <QObject>
#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QString>

#include "fly_score_state.hpp"

class QTcpServer;
class QTcpSocket;

class FlyScoreWebSocketServer : public QObject {
	Q_OBJECT
public:
	explicit FlyScoreWebSocketServer(QObject *parent = nullptr);
	~FlyScoreWebSocketServer() override;

	bool start(quint16 port);
	void stop();
	bool isListening() const;
	quint16 port() const;
	QString url() const;
	int clientCount() const;

	void broadcastState(const FlyState &state, const QString &templateName, const QString &templatePath);
	void sendState(QTcpSocket *client, const FlyState &state, const QString &templateName,
		       const QString &templatePath);

signals:
	void commandReceived(const QJsonObject &command);
	void statusChanged();

private:
	void onNewConnection();
	void onReadyRead();
	void removeClient(QObject *client);
	void processBuffer(QTcpSocket *client);
	void handleTextMessage(const QString &message);
	void sendText(QTcpSocket *client, const QString &message);
	QJsonObject makeStateEnvelope(const FlyState &state, const QString &templateName,
				      const QString &templatePath) const;

	QTcpServer *server_ = nullptr;
	QList<QTcpSocket *> clients_;
	QHash<QTcpSocket *, QByteArray> buffers_;
	QHash<QTcpSocket *, bool> handshaken_;
	quint16 port_ = 4457;
};
