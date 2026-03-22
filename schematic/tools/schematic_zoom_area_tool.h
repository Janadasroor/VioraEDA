#ifndef SCHEMATIC_ZOOM_AREA_TOOL_H
#define SCHEMATIC_ZOOM_AREA_TOOL_H

#include "schematic_tool.h"
#include <QRubberBand>

class SchematicZoomAreaTool : public SchematicTool {
    Q_OBJECT

public:
    enum class ZoomMode { ZoomIn, ZoomOut };
    explicit SchematicZoomAreaTool(QObject* parent = nullptr);
    virtual ~SchematicZoomAreaTool();

    virtual void activate(SchematicView* view) override;
    virtual void deactivate() override;

    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    virtual QCursor cursor() const override;
    virtual QString tooltip() const override { return "Zoom Area (Z)"; }

    void setDefaultMode(ZoomMode mode) { m_defaultMode = mode; }
    ZoomMode defaultMode() const { return m_defaultMode; }

private:
    QRubberBand* m_rubberBand;
    QPoint m_origin;
    QCursor m_zoomInCursor;
    QCursor m_zoomOutCursor;
    ZoomMode m_defaultMode = ZoomMode::ZoomIn;
};

#endif // SCHEMATIC_ZOOM_AREA_TOOL_H
