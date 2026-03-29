#ifndef GEMINI_PANEL_H
#define GEMINI_PANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGraphicsScene>
#include <QProcess>
#include <QVariantMap>
#include <functional>
#include <QUrl>
#include <QLabel>
#include <QList>
#include <QSet>

class QQuickWidget;
class GeminiBridge;

/**
 * @brief GeminiPanel now hosts a QML interface via QQuickWidget.
 */
class GeminiPanel : public QWidget {
    Q_OBJECT
public:
    explicit GeminiPanel(QGraphicsScene* scene = nullptr, QWidget* parent = nullptr);

    void setScene(QGraphicsScene* scene) { m_scene = scene; }
    void setNetManager(class NetManager* netManager) { m_netManager = netManager; }
    void setMode(const QString& mode);
    void setProjectFilePath(const QString& path);
    void setUndoStack(class QUndoStack* stack);
    void saveHistory();
    void loadHistory();
    void loadHistoryFromFile(const QString& filePath);
    QString mode() const { return m_mode; }

    void askPrompt(const QString& prompt, bool includeContext = true);
    void setContextProvider(std::function<QString()> provider) { m_contextProvider = provider; }

public slots:
    void clearHistory();

signals:
    void fluxScriptGenerated(const QString& code);
    void symbolJsonGenerated(const QString& json);
    void pythonScriptGenerated(const QString& code);
    void suggestionTriggered(const QString& command);
    void itemsHighlighted(const QStringList& references);
    void snippetGenerated(const QString& jsonSnippet);
    void netlistGenerated(const QString& netlistText);
    void runSimulationRequested();
    void runERCRequested();
    void togglePanelRequested(const QString& panelName);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode);
    void onRefreshModelsClicked();
    void onModelFetchFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCustomInstructionsClicked();
    void onCopyPromptClicked();

    // Bridge Slots (connected to QML signals)
    void onBridgeSendMessage(const QString& text);
    void onBridgeStopRequest();
    void onBridgeRefreshModelsRequest();

private:
    QGraphicsScene* m_scene;
    class NetManager* m_netManager = nullptr;
    class QUndoStack* m_undoStack = nullptr;
    
    // QML Integration
    QQuickWidget* m_quickWidget = nullptr;
    GeminiBridge* m_bridge = nullptr;

    // Pulse animation (handled in QML now, but timer can stay for logic if needed)
    QTimer* m_thinkingPulseTimer;
    int m_pulseStep = 0;

    // Private Process for this panel instance
    QProcess* m_process = nullptr;
    QProcess* m_modelFetchProcess = nullptr;
    QString m_modelFetchStdErr;
    QString m_responseBuffer;
    QString m_thinkingBuffer;
    QString m_errorBuffer;
    QString m_leftover;
    QString m_lastGeneratedCode;
    bool m_isWorking = false;
    QString m_mode = "schematic";
    QString m_projectFilePath;
    QString m_currentChatTitle;
    QString m_lastSubmittedPrompt;
    qint64 m_lastSubmitEpochMs = 0;
    
    QList<QVariantMap> m_history;
    std::function<QString()> m_contextProvider;

    void refreshModelList();
    void reportError(const QString& title, const QString& detailsText, bool openDialog);
    
    void syncHistoryToBridge();

    // Private Process for this panel instance
    QProcess* m_process = nullptr;
    QProcess* m_modelFetchProcess = nullptr;
    QString m_modelFetchStdErr;
    QString m_responseBuffer;
    QString m_thinkingBuffer;
    QString m_errorBuffer;
    QString m_leftover;
    QString m_lastGeneratedCode;
    QString m_lastErrorTitle;
    QString m_lastErrorDetails;
    bool m_isWorking = false;
    QString m_mode = "schematic";
    QString m_projectFilePath;
    QString m_currentChatTitle;
    QString m_lastSubmittedPrompt;
    qint64 m_lastSubmitEpochMs = 0;
    
    QList<QVariantMap> m_history;
    QList<ChatMessage> m_chatMessages;
    std::function<QString()> m_contextProvider;

    void refreshModelList();
    void showErrorBanner(const QString& summaryText, const QString& detailsText = QString());
    void hideErrorBanner();
    void showErrorDialog(const QString& title, const QString& detailsText);
    void reportError(const QString& title, const QString& detailsText, bool openDialog);
    void appendErrorHistory(const QString& title, const QString& detailsText);
    void ensureErrorDialog();
    void populateErrorDialogHistory();
    void selectErrorHistoryRow(int row);
    void showToolCallBanner(const QString& actionText = QString());
    void hideToolCallBanner();
    void appendChatMessage(const ChatMessage& message);
    QString chatMessageToHtml(const ChatMessage& message) const;
    void renderChatMessage(const ChatMessage& message);
    void resizeChatCards();
    void rerenderChatFromModel();
    void appendUserMessageCard(const QString& text, const QString& headerHtml = QString());
    void appendModelMarkdownCard(const QString& markdownText);
    void appendSystemNote(const QString& html);
    void appendSystemAction(const QString& title, const QString& details, const QString& icon = QString());
    void scrollChatToBottom();
    void beginAssistantRunUi();
    void finishAssistantRunUi(int exitCode);
    void handleActionTag(const QString& actionText);
    void handleSuggestionTag(const QString& suggestionText);
    void appendSnippetActionButton(const QString& snippetJson);
    void appendNetlistActionButton(const QString& netlistText);
    void processAgentStdoutChunk(const QString& chunkText);
    void parseAndExecuteCommandModeInput(const QString& input);
    void updateSendEnabled();
    void clearSuggestionButtons();
    void addSuggestionButton(const QString& label, const QString& command);
    void triggerSuggestionCommand(const QString& command);

    QSet<QString> m_suggestionKeys;
    QPoint m_dragStartPosition;
    QString gatherInstructions() const;
    QString m_pressedAnchor;
};

#endif // GEMINI_PANEL_H
