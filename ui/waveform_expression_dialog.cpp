// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Janada Sroor

#include "waveform_expression_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QRegularExpression>

WaveformExpressionDialog::WaveformExpressionDialog(const QString &netName, const QStringList &availableSignals, QWidget *parent)
    : QDialog(parent)
    , m_availableSignals(availableSignals)
{
    setWindowTitle("Waveform Expression");
    setMinimumWidth(450);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    m_netLabel = new QLabel(QString("Net: <b>%1</b>").arg(netName));
    m_netLabel->setStyleSheet("color: #00ff88; font-size: 12px; padding: 5px; background: #1a1a2e; border: 1px solid #333;");
    mainLayout->addWidget(m_netLabel);
    
    QLabel *hintLabel = new QLabel("Use V(net) syntax. Example: V(N001)+V(N002)");
    hintLabel->setStyleSheet("color: #888; font-size: 10px;");
    mainLayout->addWidget(hintLabel);
    
    QHBoxLayout *exprLayout = new QHBoxLayout();
    QLabel *exprLabel = new QLabel("Expression:");
    exprLabel->setStyleSheet("color: #eee;");
    exprLayout->addWidget(exprLabel);
    
    m_expressionEdit = new QLineEdit();
    m_expressionEdit->setPlaceholderText("e.g., V(N001)+V(N002)*2");
    m_expressionEdit->setStyleSheet("QLineEdit { background: #fff; color: #000; border: 1px solid #00ff88; padding: 5px; font-family: 'Courier New'; }");
    m_expressionEdit->setText(netName);
    m_expressionEdit->selectAll();
    exprLayout->addWidget(m_expressionEdit);
    
    mainLayout->addLayout(exprLayout);
    
    QHBoxLayout *opLayout = new QHBoxLayout();
    QLabel *opLabel = new QLabel("Insert:");
    opLabel->setStyleSheet("color: #888;");
    opLayout->addWidget(opLabel);
    
    QStringList ops = {"+", "-", "*", "/", "V(", ")"};
    for (const QString &op : ops) {
        QPushButton *btn = new QPushButton(op);
        btn->setMaximumWidth(40);
        btn->setStyleSheet("QPushButton { background: #2a2a3a; color: #00ff88; border: 1px solid #444; padding: 3px; } QPushButton:hover { background: #3a3a4a; }");
        connect(btn, &QPushButton::clicked, this, [this, op]() { insertOperator(op); });
        opLayout->addWidget(btn);
    }
    
    QComboBox *signalCombo = new QComboBox();
    signalCombo->addItems(m_availableSignals);
    signalCombo->setStyleSheet("QComboBox { background: #1a1a2e; color: #eee; border: 1px solid #444; }");
    signalCombo->setMinimumWidth(150);
    connect(signalCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (!text.isEmpty()) {
            m_expressionEdit->insert(text);
            m_expressionEdit->setFocus();
        }
    });
    opLayout->addWidget(signalCombo);
    
    opLayout->addStretch();
    mainLayout->addLayout(opLayout);
    
    m_previewLabel = new QLabel("Preview:");
    m_previewLabel->setStyleSheet("color: #888; font-size: 10px; font-family: 'Courier New';");
    mainLayout->addWidget(m_previewLabel);
    
    connect(m_expressionEdit, &QLineEdit::textChanged, this, &WaveformExpressionDialog::onExpressionChanged);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Plot");
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet("QPushButton { background: #1a4d1a; color: #00ff00; border: 1px solid #00ff00; padding: 5px 15px; }");
    buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet("QPushButton { background: #2a2a3a; color: #aaa; border: 1px solid #555; padding: 5px 15px; }");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    onExpressionChanged();
}

void WaveformExpressionDialog::onExpressionChanged() {
    QString expr = m_expressionEdit->text();
    m_previewLabel->setText(QString("Expression: <span style='color:#ffff00;'>%1</span>").arg(expr.isEmpty() ? "(empty)" : expr));
    
    QRegularExpression re("V\\(([^)]+)\\)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(expr);
    QStringList found;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        found.append(match.captured(1));
    }
    if (!found.isEmpty()) {
        m_previewLabel->setText(QString("Expression: <span style='color:#ffff00;'>%1</span> <span style='color:#888;'>(signals: %2)</span>")
            .arg(expr).arg(found.join(", ")));
    }
}

void WaveformExpressionDialog::insertOperator(const QString &op) {
    m_expressionEdit->insert(op);
    m_expressionEdit->setFocus();
}
