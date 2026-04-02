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
#include <QColor>

class WaveformExpressionDialog : public QDialog {
    Q_OBJECT
public:
    explicit WaveformExpressionDialog(const QString &netName, const QStringList &availableSignals, const QColor &existingColor = QColor(), const QString &expression = QString(), QWidget *parent = nullptr);
    
    QString expression() const { return m_expressionEdit->text(); }
    QString netName() const { return m_netName; }
    QColor signalColor() const { return m_colorButton->property("color").value<QColor>(); }
    
private Q_SLOTS:
    void onExpressionChanged();
    void insertOperator(const QString &op);
    void onColorClicked();
    
private:
    QLineEdit *m_expressionEdit;
    QLabel *m_previewLabel;
    QLabel *m_netLabel;
    QPushButton *m_colorButton;
    QStringList m_availableSignals;
    QString m_netName;
};

#endif
