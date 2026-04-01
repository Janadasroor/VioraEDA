#ifndef MINI_SCOPE_WIDGET_H
#define MINI_SCOPE_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>

#include <QMap>
#include <QString>

/**
 * @brief A lightweight waveform preview widget for the Logic Editor.
 */
class MiniScopeWidget : public QWidget {
    Q_OBJECT
public:
    explicit MiniScopeWidget(QWidget* parent = nullptr);

    /**
     * @brief Updates the waveform data for multiple traces.
     */
    void setMultiTraceData(const QMap<QString, QVector<QPointF>>& traces);
    void setData(const QVector<QPointF>& points);
    
    /**
     * @brief Snapshot current traces and store in memory.
     */
    void freezeCurrentTraces();
    
    /**
     * @brief Clear all frozen traces.
     */
    void clearMemories();
    
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct TraceData {
        QVector<QPointF> points;
        QColor color;
        double minV = 0.0;
        double maxV = 0.0;
        double rms = 0.0;
        double freq = 0.0;
    };

    void calculateMeasurements(const QString& name, const QVector<QPointF>& points);
    
    QMap<QString, TraceData> m_traces;
    QList<QMap<QString, TraceData>> m_memories;
    double m_globalMinY = -1.0;
    double m_globalMaxY = 1.0;
    double m_minX = 0.0;
    double m_maxX = 0.02;
};

#endif // MINI_SCOPE_WIDGET_H
