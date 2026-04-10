#pragma once

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QList>
#include <QMutex>

/**
 * WebSocket JSON-RPC 2.0 server for VioSpice state bridge.
 *
 * Listens on ws://127.0.0.1:18420 by default.
 * Clients can request state, subscribe to events, and execute commands.
 *
 * JSON-RPC 2.0 Protocol:
 *   - state.get              → returns current app state
 *   - events.subscribe       → start receiving push events
 *   - events.unsubscribe     → stop receiving push events
 *   - command.execute        → trigger an action (open file, run sim, etc.)
 */
class WsServer : public QObject
{
    Q_OBJECT
public:
    explicit WsServer(quint16 port = 18420, QObject* parent = nullptr);
    ~WsServer();

    void start();
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }

    /** Broadcast a state-change event to all subscribed clients. */
    void broadcastEvent(const QString& eventName, const QJsonObject& data);

Q_SIGNALS:
    void clientConnected();
    void clientDisconnected();

private Q_SLOTS:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onDisconnected();

private:
    // JSON-RPC dispatch
    QJsonObject handleRequest(const QJsonObject& request);
    QJsonObject handleMethod(const QString& method, const QJsonObject& params);

    // Method implementations
    QJsonObject stateGet();
    void eventsSubscribe(QWebSocket* client);
    void eventsUnsubscribe(QWebSocket* client);
    QJsonObject commandExecute(const QJsonObject& params);

    // Helpers
    static QJsonObject makeResponse(int id, const QJsonValue& result);
    static QJsonObject makeError(int id, int code, const QString& message);
    static QJsonObject makeNotification(const QString& method, const QJsonObject& params);

    void sendToClient(QWebSocket* client, const QJsonObject& obj);

    quint16 m_port;
    QWebSocketServer* m_server;
    QList<QWebSocket*> m_clients;       // all connected clients
    QList<QWebSocket*> m_subscribers;   // clients subscribed to events
    QMutex m_mutex;
};
