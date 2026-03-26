#include "symbol_editor_view.h"
#include "theme_manager.h"
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QKeyEvent>
#include <QGraphicsItem>
#include <QPen>
#include <cmath>

namespace {
QGraphicsItem* findResizeHandleItem(QGraphicsItem* item) {
    while (item) {
        if (item->data(0).toString() == "resize_handle") return item;
        item = item->parentItem();
    }
    return nullptr;
}
}

SymbolEditorView::SymbolEditorView(QWidget* parent)
    : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setMouseTracking(true);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFocusPolicy(Qt::StrongFocus);
    setInteractive(true);
    
    // Require items to be fully contained in the rubber band to be selected.
    // This allows selecting tiny items inside larger ones.
    setRubberBandSelectionMode(Qt::ContainsItemShape);

    PCBTheme* theme = ThemeManager::theme();
    setBackgroundBrush(QBrush(theme ? theme->canvasBackground() : QColor(30, 30, 30)));
    // Start slightly zoomed-out, but close to normal editing scale.
    scale(0.8, 0.8);
}

void SymbolEditorView::setCurrentTool(int tool) {
    m_currentTool = tool;
    
    // Tools: Select=0, Line=1, Rect=2, Circle=3, Arc=4, Text=5, Pin=6, Polygon=7, Erase=8, ZoomArea=9, Anchor=10, Bezier=11, Image=12, Pen=13
    if (tool == 6) {
        // Pin tool: specialized crosshair for precision
        setCursor(Qt::CrossCursor);
    } else if ((tool >= 1 && tool <= 5) || tool == 7 || tool == 11 || tool == 12) {
        // General drawing tools: use pencil cursor
        QPixmap pixmap(":/icons/cursor_pencil.svg");
        setCursor(QCursor(pixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation), 0, 23));
    } else if (tool == 8) {
        // Erase tool: use eraser cursor
        QPixmap pixmap(":/icons/cursor_eraser.svg");
        setCursor(QCursor(pixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation), 7, 19));
    } else if (tool == 13) {
        // Pen tool: Figma-style pen cursor
        QPixmap pixmap(":/icons/tool_pen.svg");
        setCursor(QCursor(pixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation), 2, 2));
    } else {
        // Default / Select / Zoom / Anchor
        setCursor(Qt::ArrowCursor);
    }
}

