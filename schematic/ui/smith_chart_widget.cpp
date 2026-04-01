#include "smith_chart_widget.h"
#include <QPainter>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <cmath>

SmithChartWidget::SmithChartWidget(QWidget* parent) : QWidget(parent) {
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMouseTracking(true);
}

void SmithChartWidget::setSParameters(const QVector<std::complex<double>>& sParams, const QString& name, const QColor& color) {
    m_traces.clear();
    Trace t;
    t.name = name;
    t.points = sParams;
    t.color = color;
    m_traces.append(t);
    update();
}

void SmithChartWidget::addTrace(const Trace& trace) {
    m_traces.append(trace);
    update();
}

void SmithChartWidget::clear() {
    m_traces.clear();
    update();
}

void SmithChartWidget::mouseMoveEvent(QMouseEvent* event) {
    m_mousePos = event->pos();
    if (m_chartRect.contains(m_mousePos)) {
        m_showTooltip = true;
    } else {
        m_showTooltip = false;
    }
    update();
}

void SmithChartWidget::leaveEvent(QEvent* /*event*/) {
    m_showTooltip = false;
    update();
}

void SmithChartWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int size = std::min(width(), height()) - 60;
    const int xOff = (width() - size) / 2;
    const int yOff = (height() - size) / 2;
    m_chartRect = QRectF(xOff, yOff, size, size);

    // Fade background slightly
    painter.fillRect(rect(), palette().base());

    drawGrid(painter);
    drawData(painter);
    if (m_showTooltip) {
        drawTooltip(painter);
    }
}

void SmithChartWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void SmithChartWidget::drawGrid(QPainter& painter) {
    const double centerX = m_chartRect.center().x();
    const double centerY = m_chartRect.center().y();
    const double radius = m_chartRect.width() / 2.0;

    // Draw Unity circle
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.drawEllipse(m_chartRect);

    // Constant Resistance Circles
    painter.setPen(QPen(QColor(200, 200, 200, 100), 1));
    QVector<double> r_values = {0.2, 0.5, 1.0, 2.0, 5.0};
    for (double r : r_values) {
        double screen_r = radius / (r + 1.0);
        double screen_x = centerX + radius * (r / (r + 1.0));
        painter.drawEllipse(QPointF(screen_x, centerY), screen_r, screen_r);
    }

    // Constant Reactance Arcs
    QVector<double> x_values = {0.2, 0.5, 1.0, 2.0, 5.0};
    for (double x : x_values) {
        double screen_r = radius / x;
        double screen_x = centerX + radius;
        double screen_y_pos = centerY - radius / x;
        double screen_y_neg = centerY + radius / x;

        // Clip to Unity circle
        painter.save();
        QRegion clipRegion(m_chartRect.toRect(), QRegion::Ellipse);
        painter.setClipRegion(clipRegion);
        painter.drawEllipse(QPointF(screen_x, screen_y_pos), screen_r, screen_r);
        painter.drawEllipse(QPointF(screen_x, screen_y_neg), screen_r, screen_r);
        painter.restore();
    }

    // Real Axis
    painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
    painter.drawLine(centerX - radius, centerY, centerX + radius, centerY);
}

void SmithChartWidget::drawData(QPainter& painter) {
    for (const auto& trace : m_traces) {
        if (!trace.visible || trace.points.isEmpty()) continue;

        painter.setPen(QPen(trace.color, 2));
        QPolygonF poly;
        for (const auto& s : trace.points) {
            poly.append(complexToPoint(s));
        }
        painter.drawPolyline(poly);

        // Markers
        if (!poly.isEmpty()) {
            painter.setBrush(Qt::green);
            painter.drawEllipse(poly.first(), 3, 3);
            painter.setBrush(Qt::red);
            painter.drawEllipse(poly.last(), 3, 3);
        }
    }
}

void SmithChartWidget::drawTooltip(QPainter& painter) {
    std::complex<double> gamma = pointToComplex(m_mousePos);
    if (std::abs(gamma) > 1.05) return;

    // Calculate Z (assuming 50 ohm)
    double z0 = 50.0;
    std::complex<double> z = z0 * (1.0 + gamma) / (1.0 - gamma);
    double vswr = (1.0 + std::abs(gamma)) / (1.0 - std::abs(gamma));

    QString text = QString("Γ: %1 + j%2\nZ: %3 + j%4 Ω\nVSWR: %5")
                       .arg(gamma.real(), 0, 'f', 3)
                       .arg(gamma.imag(), 0, 'f', 3)
                       .arg(z.real(), 0, 'f', 1)
                       .arg(z.imag(), 0, 'f', 1)
                       .arg(vswr, 0, 'f', 2);

    QFontMetrics fm = painter.fontMetrics();
    QRect textRect = fm.boundingRect(rect(), Qt::AlignLeft, text);
    textRect.adjust(-5, -5, 5, 5);
    textRect.moveTo(m_mousePos.x() + 15, m_mousePos.y() + 15);

    // Keep inside widget
    if (textRect.right() > width()) textRect.moveRight(m_mousePos.x() - 15);
    if (textRect.bottom() > height()) textRect.moveBottom(m_mousePos.y() - 15);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRoundedRect(textRect, 4, 4);

    painter.setPen(Qt::white);
    painter.drawText(textRect.adjusted(5, 5, -5, -5), Qt::AlignLeft, text);
}

QPointF SmithChartWidget::complexToPoint(const std::complex<double>& gamma) {
    const double centerX = m_chartRect.center().x();
    const double centerY = m_chartRect.center().y();
    const double radius = m_chartRect.width() / 2.0;

    return QPointF(centerX + gamma.real() * radius, centerY - gamma.imag() * radius);
}

std::complex<double> SmithChartWidget::pointToComplex(const QPointF& p) {
    const double centerX = m_chartRect.center().x();
    const double centerY = m_chartRect.center().y();
    const double radius = m_chartRect.width() / 2.0;

    if (radius == 0) return {0, 0};
    double re = (p.x() - centerX) / radius;
    double im = (centerY - p.y()) / radius;
    return {re, im};
}
