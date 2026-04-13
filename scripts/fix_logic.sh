#!/bin/bash

# Remove py:: from logic_editor_panel.cpp

sed -i '/void LogicEditorPanel::runTests()/,/^}/c\
void LogicEditorPanel::runTests() {\
    if (!m_targetBlock) return;\
    m_console->append("<br><font color=\x27#007acc\x27>[Tester] Starting Test Suite...</font>");\
    QString code = m_editor->toPlainText();\
    QMap<int, QString> errors;\
    if (!Flux::JITContextManager::instance().compileAndLoad(code, errors)) {\
        m_console->append("<font color=\x27#f44747\x27><b>Linter:</b> Code has syntax errors, skipping tests.</font>");\
        return;\
    }\
    m_console->append("<font color=\x27#f44747\x27><b>Tester:</b> Automated testing not yet migrated to new JIT engine.</font>");\
}' /home/jnd/qt_projects/viospice/schematic/ui/logic_editor_panel.cpp

sed -i '/void LogicEditorPanel::updatePreview()/,/^}/c\
void LogicEditorPanel::updatePreview() {\
    QString code = m_editor->toPlainText();\
    if (code.trimmed().isEmpty()) {\
        m_scope->clear();\
        m_editor->setErrorLines({});\
        return;\
    }\
\
    if (m_targetBlock && m_targetBlock->engineType() == SmartSignalItem::FluxScript) {\
        m_console->clear();\
        m_console->append("<font color=\x27#569cd6\x27>[FluxScript] Starting JIT-optimized preview simulation...</font>");\
        \
        QElapsedTimer timer;\
        timer.start();\
        \
        QMap<int, QString> errors;\
        if (Flux::JITContextManager::instance().compileAndLoad(code, errors)) {\
            qint64 elapsed = timer.elapsed();\
            m_console->append("<font color=\x27#4ec9b0\x27>[JIT] Compilation successful in " + QString::number(elapsed) + "ms.</font>");\
            m_statusLabel->setText("JIT Ready. Compilation: " + QString::number(elapsed) + "ms");\
        } else {\
            m_console->append("<font color=\x27#f44747\x27>[JIT ERROR] Compilation failed.</font>");\
            m_editor->setErrorLines(errors);\
        }\
    }\
}' /home/jnd/qt_projects/viospice/schematic/ui/logic_editor_panel.cpp

