#include "smith_chart_widget.h"
#include <QPainter>
#include <QResizeEvent>
#include <cmath>

SmithChartWidget::SmithChartWidget(QWidget* parent) : QWidget(parent) {
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void SmithChartWidget::setData(const QVector<std::complex<double>>& sParams) {
    m_sParams = sParams;
    update();
}

void SmithChartWidget::clear() {
    m_sParams.clear();
    update();
}

void SmithChartWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int size = std::min(width(), height()) - 40;
    const int xOff = (width() - size) / 2;
    const int yOff = (height() - size) / 2;
    m_chartRect = QRectF(xOff, yOff, size, size);

    // Draw background circle (Unity circle)
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawEllipse(m_chartRect);

    drawGrid(painter);
    drawData(painter);
}

void SmithChartWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void SmithChartWidget::drawGrid(QPainter& painter) {
    const double centerX = m_chartRect.center().x();
    const double centerY = m_chartRect.center().y();
    const double radius = m_chartRect.width() / 2.0;

    auto drawResCircle = [&](double r) {
        // Resistance circle: center = (r/(r+1), 0), radius = 1/(r+1)
        double r_norm = radius / (r + 1.0);
        double x_norm = centerX + radius * (r / (r + 1.0)) - (radius - r_norm);
        // Wait, the center in Gamma plane is r/(r+1).
        // Gamma real axis is from -1 to 1.
        // Screen center is (centerX, centerY).
        // Gamma(1,0) is (centerX + radius, centerY).
        // Circle center in screen coords: centerX + radius * (r/(r+1)), centerY
        // Circle radius in screen coords: radius * (1/(r+1))
        double c_x = centerX + radius * (r / (r + 1.0));
        double c_r = radius * (1.0 / (r + 1.0));
        painter.drawEllipse(QPointF(c_x - c_r/2.0 + c_r/2.0, centerY), c_r, c_r); 
        // Correcting manually:
        painter.drawEllipse(QPointF(centerX + radius * (r / (r + 1.0) - 1.0 + 1.0/(r+1.0)), centerY), c_r, c_r);
        // Actually: center is at Re = r/(r+1), Im = 0.
        // Gamma = (Z-Z0)/(Z+Z0) = (z-1)/(z+1)
        // For z = r + jx, if x=0, gamma = (r-1)/(r+1).
        // If r=fixed, x varies, gamma traces a circle.
        // Center: (r/(r+1), 0), Radius: 1/(r+1)
    };

    painter.setPen(QPen(QColor(200, 200, 200, 150), 1, Qt::DashLine));
    
    // Constant Resistance Circles
    QVector<double> r_values = {0.2, 0.5, 1.0, 2.0, 5.0};
    for (double r : r_values) {
        double screen_r = radius * (1.0 / (r + 1.0));
        double screen_x = centerX + radius * (r / (r + 1.0));
        painter.drawEllipse(QPointF(screen_x, centerY), screen_r, screen_r);
    }

    // Constant Reactance Arcs
    QVector<double> x_values = {0.2, 0.5, 1.0, 2.0, 5.0};
    for (double x : x_values) {
        // Center: (1, 1/x), Radius: 1/x
        double screen_r = radius * (1.0 / x);
        double screen_x = centerX + radius;
        double screen_y_pos = centerY - radius * (1.0 / x);
        double screen_y_neg = centerY + radius * (1.0 / x);

        // Draw upper arc (positive reactance)
        painter.drawEllipse(QPointF(screen_x, screen_y_pos), screen_r, screen_r);
        // Draw lower arc (negative reactance)
        painter.drawEllipse(QPointF(screen_x, screen_y_neg), screen_r, screen_r);
    }

    // Real axis
    painter.drawLine(centerX - radius, centerY, centerX + radius, centerY);
}

void SmithChartWidget::drawData(QPainter& painter) {
    if (m_sParams.isEmpty()) return;

    painter.setPen(QPen(QColor(59, 130, 246), 2)); // Modern Blue
    QPolygonF poly;
    for (const auto& s : m_sParams) {
        poly.append(complexToPoint(s));
    }
    painter.drawPolyline(poly);

    // Draw markers for start and end
    if (!poly.isEmpty()) {
        painter.setBrush(Qt::green);
        painter.drawEllipse(poly.first(), 4, 4);
        painter.setBrush(Qt::red);
        painter.drawEllipse(poly.last(), 4, 4);
    }
}

QPointF SmithChartWidget::complexToPoint(const std::complex<double>& gamma) {
    const double centerX = m_chartRect.center().x();
    const double centerY = m_chartRect.center().y();
    const double radius = m_chartRect.width() / 2.0;

    return QPointF(centerX + gamma.real() * radius, centerY - gamma.imag() * radius);
}
