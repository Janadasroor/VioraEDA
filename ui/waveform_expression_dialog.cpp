// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Janada Sroor

#include "waveform_expression_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QColorDialog>

WaveformExpressionDialog::WaveformExpressionDialog(const QString &netName, const QStringList &availableSignals, const QColor &existingColor, const QString &expression, QWidget *parent)
    : QDialog(parent)
    , m_availableSignals(availableSignals)
    , m_netName(netName)
{
    setWindowTitle("Waveform Expression");
    setMinimumWidth(480);
    setStyleSheet(R"(
        QDialog { background: #f8fafc; }
        QLabel { color: #111827; }
        QLineEdit {
            background: #ffffff;
            color: #111827;
            border: 1px solid #cbd5f5;
            border-radius: 6px;
            padding: 6px 8px;
            font-family: 'Courier New';
        }
        QLineEdit:focus { border: 1px solid #2563eb; }
        QPushButton {
            background: #f1f5f9;
            color: #0f172a;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 4px 10px;
        }
        QPushButton:hover { background: #e2e8f0; }
        QComboBox {
            background: #ffffff;
            color: #0f172a;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 4px 8px;
        }
    )");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    m_netLabel = new QLabel(QString("Net: <b>%1</b>").arg(netName));
    m_netLabel->setStyleSheet("color: #0f172a; font-size: 12px; padding: 6px 8px; background: #eef2ff; border: 1px solid #c7d2fe; border-radius: 6px;");
    mainLayout->addWidget(m_netLabel);
    
    QLabel *hintLabel = new QLabel("Use V/I/P(net) syntax. Functions: derivative(), integral(). Example: V(N001)*2+derivative(V(N002))");
    hintLabel->setStyleSheet("color: #6b7280; font-size: 10px;");
    mainLayout->addWidget(hintLabel);
    
    QHBoxLayout *exprLayout = new QHBoxLayout();
    QLabel *exprLabel = new QLabel("Expression:");
    exprLabel->setStyleSheet("color: #374151;");
    exprLayout->addWidget(exprLabel);
    
    m_expressionEdit = new QLineEdit();
    m_expressionEdit->setPlaceholderText("e.g., V(N001)+V(N002)*2");
    m_expressionEdit->setText(expression.isEmpty() ? QString("V(%1)").arg(netName) : expression);
    m_expressionEdit->selectAll();
    exprLayout->addWidget(m_expressionEdit);
    
    mainLayout->addLayout(exprLayout);
    
    QHBoxLayout *opLayout = new QHBoxLayout();
    QLabel *opLabel = new QLabel("Insert:");
    opLabel->setStyleSheet("color: #6b7280;");
    opLayout->addWidget(opLabel);
    
    QStringList ops = {"+", "-", "*", "/", "V(", ")"};
    for (const QString &op : ops) {
        QPushButton *btn = new QPushButton(op);
        btn->setMaximumWidth(40);
        connect(btn, &QPushButton::clicked, this, [this, op]() { insertOperator(op); });
        opLayout->addWidget(btn);
    }
    
    QComboBox *signalCombo = new QComboBox();
    signalCombo->addItems(m_availableSignals);
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
    
    QHBoxLayout *colorLayout = new QHBoxLayout();
    QLabel *colorLabel = new QLabel("Color:");
    colorLabel->setStyleSheet("color: #374151;");
    colorLayout->addWidget(colorLabel);
    
    QColor defaultColor = existingColor.isValid() ? existingColor : QColor(0, 120, 215);
    m_colorButton = new QPushButton();
    m_colorButton->setProperty("color", defaultColor);
    m_colorButton->setFixedSize(40, 24);
    m_colorButton->setStyleSheet(QString("background-color: %1; border: 1px solid #94a3b8; border-radius: 4px;").arg(defaultColor.name()));
    connect(m_colorButton, &QPushButton::clicked, this, &WaveformExpressionDialog::onColorClicked);
    colorLayout->addWidget(m_colorButton);
    colorLayout->addStretch();
    mainLayout->addLayout(colorLayout);
    
    QHBoxLayout *scaleLayout = new QHBoxLayout();
    QLabel *scaleLabel = new QLabel("Scale:");
    scaleLabel->setStyleSheet("color: #374151;");
    scaleLayout->addWidget(scaleLabel);
    
    QMap<QString, QString> scaleOps = {
        {"+2", "+2"}, {"-2", "-2"}, {"*2", "*2"}, {"/2", "/2"}
    };
    for (auto it = scaleOps.begin(); it != scaleOps.end(); ++it) {
        QPushButton *btn = new QPushButton(it.key());
        btn->setMaximumWidth(50);
        btn->setStyleSheet("QPushButton { background: #f1f5f9; color: #374151; border: 1px solid #cbd5e1; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background: #e2e8f0; }");
        connect(btn, &QPushButton::clicked, this, [this, op = it.value()]() {
            QString expr = m_expressionEdit->text();
            if (expr.isEmpty()) return;
            if (expr.endsWith(")")) {
                int parenCount = 0;
                int i = expr.length() - 1;
                while (i >= 0) {
                    if (expr[i] == ')') parenCount++;
                    else if (expr[i] == '(') parenCount--;
                    if (parenCount == 0 && i > 0) {
                        expr.insert(i, op);
                        m_expressionEdit->setText(expr);
                        m_expressionEdit->setCursorPosition(i + op.length());
                        return;
                    }
                    i--;
                }
            }
            m_expressionEdit->setText(expr + op);
        });
        scaleLayout->addWidget(btn);
    }
    
    QPushButton *derivBtn = new QPushButton("d/dt");
    derivBtn->setMaximumWidth(50);
    derivBtn->setStyleSheet("QPushButton { background: #fef3c7; color: #92400e; border: 1px solid #fcd34d; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background: #fde68a; }");
    connect(derivBtn, &QPushButton::clicked, this, [this]() {
        QString sel = m_expressionEdit->selectedText();
        if (!sel.isEmpty()) {
            m_expressionEdit->insert(QString("derivative(%1)").arg(sel));
        } else {
            QString text = m_expressionEdit->text();
            if (!text.isEmpty()) {
                m_expressionEdit->setText(QString("derivative(%1)").arg(text));
            }
        }
    });
    scaleLayout->addWidget(derivBtn);
    
    QPushButton *integBtn = new QPushButton("\u222b dt");
    integBtn->setMaximumWidth(50);
    integBtn->setStyleSheet("QPushButton { background: #dbeafe; color: #1e40af; border: 1px solid #93c5fd; border-radius: 4px; padding: 4px 8px; } QPushButton:hover { background: #bfdbfe; }");
    connect(integBtn, &QPushButton::clicked, this, [this]() {
        QString sel = m_expressionEdit->selectedText();
        if (!sel.isEmpty()) {
            m_expressionEdit->insert(QString("integral(%1)").arg(sel));
        } else {
            QString text = m_expressionEdit->text();
            if (!text.isEmpty()) {
                m_expressionEdit->setText(QString("integral(%1)").arg(text));
            }
        }
    });
    scaleLayout->addWidget(integBtn);
    
    scaleLayout->addStretch();
    mainLayout->addLayout(scaleLayout);
    
    m_previewLabel = new QLabel("Preview:");
    m_previewLabel->setStyleSheet("color: #6b7280; font-size: 10px; font-family: 'Courier New';");
    mainLayout->addWidget(m_previewLabel);
    
    connect(m_expressionEdit, &QLineEdit::textChanged, this, &WaveformExpressionDialog::onExpressionChanged);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Plot");
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet("QPushButton { background: #2563eb; color: #ffffff; border: 1px solid #1d4ed8; padding: 6px 16px; border-radius: 6px; } QPushButton:hover { background: #1d4ed8; }");
    buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet("QPushButton { background: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; padding: 6px 16px; border-radius: 6px; } QPushButton:hover { background: #f1f5f9; }");
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
    
    m_netLabel->setText(QString("Net: <b>%1</b>").arg(expr.isEmpty() ? m_netName : expr));
}

void WaveformExpressionDialog::insertOperator(const QString &op) {
    m_expressionEdit->insert(op);
    m_expressionEdit->setFocus();
}

void WaveformExpressionDialog::onColorClicked() {
    QColor currentColor = m_colorButton->property("color").value<QColor>();
    QColor newColor = QColorDialog::getColor(currentColor, this, "Select Signal Color");
    if (newColor.isValid()) {
        m_colorButton->setProperty("color", newColor);
        m_colorButton->setStyleSheet(QString("background-color: %1; border: 1px solid #94a3b8; border-radius: 4px;").arg(newColor.name()));
    }
}
