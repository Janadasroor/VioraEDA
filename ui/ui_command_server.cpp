#include "ui_command_server.h"

#include <QDebug>
#include <QAction>
#include <QMenuBar>
#include <QMessageBox>

UICommandServer::UICommandServer(QObject* parent)
    : QObject(parent)
{
}

UICommandServer::~UICommandServer() {
    stop();
}

UICommandServer& UICommandServer::instance() {
    static UICommandServer s_instance;
    return s_instance;
}

bool UICommandServer::start(int port) {
    if (m_server && m_server->isListening()) {
        return true;
    }

    m_server = new QWebSocketServer(QStringLiteral("VioSpice UI Command Server"),
                                     QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "UICommandServer: Failed to listen on port" << port
                   << "-" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    m_port = port;
    connect(m_server, &QWebSocketServer::newConnection, this, &UICommandServer::onNewConnection);

    qDebug() << "UICommandServer listening on port" << port;
    return true;
}

void UICommandServer::stop() {
    if (!m_server) return;

    // Disconnect all clients
    for (QWebSocket* client : m_clients) {
        client->close();
        client->deleteLater();
    }
    m_clients.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;

    qDebug() << "UICommandServer stopped";
}

bool UICommandServer::isRunning() const {
    return m_server && m_server->isListening();
}

void UICommandServer::registerCommand(const QString& cmd, CommandHandler handler) {
    m_commandHandlers[cmd] = std::move(handler);
}

void UICommandServer::broadcastToClients(const QVariantMap& message) {
    QJsonDocument doc = QJsonDocument::fromVariant(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (QWebSocket* client : m_clients) {
        client->sendTextMessage(QString::fromUtf8(data));
    }
}

void UICommandServer::onNewConnection() {
    QWebSocket* socket = m_server->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QWebSocket::textMessageReceived, this, &UICommandServer::onTextMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &UICommandServer::onDisconnected);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.append(socket);
    }

    qDebug() << "UICommandServer: Python client connected:" << socket->peerName();
}

void UICommandServer::onTextMessageReceived(const QString& message) {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) return;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        QVariantMap response;
        response["ok"] = false;
        response["error"] = QStringLiteral("Invalid JSON: ") + error.errorString();
        sendResponse(client, response);
        return;
    }

    QVariantMap request = doc.toVariant().toMap();
    QVariantMap response = handleCommand(request);
    sendResponse(client, response);
}

void UICommandServer::onDisconnected() {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.removeAll(client);
    }

    qDebug() << "UICommandServer: Python client disconnected";
    client->deleteLater();
}

QVariantMap UICommandServer::handleCommand(const QVariantMap& request) {
    QVariantMap response;
    response["ok"] = false;

    // Echo the request ID back so the client can match the response
    if (request.contains("id")) {
        response["id"] = request.value("id");
    }

    QString cmd = request.value("cmd").toString();
    if (cmd.isEmpty()) {
        response["error"] = "Missing 'cmd' field";
        return response;
    }

    QVariantMap params = request.value("params").toMap();

    // Check registered custom handlers first
    if (m_commandHandlers.contains(cmd)) {
        try {
            return m_commandHandlers[cmd](params);
        } catch (const std::exception& e) {
            response["error"] = QString("Handler error: %1").arg(e.what());
            return response;
        }
    }

    // Built-in commands
    if (cmd == "show_message") {
        QString title = params.value("title", "VioSpice").toString();
        QString text = params.value("text", "").toString();
        QString type = params.value("type", "info").toString();

        if (m_showMessageFn) {
            m_showMessageFn(title, text);
        } else {
            // Fallback: print to debug
            qInfo() << "UICommandServer show_message:" << title << "-" << text;
        }
        response["ok"] = true;
    }
    else if (cmd == "add_menu_item") {
        QString menu = params.value("menu", "Tools").toString();
        QString label = params.value("label", "Menu Item").toString();
        QString id = params.value("id", QString("%1_%2").arg(menu).arg(label)).toString();
        QString code = params.value("code", "").toString();

        MenuItem item;
        item.menu = menu;
        item.label = label;
        item.pythonCode = code;

        if (m_addMenuItemFn) {
            item.action = m_addMenuItemFn(menu, label, [this, id]() {
                onMenuItemTriggered(id);
            });
        }

        m_menuItems[id] = item;
        response["ok"] = true;
        response["id"] = id;
    }
    else if (cmd == "remove_menu_item") {
        QString id = params.value("id").toString();
        if (m_menuItems.contains(id)) {
            MenuItem item = m_menuItems.take(id);
            if (m_removeMenuItemFn) {
                m_removeMenuItemFn(item.menu, item.label);
            }
            if (item.action) {
                item.action->deleteLater();
            }
        }
        response["ok"] = true;
    }
    else if (cmd == "run_python_code") {
        QString code = params.value("code").toString();
        if (m_runPythonCodeFn) {
            response = m_runPythonCodeFn(code);
        } else {
            response["ok"] = false;
            response["error"] = "Python code execution not available";
        }
    }
    else if (cmd == "get_schematic_context") {
        if (m_getSchematicContextFn) {
            response = m_getSchematicContextFn();
            response["ok"] = true;
        } else {
            response["ok"] = true;
            response["has_schematic"] = false;
            response["message"] = "No schematic context available";
        }
    }
    else if (cmd == "ping") {
        response["ok"] = true;
        response["pong"] = true;
        response["server"] = "VioSpice UI Command Server";
    }
    else {
        response["error"] = QString("Unknown command: %1").arg(cmd);
    }

    return response;
}

void UICommandServer::sendResponse(QWebSocket* client, const QVariantMap& response) {
    if (!client || client->state() != QAbstractSocket::ConnectedState) return;

    QJsonDocument doc = QJsonDocument::fromVariant(response);
    client->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void UICommandServer::onMenuItemTriggered(const QString& id) {
    MenuItem item;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_menuItems.contains(id)) return;
        item = m_menuItems[id];
    }

    // Broadcast to all Python clients
    QVariantMap broadcast;
    broadcast["type"] = "menu_item_triggered";
    broadcast["id"] = id;
    broadcast["label"] = item.label;
    broadcast["code"] = item.pythonCode;
    broadcastToClients(broadcast);

    // Emit signal for C++ listeners
    emit menuItemTriggered(id);
}