void SymbolEditorView::drawBackground(QPainter* painter, const QRectF& rect) {
    PCBTheme* theme = ThemeManager::theme();
    QColor bgColor = theme ? theme->canvasBackground() : QColor(30, 30, 30);
    painter->fillRect(rect, bgColor);
    
    // Safety check: if grid is too small or viewport is too large, skip fine grid
    qreal fineGrid = m_gridSize;
    if (fineGrid < 0.5) fineGrid = 10.0; // Default fallback if corrupted
    
    // Determine how many lines would be drawn
    qreal horizontalLines = rect.height() / fineGrid;
    qreal verticalLines = rect.width() / fineGrid;
    
    // If we'd draw more than 500 lines, only draw major grid to prevent hang
    bool drawFine = (horizontalLines < 500 && verticalLines < 500);
    qreal majorGrid = fineGrid * 10;
    
    qreal startX = std::floor(rect.left() / (drawFine ? fineGrid : majorGrid)) * (drawFine ? fineGrid : majorGrid);
    qreal startY = std::floor(rect.top() / (drawFine ? fineGrid : majorGrid)) * (drawFine ? fineGrid : majorGrid);
    
    // Detect dark vs light theme by canvas luminance
    bool isDark = bgColor.lightnessF() < 0.5;

    QColor subColor = theme ? theme->gridSecondary() : QColor(120, 120, 130);
    QColor mainColor = theme ? theme->gridPrimary() : QColor(180, 180, 190);
    if (isDark) {
        // Stronger contrast for dark canvas
        subColor = QColor(100, 108, 122);
        mainColor = QColor(150, 162, 182);
    } else {
        // Darker grid tones for light canvas
        subColor = QColor(170, 178, 192);
        mainColor = QColor(118, 128, 146);
    }
    subColor.setAlpha(isDark ? 235 : 225);
    mainColor.setAlpha(isDark ? 255 : 245);

    QColor dotColor = mainColor;
    dotColor.setAlpha(isDark ? 255 : 255);

    QPen minorPen(subColor, 0.0);
    QPen majorPen(mainColor, 0.0);
    minorPen.setCosmetic(true);
    majorPen.setCosmetic(true);

    // ---- Minor grid lines ----
    if (drawFine) {
        painter->setPen(minorPen);
        for (qreal y = startY; y < rect.bottom(); y += fineGrid) {
            if (!qFuzzyIsNull(std::fmod(std::abs(y), majorGrid)))
                painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
        }
        for (qreal x = startX; x < rect.right(); x += fineGrid) {
            if (!qFuzzyIsNull(std::fmod(std::abs(x), majorGrid)))
                painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
        }
    }

    // ---- Major grid lines (thicker, fully opaque) ----
    QPen majorThickPen(mainColor, 0.0);
    majorThickPen.setCosmetic(true);
    painter->setPen(majorThickPen);
    qreal majorStartX = std::floor(rect.left() / majorGrid) * majorGrid;
    qreal majorStartY = std::floor(rect.top()  / majorGrid) * majorGrid;
    for (qreal y = majorStartY; y < rect.bottom(); y += majorGrid)
        painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
    for (qreal x = majorStartX; x < rect.right(); x += majorGrid)
        painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));

    // ---- Dots at every grid intersection ----
    {
        QPen dotPen(dotColor, 0);
        dotPen.setCosmetic(true);
        painter->setPen(dotPen);
        painter->setBrush(Qt::NoBrush);

        const qreal viewScale = std::abs(transform().m11());
        const qreal gridPixelSpacing = fineGrid * viewScale;

        if (gridPixelSpacing >= 10.0) {
            // Draw dots at minor intersections
            painter->setPen(QPen(subColor, 2.5, Qt::SolidLine, Qt::RoundCap));
            for (qreal x = startX; x < rect.right(); x += fineGrid) {
                if (qFuzzyIsNull(std::fmod(std::abs(x), majorGrid))) continue;
                for (qreal y = startY; y < rect.bottom(); y += fineGrid) {
                    if (qFuzzyIsNull(std::fmod(std::abs(y), majorGrid))) continue;
                    painter->drawPoint(QPointF(x, y));
                }
            }
            // Larger dots at major intersections
            painter->setPen(QPen(mainColor, 4.0, Qt::SolidLine, Qt::RoundCap));
            for (qreal x = majorStartX; x < rect.right(); x += majorGrid) {
                for (qreal y = majorStartY; y < rect.bottom(); y += majorGrid) {
                    painter->drawPoint(QPointF(x, y));
                }
            }
        }
    }

    // ---- Origin crosshair (bright, visible) ----
    QColor originColor = theme ? theme->accentColor() : QColor(0, 200, 255);
    originColor.setAlpha(isDark ? 220 : 200);
    QPen originPen(originColor, 0);
    originPen.setCosmetic(true);
    painter->setPen(originPen);

    qreal crossSize = majorGrid * 0.75;
    painter->drawLine(QLineF(-crossSize, 0, crossSize, 0));
    painter->drawLine(QLineF(0, -crossSize, 0, crossSize));

    // Circle at origin
    QColor originFill = originColor;
    originFill.setAlpha(isDark ? 120 : 80);
    painter->setBrush(originFill);
    painter->drawEllipse(QPointF(0, 0), fineGrid * 0.75, fineGrid * 0.75);
}

QPointF SymbolEditorView::snapToGrid(QPointF pos) const {
    if (!m_snapToGrid) return pos;
    return QPointF(std::round(pos.x() / m_gridSize) * m_gridSize,
                   std::round(pos.y() / m_gridSize) * m_gridSize);
}

