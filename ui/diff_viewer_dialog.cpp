#include "diff_viewer_dialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QScrollBar>

DiffViewerDialog::DiffViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    applyTheme();
}

void DiffViewerDialog::setupUi() {
    setWindowTitle("Diff Viewer");
    resize(800, 500);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_fileLabel = new QLabel(this);
    m_fileLabel->setStyleSheet("font-weight: bold; font-size: 12px; padding: 4px;");
    layout->addWidget(m_fileLabel);

    m_diffView = new QTextEdit(this);
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Monospace", 10));
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(m_diffView, 1);

    QPushButton* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
}

void DiffViewerDialog::setDiff(const QString& diff, const QString& filePath) {
    if (!filePath.isEmpty()) {
        m_fileLabel->setText("Diff: " + filePath);
        setWindowTitle("Diff - " + filePath);
    } else {
        m_fileLabel->setText("Diff");
    }

    // Colorize diff output
    QString html;
    for (const QString& line : diff.split('\n')) {
        QString escaped = line.toHtmlEscaped();
        if (line.startsWith('+') && !line.startsWith("+++")) {
            html += "<span style='color:#22863a;background:#e6ffed;'>" + escaped + "</span>\n";
        } else if (line.startsWith('-') && !line.startsWith("---")) {
            html += "<span style='color:#cb2431;background:#ffeef0;'>" + escaped + "</span>\n";
        } else if (line.startsWith("@@")) {
            html += "<span style='color:#6f42c1;'>" + escaped + "</span>\n";
        } else if (line.startsWith("diff --git") || line.startsWith("index ")) {
            html += "<span style='color:#0366d6;font-weight:bold;'>" + escaped + "</span>\n";
        } else {
            html += escaped + "\n";
        }
    }

    m_diffView->setHtml("<pre style='font-family:monospace;font-size:12px;'>" + html + "</pre>");
}

void DiffViewerDialog::applyTheme() {
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #d4d4d4; }"
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; selection-background-color: #264f78; }"
        "QPushButton { background-color: #0e639c; color: white; border: none; padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1177bb; }"
        "QLabel { color: #cccccc; }"
    );
}
