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
    if (m_showTooltip || m_highlightIndex >= 0) {
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
        
        // Label on real axis
        double label_x = centerX + radius * ((r - 1.0) / (r + 1.0));
        painter.setPen(Qt::darkGray);
        painter.drawText(QRectF(label_x - 10, centerY + 2, 20, 15), Qt::AlignCenter, QString::number(r));
        painter.setPen(QPen(QColor(200, 200, 200, 100), 1));
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
        
        // Label on perimeter
        double angle = 2.0 * std::atan(x);
        double lx = centerX + radius * std::cos(angle);
        double ly = centerY - radius * std::sin(angle);
        painter.setPen(Qt::darkGray);
        painter.drawText(QRectF(lx + (lx > centerX ? 5 : -25), ly + (ly > centerY ? 5 : -15), 20, 15), 
                         Qt::AlignCenter, (x > 0 ? "+" : "") + QString::number(x) + "j");
        
        // Mirror label for negative reactance
        ly = centerY + radius * std::sin(angle);
        painter.drawText(QRectF(lx + (lx > centerX ? 5 : -25), ly + (ly > centerY ? 5 : -15), 20, 15), 
                         Qt::AlignCenter, "-" + QString::number(x) + "j");
        
        painter.setPen(QPen(QColor(200, 200, 200, 100), 1));
    }

    // Real Axis
    painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
    painter.drawLine(centerX - radius, centerY, centerX + radius, centerY);
}

void SmithChartWidget::setHighlightIndex(int index) {
    if (m_highlightIndex != index) {
        m_highlightIndex = index;
        update();
    }
}

void SmithChartWidget::drawData(QPainter& painter) {
    for (const auto& trace : m_traces) {
        if (!trace.visible || trace.points.isEmpty()) continue;

        painter.setPen(QPen(trace.color, 2));
        QPolygonF poly;
        for (const auto& s : trace.points) {
            poly.append(complexToPoint(s));
        }
        
        if (poly.size() > 1) {
            painter.drawPolyline(poly);
            
            // Markers for start and end
            painter.setBrush(QColor(34, 197, 94)); // Green start
            painter.drawEllipse(poly.first(), 3, 3);
            painter.setBrush(QColor(148, 163, 184)); // Gray end
            painter.drawEllipse(poly.last(), 3, 3);
        } else if (poly.size() == 1) {
            painter.setBrush(trace.color);
            painter.drawEllipse(poly.first(), 4, 4);
        }
        
        // Highlight current point
        if (m_highlightIndex >= 0 && m_highlightIndex < poly.size()) {
            painter.setBrush(Qt::red);
            painter.setPen(QPen(Qt::white, 1.5));
            painter.drawEllipse(poly[m_highlightIndex], 6, 6);
            
            // Add a label for the highlighted point if it's the primary trace (S11)
            if (trace.name == "S11" || m_traces.size() == 1) {
                painter.setPen(Qt::white);
                std::complex<double> val = trace.points[m_highlightIndex];
                QString valStr;
                if (trace.name == "S21" || trace.name == "S12") {
                    double db = 20.0 * std::log10(std::max(std::abs(val), 1e-12));
                    valStr = QString("%1: %2 dB").arg(trace.name).arg(db, 0, 'f', 2);
                } else {
                    valStr = QString("%1: %2%3%4j")
                                        .arg(trace.name)
                                        .arg(val.real(), 0, 'f', 2)
                                        .arg(val.imag() >= 0 ? "+" : "-")
                                        .arg(std::abs(val.imag()), 0, 'f', 2);
                }
                painter.drawText(poly[m_highlightIndex] + QPointF(10, -10), valStr);
            }
        }
    }
}

void SmithChartWidget::drawTooltip(QPainter& painter) {
    QString text;
    std::complex<double> gamma;
    bool foundData = false;

    // If mouse is near a trace point, show that data. Otherwise show mouse coordinate complex value.
    if (m_highlightIndex >= 0) {
        for (const auto& trace : m_traces) {
            if (trace.visible && m_highlightIndex < trace.points.size()) {
                gamma = trace.points[m_highlightIndex];
                QString line = QString("%1: %2 %3 j%4")
                                    .arg(trace.name.leftJustified(3))
                                    .arg(std::abs(gamma), 6, 'f', 3)
                                    .arg(gamma.imag() >= 0 ? "+" : "-")
                                    .arg(std::abs(gamma.imag()), 5, 'f', 3);
                
                if (trace.name == "S21" || trace.name == "S12") {
                    double db = 20.0 * std::log10(std::max(std::abs(gamma), 1e-12));
                    line += QString(" (%1 dB)").arg(db, 0, 'f', 1);
                }
                text += line + "\n";
                foundData = true;
            }
        }
    }

    if (!foundData) {
        gamma = pointToComplex(m_mousePos);
        if (std::abs(gamma) > 2.0) return; // Too far out
    } else {
        // Use the first trace's gamma for Z calculation in tooltip
        gamma = m_traces.first().points[m_highlightIndex];
    }

    // Calculate Z (assuming 50 ohm)
    double z0 = 50.0;
    std::complex<double> denom = (1.0 - gamma);
    std::complex<double> z = (std::abs(denom) < 1e-12) ? std::complex<double>(1e9, 0) : z0 * (1.0 + gamma) / denom;
    double vswr = (std::abs(gamma) < 0.999) ? (1.0 + std::abs(gamma)) / (1.0 - std::abs(gamma)) : 999.0;

    if (!foundData) {
        text = QString("Γ: %1 + j%2\n").arg(gamma.real(), 0, 'f', 3).arg(gamma.imag(), 0, 'f', 3);
    }
    text += QString("Z: %1 + j%2 Ω\nVSWR: %3")
                       .arg(z.real(), 0, 'f', 1)
                       .arg(z.imag(), 0, 'f', 1)
                       .arg(vswr, 0, 'f', 2);

    QFontMetrics fm = painter.fontMetrics();
    QRect textRect = fm.boundingRect(rect(), Qt::AlignLeft, text);
    textRect.adjust(-5, -5, 5, 5);
    
    QPointF pos = m_mousePos;
    if (!m_showTooltip && m_highlightIndex >= 0 && !m_traces.isEmpty()) {
        // Position near the first trace's highlight point
        pos = complexToPoint(m_traces.first().points[m_highlightIndex]);
    }
    
    textRect.moveTo(pos.x() + 15, pos.y() + 15);

    // Keep inside widget
    if (textRect.right() > width()) textRect.moveRight(pos.x() - 15);
    if (textRect.bottom() > height()) textRect.moveBottom(pos.y() - 15);

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
