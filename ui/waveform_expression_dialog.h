// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Janada Sroor

#ifndef WAVEFORM_EXPRESSION_DIALOG_H
#define WAVEFORM_EXPRESSION_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <QComboBox>

class WaveformExpressionDialog : public QDialog {
    Q_OBJECT
public:
    explicit WaveformExpressionDialog(const QString &netName, const QStringList &availableSignals, QWidget *parent = nullptr);
    
    QString expression() const { return m_expressionEdit->text(); }
    
private slots:
    void onExpressionChanged();
    void insertOperator(const QString &op);
    
private:
    QLineEdit *m_expressionEdit;
    QLabel *m_previewLabel;
    QLabel *m_netLabel;
    QStringList m_availableSignals;
};

#endif