void SymbolEditorView::mousePressEvent(QMouseEvent* event) {
     if (event->button() == Qt::MiddleButton) {
         m_isPanning    = true;
         m_lastPanPoint = event->pos();
         setCursor(Qt::ClosedHandCursor);
         event->accept();
         return;
     }

     if (event->button() == Qt::RightButton) {
         emit rightClicked();
         event->accept();
         return;
     }

      if (event->button() == Qt::LeftButton) {
          if (m_currentTool == 0) {
              QGraphicsItem* hit = itemAt(event->pos());
              if (QGraphicsItem* handle = findResizeHandleItem(hit)) {
                  m_rectResizeActive = true;
                  emit rectResizeStarted(handle->data(1).toString(), snapToGrid(mapToScene(event->pos())));
                  event->accept();
                  return;
              }
          }

          if (m_currentTool > 0) {
              if (m_currentTool == 8) { // Erase tool
                  QGraphicsItem* hit = itemAt(event->pos());
                  if (hit) {
                      emit itemErased(hit);
                  }
                  event->accept();
                  return;
              }
              if (m_currentTool == 13) { // Pen tool - allow point/handle selection & manipulation
                  m_penIsDragging = false;
                  m_penPressPos = snapToGrid(mapToScene(event->pos()));
                  m_isDrawing = true;
                  
                  // Check for Alt key to toggle smooth/corner mode
                  bool isAltPressed = (event->modifiers() & Qt::AltModifier);
                  
                  // Emit signal with position and metadata for editor to handle point selection
                  emit penClicked(m_penPressPos, -1, isAltPressed ? 1 : 0);
                  
                  event->accept();
                  return;
              }
              m_isDrawing = true;
              m_drawStart = snapToGrid(mapToScene(event->pos()));
              emit pointClicked(m_drawStart);
              event->accept();
              return;
          } else {
              // Select tool - check for bezier edit point clicks
              m_bezierEditPressPos = snapToGrid(mapToScene(event->pos()));
              emit bezierEditPointClicked(m_bezierEditPressPos);
              m_bezierEditIsDragging = false;
              
              m_snapPressPos  = snapToGrid(mapToScene(event->pos()));
              QGraphicsItem* hit = itemAt(event->pos());
              bool isReal = false;
              QGraphicsItem* p = hit;
              while (p) {
                  if (p->data(1).isValid()) { isReal = true; break; }
                  p = p->parentItem();
              }
              setDragMode(isReal ? QGraphicsView::NoDrag
                                 : QGraphicsView::RubberBandDrag);
          }
      }

      QGraphicsView::mousePressEvent(event);
}

void SymbolEditorView::mouseMoveEvent(QMouseEvent* event) {
    QPointF scenePos = mapToScene(event->pos());
    QPointF snapped = snapToGrid(scenePos);
    m_hasMousePos = true;
    m_lastSnappedMousePos = snapped;
    if (m_snapCursorCrosshair) viewport()->update();
    emit coordinatesChanged(scenePos);
    emit mouseMoved(snapped);

    if (m_rectResizeActive) {
        emit rectResizeUpdated(snapToGrid(scenePos));
        event->accept();
        return;
    }

    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    if (m_isDrawing) {
        if (m_currentTool == 13 && m_penIsDragging) { // Pen tool dragging
            emit penHandleDragged(snapToGrid(mapToScene(event->pos())));
        } else if (m_currentTool != 13) {
            QPointF rawPos = mapToScene(event->pos());
            QPointF snapped = snapToGrid(rawPos);
            updateDrawGuides(rawPos);
            emit lineDragged(m_drawStart, snapped);
        }
        event->accept();
        return;
    }
    
    // Check if we should start pen dragging
    if (m_currentTool == 13 && (event->buttons() & Qt::LeftButton)) {
        QPointF currentPos = mapToScene(event->pos());
        if (QLineF(m_penPressPos, snapToGrid(currentPos)).length() > PEN_DRAG_THRESHOLD) {
            m_penIsDragging = true;
            emit penHandleDragged(snapToGrid(currentPos));
        }
    }
    
    // Check if we should start bezier edit dragging (Select mode)
    if (m_currentTool == 0 && (event->buttons() & Qt::LeftButton)) {
        QPointF currentPos = mapToScene(event->pos());
        if (QLineF(m_bezierEditPressPos, snapToGrid(currentPos)).length() > PEN_DRAG_THRESHOLD) {
            m_bezierEditIsDragging = true;
            emit bezierEditPointDragged(snapToGrid(currentPos));
        }
    } else if (m_bezierEditIsDragging) {
        // Continue dragging if button still pressed
        if (event->buttons() & Qt::LeftButton) {
            emit bezierEditPointDragged(snapToGrid(mapToScene(event->pos())));
        }
    }

    if (m_currentTool == 0 && (event->buttons() & Qt::LeftButton)) {
        updatePinAlignmentGuides();
    } else if (m_showVGuide || m_showHGuide) {
        clearPinAlignmentGuides();
    }

    QGraphicsView::mouseMoveEvent(event);
}

 void SymbolEditorView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_rectResizeActive) {
            m_rectResizeActive = false;
            emit rectResizeFinished(snapToGrid(mapToScene(event->pos())));
            event->accept();
            return;
        }

        if (m_isDrawing) {
            if (m_currentTool == 13) { // Pen tool
                QPointF releasePos = snapToGrid(mapToScene(event->pos()));
                emit penPointFinished();
                emit penPointAdded(m_penPressPos);
                m_penIsDragging = false;
            } else {
                clearPinAlignmentGuides();
                emit drawingFinished(m_drawStart, snapToGrid(mapToScene(event->pos())));
            }
            m_isDrawing = false;
            event->accept();
            return;
        }

        // Reset bezier edit dragging state
        m_bezierEditIsDragging = false;

        if (m_currentTool == 0 && dragMode() != QGraphicsView::RubberBandDrag) {
            QPointF snapEnd = snapToGrid(mapToScene(event->pos()));
            QPointF delta   = snapEnd - m_snapPressPos;
            if (delta != QPointF(0, 0) && scene() && !scene()->selectedItems().isEmpty())
                emit itemsMoved(delta);
        }
        setDragMode(QGraphicsView::NoDrag);
        clearPinAlignmentGuides();
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void SymbolEditorView::wheelEvent(QWheelEvent* event) {
    const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    applyZoomFactor(factor);
    event->accept();
}

