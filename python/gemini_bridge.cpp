#include "gemini_bridge.h"
#include "../core/theme_manager.h"
#include "../core/config_manager.h"
#include <QColor>
#include <QPalette>
#include <QDebug>
#include <QClipboard>
#include <QGuiApplication>

GeminiBridge::GeminiBridge(QObject* parent) : QObject(parent) {
    m_currentModel = ConfigManager::instance().geminiSelectedModel();
    m_currentMode = ConfigManager::instance().geminiSelectedMode();
    m_availableModels << m_currentModel;
}

QString GeminiBridge::textColor() const {
    PCBTheme* theme = ThemeManager::theme();
    return theme ? theme->textColor().name() : "#000000";
}

QString GeminiBridge::secondaryColor() const {
    PCBTheme* theme = ThemeManager::theme();
    return theme ? theme->textSecondary().name() : "#888888";
}

QString GeminiBridge::accentColor() const {
    PCBTheme* theme = ThemeManager::theme();
    return theme ? theme->accentColor().name() : "#3b82f6";
}

QString GeminiBridge::backgroundColor() const {
    PCBTheme* theme = ThemeManager::theme();
    return theme ? theme->windowBackground().name() : "#ffffff";
}

QString GeminiBridge::glassBackground() const {
    PCBTheme* theme = ThemeManager::theme();
    if (!theme) return "rgba(255, 255, 255, 0.95)";
    
    if (theme->type() == PCBTheme::Light) {
        return "rgba(255, 255, 255, 0.95)";
    } else {
        return "rgba(13, 17, 23, 0.92)";
    }
}

void GeminiBridge::setCurrentModel(const QString& model) {
    if (m_currentModel != model) {
        m_currentModel = model;
        ConfigManager::instance().setGeminiSelectedModel(m_currentModel);
        emit currentModelChanged();
    }
}

void GeminiBridge::setCurrentMode(const QString& mode) {
    if (m_currentMode != mode) {
        m_currentMode = mode;
        ConfigManager::instance().setGeminiSelectedMode(m_currentMode);
        emit currentModeChanged();
    }
}

void GeminiBridge::sendMessage(const QString& text) {
    qDebug() << "[GeminiBridge] sendMessage:" << text;
    emit sendMessageRequested(text);
}

void GeminiBridge::clearHistory() {
    emit clearHistoryRequested();
}

void GeminiBridge::stopRun() {
    emit stopRequested();
}

void GeminiBridge::refreshModels() {
    emit refreshModelsRequested();
}

void GeminiBridge::updateMessages(const QVariantList& msgs) {
    m_messages = msgs;
    emit messagesChanged();
}

void GeminiBridge::setWorking(bool working, const QString& thinking) {
    if (m_isWorking != working) {
        m_isWorking = working;
        emit isWorkingChanged();
    }
    if (!thinking.isEmpty()) {
        updateStatus(thinking);
    }
}

void GeminiBridge::updateStatus(const QString& status) {
    if (m_thinkingText != status) {
        m_thinkingText = status;
        
        // Parse "Tool: Action" format
        int colonIdx = status.indexOf(':');
        if (colonIdx != -1) {
            m_currentTool = status.left(colonIdx).trimmed();
            m_currentAction = status.mid(colonIdx + 1).trimmed();
        } else {
            m_currentTool = "ViorAI";
            m_currentAction = status.trimmed();
        }

        emit thinkingTextChanged();
        emit currentToolChanged();
        emit currentActionChanged();
    }
}

void GeminiBridge::updateAvailableModels(const QStringList& models) {
    if (m_availableModels != models) {
        m_availableModels = models;
        emit availableModelsChanged();
    }
}

void GeminiBridge::updateTitle(const QString& title) {
    if (m_conversationTitle != title) {
        m_conversationTitle = title;
        emit conversationTitleChanged();
    }
}

void GeminiBridge::closePanel() {
    emit closeRequested();
}

void GeminiBridge::showHistory() {
    emit showHistoryRequested();
}

void GeminiBridge::startNewChat() {
    emit startNewChatRequested();
}
void GeminiBridge::setTokenCount(int count) {
    if (m_tokenCount != count) {
        m_tokenCount = count;
        emit tokenCountChanged();
    }
}

void GeminiBridge::setUsagePercentage(double percentage) {
    if (m_usagePercentage != percentage) {
        m_usagePercentage = percentage;
        emit usagePercentageChanged();
    }
}

void GeminiBridge::copyToClipboard(const QString& text) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

void GeminiBridge::showInstructions() {
    emit showInstructionsRequested();
}

void GeminiBridge::setZoomFactor(double zoom) {
    // Keep zoom between 0.5 and 3.0
    double boundedZoom = qBound(0.5, zoom, 3.0);
    if (m_zoomFactor != boundedZoom) {
        m_zoomFactor = boundedZoom;
        emit zoomFactorChanged();
    }
}

void GeminiBridge::zoomIn() {
    setZoomFactor(m_zoomFactor + 0.1);
}

void GeminiBridge::zoomOut() {
    setZoomFactor(m_zoomFactor - 0.1);
}

void GeminiBridge::resetZoom() {
    setZoomFactor(1.0);
}

void GeminiBridge::exportChat() {
    emit exportRequested();
}

void GeminiBridge::addToolCall(const QVariantMap& call) {
    m_toolCalls.append(call);
    emit toolCallsChanged();
}

void GeminiBridge::updateToolResult(const QString& toolName, const QVariantMap& result) {
    // Find the LAST tool with this name that is still 'pending' or 'running'
    for (int i = m_toolCalls.size() - 1; i >= 0; --i) {
        QVariantMap tool = m_toolCalls[i].toMap();
        if (tool["name"].toString() == toolName) {
            // Update the tool entry
            for (auto it = result.begin(); it != result.end(); ++it) {
                tool[it.key()] = it.value();
            }
            tool["status"] = "success";
            m_toolCalls[i] = tool;
            emit toolCallsChanged();
            return;
        }
    }
}

void GeminiBridge::clearToolCalls() {
    if (!m_toolCalls.isEmpty()) {
        m_toolCalls.clear();
        emit toolCallsChanged();
    }
}
