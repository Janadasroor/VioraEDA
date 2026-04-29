#include "remote_display_server.h"
#if VIOSPICE_HAS_QT_WEBSOCKETS
#include <QHostAddress>
#include <QDebug>

RemoteDisplayServer::RemoteDisplayServer(quint16 port, QObject *parent)
    : QObject(parent), m_port(port) {
}

RemoteDisplayServer::~RemoteDisplayServer() {
    stop();
}

RemoteDisplayServer& RemoteDisplayServer::instance() {
    static RemoteDisplayServer s_instance;
    return s_instance;
}

void RemoteDisplayServer::start() {
    if (m_server) return;

    m_server = new QWebSocketServer(QStringLiteral("VioSpice Remote"),
                                    QWebSocketServer::NonSecureMode, this);

    if (m_server->listen(QHostAddress::Any, m_port)) {
        qDebug() << "Remote Display Server started on port" << m_port;
        connect(m_server, &QWebSocketServer::newConnection,
                this, &RemoteDisplayServer::onNewConnection);
    } else {
        qWarning() << "Remote display server error:" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
    }
}

void RemoteDisplayServer::stop() {
    if (m_server) {
        m_server->close();
        qDeleteAll(m_clients.begin(), m_clients.end());
        m_clients.clear();
        delete m_server;
        m_server = nullptr;
    }
}

void RemoteDisplayServer::onNewConnection() {
    QWebSocket *socket = m_server->nextPendingConnection();

    connect(socket, &QWebSocket::textMessageReceived, this, &RemoteDisplayServer::processTextMessage);
    connect(socket, &QWebSocket::disconnected, this, &RemoteDisplayServer::socketDisconnected);

    m_clients << socket;
    const QString addr = socket->peerAddress().toString();
    qDebug() << "Remote display client connected:" << addr;
    emit clientConnected(addr);
    
    // Send welcome info
    QJsonObject welcome;
    welcome["type"] = "welcome";
    welcome["server"] = "VioSpice Remote Display";
    socket->sendTextMessage(QJsonDocument(welcome).toJson(QJsonDocument::Compact));
}

void RemoteDisplayServer::processTextMessage(QString message) {
    // Optional: Handle commands from mobile (e.g. "Pause Simulation")
}

void RemoteDisplayServer::socketDisconnected() {
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    if (socket) {
        m_clients.removeAll(socket);
        const QString addr = socket->peerAddress().toString();
        qDebug() << "Remote display client disconnected:" << addr;
        emit clientDisconnected(addr);
        socket->deleteLater();
    }
}

void RemoteDisplayServer::broadcastUpdate(const QString& type, const QUuid& instrumentId, const QJsonObject& data) {
    if (m_clients.isEmpty()) return;

    QJsonObject root;
    root["type"] = type;
    root["id"] = instrumentId.toString();
    root["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    root["data"] = data;

    QByteArray msg = QJsonDocument(root).toJson(QJsonDocument::Compact);
    for (QWebSocket *client : m_clients) {
        client->sendTextMessage(msg);
    }
}
#else
#include <QDebug>

RemoteDisplayServer::RemoteDisplayServer(quint16 port, QObject *parent)
    : QObject(parent), m_port(port) {
}

RemoteDisplayServer::~RemoteDisplayServer() = default;

RemoteDisplayServer& RemoteDisplayServer::instance() {
    static RemoteDisplayServer s_instance;
    return s_instance;
}

void RemoteDisplayServer::start() {
    qWarning() << "Qt WebSockets is not available in this build.";
}

void RemoteDisplayServer::stop() {
}

void RemoteDisplayServer::onNewConnection() {
}

void RemoteDisplayServer::processTextMessage(QString) {
}

void RemoteDisplayServer::socketDisconnected() {
}

void RemoteDisplayServer::broadcastUpdate(const QString&, const QUuid&, const QJsonObject&) {
}
#endif