bool SymbolEditorView::applyZoomFactor(qreal factor) {
    if (factor <= 0.0) return false;
    const qreal current = std::abs(transform().m11());
    if (current <= 0.0) return false;
    const qreal target = current * factor;
    if (target < m_minZoom || target > m_maxZoom) return false;
    scale(factor, factor);
    return true;
}

void SymbolEditorView::zoomByFactor(qreal factor) {
    applyZoomFactor(factor);
}

void SymbolEditorView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape)
        emit rightClicked();
    QGraphicsView::keyPressEvent(event);
}

void SymbolEditorView::contextMenuEvent(QContextMenuEvent* event) {
    emit contextMenuRequested(event->pos());
}

void SymbolEditorView::drawForeground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawForeground(painter, rect);
    if (!m_showVGuide && !m_showHGuide) return;

    PCBTheme* theme = ThemeManager::theme();
    QColor guideColor = QColor(239, 68, 68); // Red for high visibility
    guideColor.setAlpha(220);

    QPen guidePen(guideColor, 0.0, Qt::SolidLine);
    guidePen.setCosmetic(true);

    const qreal markerR = 3.5;
    const QPointF cursor = m_guideCursorPos;

    if (m_showVGuide) {
        painter->setPen(guidePen);
        painter->drawLine(QLineF(m_vGuideX, rect.top(), m_vGuideX, rect.bottom()));

        // Mark anchor points on this vertical guide
        painter->setBrush(guideColor);
        for (const QPointF& anchor : m_guideAnchors) {
            if (qAbs(anchor.x() - m_vGuideX) < 0.01)
                painter->drawEllipse(anchor, markerR, markerR);
        }

        // Draw cursor cross-mark on guide
        if (!cursor.isNull()) {
            painter->setPen(QPen(guideColor, 0));
            painter->drawEllipse(QPointF(m_vGuideX, cursor.y()), markerR * 1.2, markerR * 1.2);
        }
    }
    if (m_showHGuide) {
        painter->setPen(guidePen);
        painter->drawLine(QLineF(rect.left(), m_hGuideY, rect.right(), m_hGuideY));

        painter->setBrush(guideColor);
        for (const QPointF& anchor : m_guideAnchors) {
            if (qAbs(anchor.y() - m_hGuideY) < 0.01)
                painter->drawEllipse(anchor, markerR, markerR);
        }

        if (!cursor.isNull()) {
            painter->setPen(QPen(guideColor, 0));
            painter->drawEllipse(QPointF(cursor.x(), m_hGuideY), markerR * 1.2, markerR * 1.2);
        }
    }

    if (m_snapCursorCrosshair && m_hasMousePos) {
        QColor c(34, 197, 94);
        c.setAlpha(220);
        QPen p(c, 0.0, Qt::DashLine);
        p.setCosmetic(true);
        painter->setPen(p);
        const qreal x = m_lastSnappedMousePos.x();
        const qreal y = m_lastSnappedMousePos.y();
        painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
        painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
        painter->setPen(QPen(c, 0.0));
        painter->setBrush(QColor(c.red(), c.green(), c.blue(), 90));
        const qreal r = qMax<qreal>(2.5, m_gridSize * 0.12);
        painter->drawEllipse(QPointF(x, y), r, r);
    }
}

