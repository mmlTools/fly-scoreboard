#include "fly_score_websocket_server.hpp"

#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][websocket]"
#include "fly_score_log.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

static QByteArray websocketFrame(const QByteArray &payload)
{
	QByteArray frame;
	frame.append(char(0x81));

	const qsizetype len = payload.size();
	if (len < 126) {
		frame.append(char(len));
	} else if (len <= 0xffff) {
		frame.append(char(126));
		frame.append(char((len >> 8) & 0xff));
		frame.append(char(len & 0xff));
	} else {
		frame.append(char(127));
		for (int i = 7; i >= 0; --i)
			frame.append(char((quint64(len) >> (8 * i)) & 0xff));
	}

	frame.append(payload);
	return frame;
}

FlyScoreWebSocketServer::FlyScoreWebSocketServer(QObject *parent) : QObject(parent) {}

FlyScoreWebSocketServer::~FlyScoreWebSocketServer()
{
	stop();
}

bool FlyScoreWebSocketServer::start(quint16 port)
{
	if (server_ && server_->isListening() && port_ == port)
		return true;

	stop();
	port_ = port ? port : 4457;
	server_ = new QTcpServer(this);
	connect(server_, &QTcpServer::newConnection, this, &FlyScoreWebSocketServer::onNewConnection);

	if (!server_->listen(QHostAddress::LocalHost, port_)) {
		LOGW("Failed to listen on ws://127.0.0.1:%u", static_cast<unsigned>(port_));
		server_->deleteLater();
		server_ = nullptr;
		emit statusChanged();
		return false;
	}

	LOGI("Listening on %s", url().toUtf8().constData());
	emit statusChanged();
	return true;
}

void FlyScoreWebSocketServer::stop()
{
	for (auto *client : clients_) {
		if (client)
			client->disconnectFromHost();
	}
	clients_.clear();
	buffers_.clear();
	handshaken_.clear();

	if (server_) {
		server_->close();
		server_->deleteLater();
		server_ = nullptr;
	}
	emit statusChanged();
}

bool FlyScoreWebSocketServer::isListening() const
{
	return server_ && server_->isListening();
}

quint16 FlyScoreWebSocketServer::port() const
{
	return port_;
}

QString FlyScoreWebSocketServer::url() const
{
	return QStringLiteral("ws://127.0.0.1:%1").arg(port_);
}

int FlyScoreWebSocketServer::clientCount() const
{
	return clients_.size();
}

void FlyScoreWebSocketServer::onNewConnection()
{
	if (!server_)
		return;

	while (auto *client = server_->nextPendingConnection()) {
		clients_.push_back(client);
		handshaken_.insert(client, false);
		connect(client, &QTcpSocket::readyRead, this, &FlyScoreWebSocketServer::onReadyRead);
		connect(client, &QTcpSocket::disconnected, this, [this, client]() { removeClient(client); });
		emit statusChanged();
	}
}

void FlyScoreWebSocketServer::onReadyRead()
{
	auto *client = qobject_cast<QTcpSocket *>(sender());
	if (!client)
		return;

	buffers_[client].append(client->readAll());
	processBuffer(client);
}

void FlyScoreWebSocketServer::processBuffer(QTcpSocket *client)
{
	QByteArray &buffer = buffers_[client];

	if (!handshaken_.value(client, false)) {
		const int end = buffer.indexOf("\r\n\r\n");
		if (end < 0)
			return;

		const QByteArray header = buffer.left(end + 4);
		buffer.remove(0, end + 4);

		QByteArray key;
		for (const QByteArray &line : header.split('\n')) {
			const QByteArray trimmed = line.trimmed();
			if (trimmed.toLower().startsWith("sec-websocket-key:")) {
				key = trimmed.mid(trimmed.indexOf(':') + 1).trimmed();
				break;
			}
		}

		if (key.isEmpty()) {
			client->disconnectFromHost();
			return;
		}

		const QByteArray accept = QCryptographicHash::hash(
						  key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
						  QCryptographicHash::Sha1)
						  .toBase64();
		QByteArray response;
		response += "HTTP/1.1 101 Switching Protocols\r\n";
		response += "Upgrade: websocket\r\n";
		response += "Connection: Upgrade\r\n";
		response += "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
		client->write(response);
		handshaken_[client] = true;
	}

	while (buffer.size() >= 2) {
		const quint8 b0 = quint8(buffer[0]);
		const quint8 b1 = quint8(buffer[1]);
		const quint8 opcode = b0 & 0x0f;
		const bool masked = (b1 & 0x80) != 0;
		quint64 len = b1 & 0x7f;
		int pos = 2;

		if (len == 126) {
			if (buffer.size() < pos + 2)
				return;
			len = (quint8(buffer[pos]) << 8) | quint8(buffer[pos + 1]);
			pos += 2;
		} else if (len == 127) {
			if (buffer.size() < pos + 8)
				return;
			len = 0;
			for (int i = 0; i < 8; ++i)
				len = (len << 8) | quint8(buffer[pos + i]);
			pos += 8;
		}

		QByteArray mask;
		if (masked) {
			if (buffer.size() < pos + 4)
				return;
			mask = buffer.mid(pos, 4);
			pos += 4;
		}

		if (len > 1024 * 1024) {
			client->disconnectFromHost();
			return;
		}
		if (buffer.size() < pos + qsizetype(len))
			return;

		QByteArray payload = buffer.mid(pos, qsizetype(len));
		buffer.remove(0, pos + qsizetype(len));

		if (masked) {
			for (qsizetype i = 0; i < payload.size(); ++i)
				payload[i] = payload[i] ^ mask[int(i % 4)];
		}

		if (opcode == 0x8) {
			client->disconnectFromHost();
			return;
		}
		if (opcode == 0x1)
			handleTextMessage(QString::fromUtf8(payload));
	}
}

void FlyScoreWebSocketServer::handleTextMessage(const QString &message)
{
	const auto doc = QJsonDocument::fromJson(message.toUtf8());
	if (!doc.isObject())
		return;

	emit commandReceived(doc.object());
}

void FlyScoreWebSocketServer::removeClient(QObject *client)
{
	auto *sock = qobject_cast<QTcpSocket *>(client);
	clients_.removeAll(sock);
	buffers_.remove(sock);
	handshaken_.remove(sock);
	if (client)
		client->deleteLater();
	emit statusChanged();
}

QJsonObject FlyScoreWebSocketServer::makeStateEnvelope(const FlyState &state, const QString &templateName,
						       const QString &templatePath) const
{
	QJsonObject env;
	env.insert(QStringLiteral("type"), QStringLiteral("state"));
	env.insert(QStringLiteral("state"), fly_state_to_json_object(state));
	env.insert(QStringLiteral("template"), templateName);
	env.insert(QStringLiteral("template_path"), templatePath);
	return env;
}

void FlyScoreWebSocketServer::sendText(QTcpSocket *client, const QString &message)
{
	if (!client || !handshaken_.value(client, false))
		return;

	client->write(websocketFrame(message.toUtf8()));
}

void FlyScoreWebSocketServer::sendState(QTcpSocket *client, const FlyState &state, const QString &templateName,
					const QString &templatePath)
{
	const QJsonDocument doc(makeStateEnvelope(state, templateName, templatePath));
	sendText(client, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void FlyScoreWebSocketServer::broadcastState(const FlyState &state, const QString &templateName,
					     const QString &templatePath)
{
	const QJsonDocument doc(makeStateEnvelope(state, templateName, templatePath));
	const QString payload = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

	for (auto *client : clients_)
		sendText(client, payload);
}
