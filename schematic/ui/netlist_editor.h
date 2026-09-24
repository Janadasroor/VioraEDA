/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETLIST_EDITOR_H
#define NETLIST_EDITOR_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QTemporaryFile>
#include "spice_highlighter.h"

class NetlistEditor : public QWidget {
    Q_OBJECT
public:
    explicit NetlistEditor(QWidget* parent = nullptr);
    ~NetlistEditor();

    void setNetlist(const QString& netlist);
    void loadFile(const QString& path);
    QString netlist() const;
    void applyTheme();
    // Runs the current editor content (used by the global Run action when
    // this tab is active).
    void runActiveNetlist();

Q_SIGNALS:
    // Emitted when a netlist run is launched so the owning editor can reset
    // result views (otherwise they keep showing the previous tab's data).
    void runStarted(const QString& source);

private Q_SLOTS:
    void onRun();
    void onClearLog();
    void onSaveAs();
    void onOutputReceived(const QString& msg);
    void onSimulationFinished();

private:
    void setupUI();

    QPlainTextEdit* m_editor;
    QPlainTextEdit* m_logArea;
    SpiceHighlighter* m_highlighter;
    SpiceHighlighter* m_logHighlighter;
    QToolBar* m_toolbar;
    QTemporaryFile* m_activeTempFile;
    
    QString m_currentFilePath;
};

#endif // NETLIST_EDITOR_H