void SymbolEditorView::updatePinAlignmentGuides() {
    if (!scene()) return;

    QList<QGraphicsItem*> selected = scene()->selectedItems();
    QList<QPointF> selectedPins;
    QList<QPointF> otherPins;

    auto pinPoint = [](QGraphicsItem* item) -> QPointF {
        const qreal px = item->data(3).toDouble();
        const qreal py = item->data(4).toDouble();
        return item->mapToScene(QPointF(px, py));
    };

    for (QGraphicsItem* it : scene()->items()) {
        if (!it) continue;
        if (it->data(2).toString() != "pin") continue;
        if (it->isSelected()) selectedPins.append(pinPoint(it));
        else otherPins.append(pinPoint(it));
    }

    if (selectedPins.isEmpty() || otherPins.isEmpty()) {
        clearPinAlignmentGuides();
        return;
    }

    const qreal threshold = qMax<qreal>(2.0, m_gridSize * 0.4);
    qreal bestDx = threshold + 1.0;
    qreal bestDy = threshold + 1.0;
    bool foundX = false;
    bool foundY = false;
    qreal targetX = 0.0;
    qreal targetY = 0.0;

    for (const QPointF& sp : selectedPins) {
        for (const QPointF& op : otherPins) {
            const qreal dx = qAbs(sp.x() - op.x());
            if (dx < bestDx && dx <= threshold) {
                bestDx = dx;
                targetX = op.x();
                foundX = true;
            }
            const qreal dy = qAbs(sp.y() - op.y());
            if (dy < bestDy && dy <= threshold) {
                bestDy = dy;
                targetY = op.y();
                foundY = true;
            }
        }
    }

    m_showVGuide = foundX;
    m_showHGuide = foundY;
    m_vGuideX = targetX;
    m_hGuideY = targetY;
    viewport()->update();
}

void SymbolEditorView::clearPinAlignmentGuides() {
    if (!m_showVGuide && !m_showHGuide) return;
    m_showVGuide = false;
    m_showHGuide = false;
    m_guideCursorPos = QPointF();
    viewport()->update();
}

void SymbolEditorView::setGuideAnchorPoints(const QList<QPointF>& points) {
    m_guideAnchors = points;
}

void SymbolEditorView::updateDrawGuides(const QPointF& rawCursor) {
    m_showVGuide = false;
    m_showHGuide = false;
    m_guideCursorPos = rawCursor;

    if (m_guideAnchors.isEmpty()) {
        viewport()->update();
        return;
    }

    // Use a generous threshold in scene units — equivalent to ~6px on screen
    const qreal viewScale = std::abs(transform().m11());
    const qreal threshold = (viewScale > 0.01) ? (6.0 / viewScale) : m_gridSize;

    // Best match for vertical / horizontal alignment
    qreal bestDistX = threshold;
    qreal bestDistY = threshold;
    bool foundX = false;
    bool foundY = false;
    qreal guideX = 0.0;
    qreal guideY = 0.0;

    for (const QPointF& anchor : m_guideAnchors) {
        // Skip anchors that are the draw start point (avoid self-alignment)
        if (m_isDrawing && QLineF(rawCursor, anchor).length() < m_gridSize * 0.3)
            continue;

        const qreal distX = qAbs(rawCursor.x() - anchor.x());
        if (distX < bestDistX) {
            bestDistX = distX;
            guideX = anchor.x();
            foundX = true;
        }
        const qreal distY = qAbs(rawCursor.y() - anchor.y());
        if (distY < bestDistY) {
            bestDistY = distY;
            guideY = anchor.y();
            foundY = true;
        }
    }

    m_showVGuide = foundX;
    m_showHGuide = foundY;
    if (foundX) m_vGuideX = guideX;
    if (foundY) m_hGuideY = guideY;
    viewport()->update();
}
