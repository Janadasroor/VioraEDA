#ifndef SMITH_CHART_WIDGET_H
#define SMITH_CHART_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <complex>

/**
 * @brief A high-performance Smith Chart widget for RF analysis visualization.
 */
class SmithChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit SmithChartWidget(QWidget* parent = nullptr);

    void setData(const QVector<std::complex<double>>& sParams);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    void drawData(QPainter& painter);
    QPointF complexToPoint(const std::complex<double>& gamma);

    QVector<std::complex<double>> m_sParams;
    QRectF m_chartRect;
};

#endif // SMITH_CHART_WIDGET_H
