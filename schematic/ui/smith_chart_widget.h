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
    
    struct Trace {
        QString name;
        QVector<std::complex<double>> points;
        QColor color;
        bool visible = true;
    };

    void setSParameters(const QVector<std::complex<double>>& sParams, const QString& name = "S11", const QColor& color = QColor(59, 130, 246));
    void addTrace(const Trace& trace);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    void drawData(QPainter& painter);
    void drawTooltip(QPainter& painter);
    QPointF complexToPoint(const std::complex<double>& gamma);
    std::complex<double> pointToComplex(const QPointF& p);

    QVector<Trace> m_traces;
    QRectF m_chartRect;
    QPointF m_mousePos;
    bool m_showTooltip = false;
};

#endif // SMITH_CHART_WIDGET_H
