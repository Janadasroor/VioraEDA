#ifndef REMOTE_DISPLAY_SERVER_H
#define REMOTE_DISPLAY_SERVER_H

#include <QObject>
#if __has_include(<QtWebSockets/QWebSocketServer>)
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#define VIOSPICE_HAS_QT_WEBSOCKETS 1
#else
#define VIOSPICE_HAS_QT_WEBSOCKETS 0
class QWebSocketServer;
class QWebSocket;
#endif
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QUuid>

/**
 * @brief WebSocket server to stream real-time instrument data to mobile/web clients.
 */
class RemoteDisplayServer : public QObject {
public:
    explicit RemoteDisplayServer(quint16 port = 8080, QObject *parent = nullptr);
    ~RemoteDisplayServer();

    bool isRunning() const {
#if VIOSPICE_HAS_QT_WEBSOCKETS
        return m_server != nullptr && m_server->isListening();
#else
        return false;
#endif
    }
    quint16 port() const {
#if VIOSPICE_HAS_QT_WEBSOCKETS
        return m_server ? m_server->serverPort() : 0;
#else
        return 0;
#endif
    }
    QString serverUrl() const {
#if VIOSPICE_HAS_QT_WEBSOCKETS
        return m_server ? m_server->serverUrl().toString() : "";
#else
        return QString();
#endif
    }

    static RemoteDisplayServer& instance();

    void start();
    void stop();

    /**
     * @brief Broadcast instrument update to all connected clients.
     * @param type "oscilloscope" or "logicanalyzer"
     * @param instrumentId Unique ID of the instrument item
     * @param data JSON object containing trace points/states
     */
    void broadcastUpdate(const QString& type, const QUuid& instrumentId, const QJsonObject& data);

    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();

private:
    QWebSocketServer *m_server = nullptr;
    QList<QWebSocket *> m_clients;
    quint16 m_port;
};

#endif // REMOTE_DISPLAY_SERVER_H
