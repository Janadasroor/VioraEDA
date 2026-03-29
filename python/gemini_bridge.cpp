#include "gemini_bridge.h"
#include "../core/theme_manager.h"
#include <QColor>
#include <QPalette>

GeminiBridge::GeminiBridge(QObject* parent) : QObject(parent) {
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
        emit currentModelChanged();
    }
}

void GeminiBridge::setCurrentMode(const QString& mode) {
    if (m_currentMode != mode) {
        m_currentMode = mode;
        emit currentModeChanged();
    }
}

void GeminiBridge::sendMessage(const QString& text) {
    emit requestSendMessage(text);
}

void GeminiBridge::clearHistory() {
    m_messages.clear();
    emit messagesChanged();
}

void GeminiBridge::stopRun() {
    emit requestStop();
}

void GeminiBridge::refreshModels() {
    emit requestRefreshModels();
}

void GeminiBridge::updateMessages(const QVariantList& msgs) {
    m_messages = msgs;
    emit messagesChanged();
}

void GeminiBridge::updateStatus(bool working, const QString& thinking) {
    m_isWorking = working;
    m_thinkingText = thinking;
    emit isWorkingChanged();
    emit thinkingTextChanged();
}

void GeminiBridge::updateModels(const QStringList& models) {
    m_availableModels = models;
    emit availableModelsChanged();
}
