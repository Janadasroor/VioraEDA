#include "splash_screen.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include "../core/theme_manager.h"

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 350);

    const PCBTheme* theme = ThemeManager::theme();
    const QString panelBg = theme ? theme->panelBackground().name() : QString("#1a1a1a");
    const QString border = theme ? theme->panelBorder().name() : QString("#333333");
    const QString text = theme ? theme->textColor().name() : QString("#ffffff");
    const QString textSecondary = theme ? theme->textSecondary().name() : QString("#aaaaaa");
    const QString accent = theme ? theme->accentColor().name() : QString("#4CAF50");

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet(QString(
        "#container {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
        "QLabel {"
        "  color: %3;"
        "  background: transparent;"
        "}"
    ).arg(panelBg, border, text));
    mainLayout->addWidget(container);

    auto layout = new QVBoxLayout(container);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(20);

    m_logoLabel = new QLabel(this);
    m_logoLabel->setPixmap(QPixmap(":/icons/viospice_logo.png").scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_logoLabel);

    m_titleLabel = new QLabel("VIOSPICE", this);
    m_titleLabel->setStyleSheet(QString("font-size: 32px; font-weight: bold; letter-spacing: 4px; color: %1; background: transparent;").arg(accent));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_titleLabel);

    auto subtitle = new QLabel("High Performance Circuit Simulator", this);
    subtitle->setStyleSheet(QString("font-size: 14px; color: %1; background: transparent;").arg(textSecondary));
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);

    layout->addStretch();

    m_statusLabel = new QLabel("Initializing...", this);
    m_statusLabel->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent;").arg(textSecondary));
    m_statusLabel->setAlignment(Qt::AlignLeft);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(QString(
        "QProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(border, accent));
    layout->addWidget(m_progressBar);

    // Center on screen
    move(QGuiApplication::primaryScreen()->availableGeometry().center() - rect().center());
}

void SplashScreen::setStatus(const QString& status) {
    m_statusLabel->setText(status);
}

void SplashScreen::setProgress(int value, int total) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(value);
}
