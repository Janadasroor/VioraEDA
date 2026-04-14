#ifndef UI_COMMAND_SERVER_H
#define UI_COMMAND_SERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QAction>
#include <functional>
#include <mutex>

/**
 * @brief WebSocket server that allows Python scripts to interact with the GUI.
 *
 * Commands supported:
 *   - show_message: Show a message dialog
 *   - add_menu_item: Add a menu item that triggers a Python callback
 *   - remove_menu_item: Remove a previously added menu item
 *   - run_python_code: Execute Python code in the client's context
 *   - get_schematic_context: Get info about the current schematic
 *   - create_dock: Create a dock widget (placeholder for future expansion)
 *
 * Python clients connect via websockets and send JSON commands.
 * The server responds with JSON results.
 */
class UICommandServer : public QObject {
    Q_OBJECT
public:
    static UICommandServer& instance();

    /** Start listening on the given port (default 18790). */
    bool start(int port = 18790);

    /** Stop the server and disconnect all clients. */
    void stop();

    /** Returns true if the server is running. */
    bool isRunning() const;

    /** Get the port the server is listening on. */
    int port() const { return m_port; }

    /** Register a handler for custom commands. */
    using CommandHandler = std::function<QVariantMap(const QVariantMap&)>;
    void registerCommand(const QString& cmd, CommandHandler handler);

    /** Broadcast a message to all connected Python clients. */
    void broadcastToClients(const QVariantMap& message);

Q_SIGNALS:
    /** Emitted when a Python client sends a command. */
    void commandReceived(const QString& command, const QVariantMap& params);

    /** Emitted when a registered menu item is clicked. */
    void menuItemTriggered(const QString& menuItemId);

private Q_SLOTS:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onDisconnected();

private:
    explicit UICommandServer(QObject* parent = nullptr);
    ~UICommandServer();

    QVariantMap handleCommand(const QVariantMap& request);
    void sendResponse(QWebSocket* client, const QVariantMap& response);

    QWebSocketServer* m_server = nullptr;
    QList<QWebSocket*> m_clients;
    int m_port = 18790;
    std::mutex m_mutex;

    // Registered command handlers
    QMap<QString, CommandHandler> m_commandHandlers;

    // Registered menu items: id -> {menu, label, pythonCode}
    struct MenuItem {
        QString menu;
        QString label;
        QString pythonCode;
        QAction* action = nullptr;
    };
    QMap<QString, MenuItem> m_menuItems;

    // Callback for when a menu item is triggered
    void onMenuItemTriggered(const QString& id);

    // UI integration hooks (to be connected by the GUI)
    std::function<void(const QString& title, const QString& text)> m_showMessageFn;
    std::function<QAction*(const QString& menu, const QString& label, std::function<void()>)> m_addMenuItemFn;
    std::function<void(const QString& menu, const QString& label)> m_removeMenuItemFn;
    std::function<QVariantMap()> m_getSchematicContextFn;
    std::function<QVariantMap(const QString& code)> m_runPythonCodeFn;

public:
    // Set UI integration hooks (called by the GUI on startup)
    void setShowMessageFn(std::function<void(const QString&, const QString&)> fn) { m_showMessageFn = std::move(fn); }
    void setAddMenuItemFn(std::function<QAction*(const QString&, const QString&, std::function<void()>)> fn) { m_addMenuItemFn = std::move(fn); }
    void setRemoveMenuItemFn(std::function<void(const QString&, const QString&)> fn) { m_removeMenuItemFn = std::move(fn); }
    void setGetSchematicContextFn(std::function<QVariantMap()> fn) { m_getSchematicContextFn = std::move(fn); }
    void setRunPythonCodeFn(std::function<QVariantMap(const QString&)> fn) { m_runPythonCodeFn = std::move(fn); }
};

#endif // UI_COMMAND_SERVER_H
