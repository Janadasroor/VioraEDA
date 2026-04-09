#include "gerber_exporter.h"
#include "nc_drill_exporter.h"
#include "../items/pcb_item.h"
#include "../items/component_item.h"
#include "../items/shape_item.h"
#include "../items/image_item.h"
#include "../../footprints/models/footprint_primitive.h"
#include <QFileInfo>
#include <QFile>
#include "../items/trace_item.h"
#include "../items/pad_item.h"
#include "../items/via_item.h"
#include "../items/copper_pour_item.h"
#include "../layers/pcb_layer.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <cmath>
#include <QPainterPath>
#include <QPolygonF>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>

namespace {
constexpr int kFootprintPrimitiveLayerKey = 0x46504C59; // "FPLY"
QString normalizedPadShape(QString shape);

bool footprintLayerMatchesOutput(int footprintLayer, int outputLayerId) {
    using Flux::Model::FootprintPrimitive;
    switch (footprintLayer) {
    case FootprintPrimitive::Top_Silkscreen: return outputLayerId == PCBLayerManager::TopSilkscreen;
    case FootprintPrimitive::Bottom_Silkscreen: return outputLayerId == PCBLayerManager::BottomSilkscreen;
    case FootprintPrimitive::Top_Courtyard: return outputLayerId == PCBLayerManager::TopCourtyard;
    case FootprintPrimitive::Bottom_Courtyard: return outputLayerId == PCBLayerManager::BottomCourtyard;
    case FootprintPrimitive::Top_Fabrication: return outputLayerId == PCBLayerManager::TopFabrication;
    case FootprintPrimitive::Bottom_Fabrication: return outputLayerId == PCBLayerManager::BottomFabrication;
    default: return false;
    }
}

bool defaultComponentGraphicMatchesOutput(const ComponentItem* comp, int outputLayerId) {
    if (!comp) {
        return false;
    }
    const PCBLayer* compLayer = PCBLayerManager::instance().layer(comp->layer());
    if (!compLayer) {
        return false;
    }
    if (compLayer->side() == PCBLayer::Bottom) {
        return outputLayerId == PCBLayerManager::BottomSilkscreen;
    }
    return outputLayerId == PCBLayerManager::TopSilkscreen;
}

QString formatCoordLocal(double val, int decimals) {
    const long long intValue = static_cast<long long>(std::round(val * std::pow(10, decimals)));
    return QString::number(intValue);
}

void writeMove(QTextStream& out, double x, double y, int decimals) {
    out << "X" << formatCoordLocal(x, decimals)
        << "Y" << formatCoordLocal(y, decimals) << "D02*\n";
}

void writeDraw(QTextStream& out, double x, double y, int decimals) {
    out << "X" << formatCoordLocal(x, decimals)
        << "Y" << formatCoordLocal(y, decimals) << "D01*\n";
}

void writeFlash(QTextStream& out, double x, double y, int decimals) {
    out << "X" << formatCoordLocal(x, decimals)
        << "Y" << formatCoordLocal(y, decimals) << "D03*\n";
}

void writePolygonRegion(QTextStream& out, const QPolygonF& poly, int decimals) {
    if (poly.size() < 3) return;
    out << "G36*\n";
    writeMove(out, poly.first().x(), poly.first().y(), decimals);
    for (int i = 1; i < poly.size(); ++i) {
        writeDraw(out, poly[i].x(), poly[i].y(), decimals);
    }
    if (poly.last() != poly.first()) {
        writeDraw(out, poly.first().x(), poly.first().y(), decimals);
    }
    out << "G37*\n";
}

void writePathAsRegions(QTextStream& out, const QPainterPath& path, int decimals) {
    const QList<QPolygonF> polys = path.toSubpathPolygons();
    if (polys.isEmpty()) return;

    struct PolyInfo {
        QPolygonF poly;
        double area;
    };
    QList<PolyInfo> infos;
    for (const QPolygonF& poly : polys) {
        if (poly.size() < 3) continue;
        double a = 0;
        for (int i = 0; i < poly.size(); ++i) {
            int j = (i + 1) % poly.size();
            a += poly[i].x() * poly[j].y() - poly[j].x() * poly[i].y();
        }
        infos.append({poly, std::abs(a / 2.0)});
    }

    std::sort(infos.begin(), infos.end(), [](const PolyInfo& a, const PolyInfo& b) {
        return a.area > b.area;
    });

    if (infos.isEmpty()) return;

    QList<QPolygonF> darkPolys;
    QList<QPolygonF> clearPolys;

    for (int i = 0; i < infos.size(); ++i) {
        int containmentCount = 0;
        QPointF center = infos[i].poly.boundingRect().center();
        for (int j = 0; j < i; ++j) {
            if (infos[j].poly.containsPoint(center, Qt::OddEvenFill)) {
                containmentCount++;
            }
        }
        if (containmentCount % 2 != 0) clearPolys.append(infos[i].poly);
        else darkPolys.append(infos[i].poly);
    }

    for (const QPolygonF& poly : darkPolys) {
        writePolygonRegion(out, poly, decimals);
    }

    if (!clearPolys.isEmpty()) {
        out << "%LPC*%\n";
        for (const QPolygonF& poly : clearPolys) {
            writePolygonRegion(out, poly, decimals);
        }
        out << "%LPD*%\n";
    }
}

QPainterPath buildPadPath(const PadItem* pad, double expansion = 0.0, double drill = 0.0) {
    QPainterPath path;
    if (!pad) {
        return path;
    }

    const QSizeF rawSize = pad->size();
    const double w = rawSize.width() + expansion * 2.0;
    const double h = rawSize.height() + expansion * 2.0;
    if (w <= 0.0 || h <= 0.0) {
        return path;
    }

    const QRectF rect(-w / 2.0, -h / 2.0, w, h);
    const QString shape = normalizedPadShape(pad->padShape());
    if (shape == "rect") {
        path.addRect(rect);
    } else if (shape == "oblong") {
        const double r = std::min(w, h) / 2.0;
        path.addRoundedRect(rect, r, r);
    } else if (shape == "roundedrect") {
        const double r = std::min(w, h) * 0.25;
        path.addRoundedRect(rect, r, r);
    } else {
        path.addEllipse(rect);
    }

    if (drill > 0.001) {
        path.addEllipse(QPointF(0, 0), drill / 2.0, drill / 2.0);
        path.setFillRule(Qt::OddEvenFill);
    }

    return path;
}

QString flashApertureKey(const QString& type, double a, double b = 0.0, double drill = 0.0) {
    return QString("%1:%2:%3:%4")
        .arg(type)
        .arg(a, 0, 'f', 6)
        .arg(b, 0, 'f', 6)
        .arg(drill, 0, 'f', 6);
}

QString normalizedPadShape(QString shape) {
    shape = shape.trimmed().toLower();
    if (shape == "rect" || shape == "rectangle" || shape == "square") return "rect";
    if (shape == "round" || shape == "circle") return "round";
    if (shape == "oblong" || shape == "oval") return "oblong";
    if (shape == "roundedrect" || shape == "roundrect") return "roundedrect";
    if (shape == "trapezoid") return "trapezoid";
    if (shape == "custom") return "custom";
    return shape;
}

bool rotationSupportsAxisAlignedFlash(double rotationDeg) {
    const double normalized = std::fmod(std::abs(rotationDeg), 180.0);
    return std::abs(normalized) < 1e-4 || std::abs(normalized - 90.0) < 1e-4;
}

double itemSceneRotationDeg(const QGraphicsItem* item) {
    if (!item) {
        return 0.0;
    }
    const QTransform t = item->sceneTransform();
    return std::atan2(t.m12(), t.m11()) * 180.0 / M_PI;
}

void writePolyline(QTextStream& out, const QPolygonF& poly, int decimals) {
    if (poly.size() < 2) return;
    writeMove(out, poly.first().x(), poly.first().y(), decimals);
    for (int i = 1; i < poly.size(); ++i) {
        writeDraw(out, poly[i].x(), poly[i].y(), decimals);
    }
}

void exportChildGraphic(QTextStream& out, QGraphicsItem* child, int decimals,
                        QMap<double, int>& apertures, int& nextD) {
    if (!child || !child->isVisible()) return;

    auto getApertureLocal = [&](double width) {
        if (!apertures.contains(width)) {
            apertures[width] = nextD++;
            out << QString("%%ADD%1C,%2*%%\n").arg(apertures[width]).arg(width, 0, 'f', 3);
        }
        return apertures[width];
    };
    if (auto* line = dynamic_cast<QGraphicsLineItem*>(child)) {
        const int d = getApertureLocal(std::max(0.05, line->pen().widthF()));
        out << QString("D%1*\n").arg(d);
        const QLineF sceneLine(line->mapToScene(line->line().p1()), line->mapToScene(line->line().p2()));
        writeMove(out, sceneLine.p1().x(), sceneLine.p1().y(), decimals);
        writeDraw(out, sceneLine.p2().x(), sceneLine.p2().y(), decimals);
        return;
    }

    if (auto* rect = dynamic_cast<QGraphicsRectItem*>(child)) {
        const int d = getApertureLocal(std::max(0.05, rect->pen().widthF()));
        out << QString("D%1*\n").arg(d);
        const QPolygonF poly = rect->mapToScene(rect->rect());
        writePolyline(out, poly, decimals);
        if (!poly.isEmpty()) writeDraw(out, poly.first().x(), poly.first().y(), decimals);
        return;
    }

    if (auto* ellipse = dynamic_cast<QGraphicsEllipseItem*>(child)) {
        const int d = getApertureLocal(std::max(0.05, ellipse->pen().widthF()));
        out << QString("D%1*\n").arg(d);
        const QPainterPath scenePath = ellipse->sceneTransform().map(ellipse->shape());
        const QList<QPolygonF> polys = scenePath.toSubpathPolygons();
        for (const QPolygonF& poly : polys) {
            writePolyline(out, poly, decimals);
            if (!poly.isEmpty()) writeDraw(out, poly.first().x(), poly.first().y(), decimals);
        }
        return;
    }

    if (auto* pathItem = dynamic_cast<QGraphicsPathItem*>(child)) {
        const int d = getApertureLocal(std::max(0.05, pathItem->pen().widthF()));
        out << QString("D%1*\n").arg(d);
        const QList<QPolygonF> polys = pathItem->sceneTransform().map(pathItem->path()).toSubpathPolygons();
        for (const QPolygonF& poly : polys) {
            writePolyline(out, poly, decimals);
        }
        return;
    }

    if (auto* polyItem = dynamic_cast<QGraphicsPolygonItem*>(child)) {
        const int d = getApertureLocal(std::max(0.05, polyItem->pen().widthF()));
        out << QString("D%1*\n").arg(d);
        const QPolygonF poly = polyItem->mapToScene(polyItem->polygon());
        writePolyline(out, poly, decimals);
        if (!poly.isEmpty()) writeDraw(out, poly.first().x(), poly.first().y(), decimals);
        return;
    }

    if (auto* textItem = dynamic_cast<QGraphicsSimpleTextItem*>(child)) {
        const int d = getApertureLocal(0.12);
        out << QString("D%1*\n").arg(d);
        const QList<QPolygonF> polys = textItem->sceneTransform().map(textItem->shape()).toSubpathPolygons();
        for (const QPolygonF& poly : polys) {
            writePolyline(out, poly, decimals);
            if (!poly.isEmpty()) writeDraw(out, poly.first().x(), poly.first().y(), decimals);
        }
        return;
    }

    if (auto* textItem = dynamic_cast<QGraphicsTextItem*>(child)) {
        const int d = getApertureLocal(0.12);
        out << QString("D%1*\n").arg(d);
        const QList<QPolygonF> polys = textItem->sceneTransform().map(textItem->shape()).toSubpathPolygons();
        for (const QPolygonF& poly : polys) {
            writePolyline(out, poly, decimals);
            if (!poly.isEmpty()) writeDraw(out, poly.first().x(), poly.first().y(), decimals);
        }
    }
}

void exportImagePreview(QTextStream& out, const PCBImageItem* imageItem, int decimals) {
    if (!imageItem) {
        return;
    }

    const QImage preview = imageItem->fabPreview();
    if (preview.isNull() || preview.width() <= 0 || preview.height() <= 0) {
        return;
    }

    const QRectF localRect = imageItem->boundingRect();
    const double pxW = localRect.width() / preview.width();
    const double pxH = localRect.height() / preview.height();
    if (pxW <= 0.0 || pxH <= 0.0) {
        return;
    }

    auto alphaAt = [&](int x, int y) {
        return qAlpha(preview.pixel(x, y)) > 20;
    };

    for (int y = 0; y < preview.height(); ++y) {
        int x = 0;
        while (x < preview.width()) {
            while (x < preview.width() && !alphaAt(x, y)) {
                ++x;
            }
            const int start = x;
            while (x < preview.width() && alphaAt(x, y)) {
                ++x;
            }
            if (start >= x) {
                continue;
            }

            const QRectF segRect(
                localRect.left() + start * pxW,
                localRect.top() + y * pxH,
                (x - start) * pxW,
                pxH);
            const QPolygonF scenePoly = imageItem->mapToScene(segRect);
            writePolygonRegion(out, scenePoly, decimals);
        }
    }
}
}

bool GerberExporter::exportLayer(QGraphicsScene* scene, int layerId, const QString& filePath, const GerberExportSettings& settings) {
    if (!scene) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    const int decimals = std::max(3, settings.decimalPlaces);

    // 1. Header
    out << "%FSLAX24Y24*%\n";
    out << "%MOMM*%\n";
    out << "%LPD*%\n";
    
    // 2. Define Apertures
    QMap<double, int> apertures;
    QMap<QString, int> flashApertures;
    int nextD = 10;

    auto getAperture = [&](double width) {
        if (!apertures.contains(width)) {
            apertures[width] = nextD++;
            out << QString("%%ADD%1C,%2*%%\n").arg(apertures[width]).arg(width, 0, 'f', 3);
        }
        return apertures[width];
    };

    auto getFlashAperture = [&](const QString& shape, double w, double h = 0.0, double drill = 0.0) {
        const QString key = flashApertureKey(shape, w, h, drill);
        if (!flashApertures.contains(key)) {
            flashApertures[key] = nextD++;
            if (shape == "C") {
                if (drill > 0.001) {
                    out << QString("%%ADD%1C,%2X%3*%%\n").arg(flashApertures[key]).arg(w, 0, 'f', 3).arg(drill, 0, 'f', 3);
                } else {
                    out << QString("%%ADD%1C,%2*%%\n").arg(flashApertures[key]).arg(w, 0, 'f', 3);
                }
            } else if (shape == "R") {
                if (drill > 0.001) {
                    out << QString("%%ADD%1R,%2X%3X%4*%%\n").arg(flashApertures[key]).arg(w, 0, 'f', 3).arg(h, 0, 'f', 3).arg(drill, 0, 'f', 3);
                } else {
                    out << QString("%%ADD%1R,%2X%3*%%\n").arg(flashApertures[key]).arg(w, 0, 'f', 3).arg(h, 0, 'f', 3);
                }
            } else if (shape == "O") {
                if (drill > 0.001) {
                    out << QString("%%ADD%1O,%2X%3X%4*%%\n").arg(flashApertures[key]).arg(w, 0, 'f', 3).arg(h, 0, 'f', 3).arg(drill, 0, 'f', 3);
                } else {
                    out << QString("%%ADD%1O,%2X%3*%%\n").arg(flashApertures[key]).arg(w, 0, 'f', 3).arg(h, 0, 'f', 3);
                }
            }
        }
        return flashApertures[key];
    };

    const PCBLayerManager::BoardStackup stackup = PCBLayerManager::instance().stackup();
    const bool isTopMask = (layerId == PCBLayerManager::TopSoldermask);
    const bool isBottomMask = (layerId == PCBLayerManager::BottomSoldermask);
    const bool isTopPaste = (layerId == PCBLayerManager::TopPaste);
    const bool isBottomPaste = (layerId == PCBLayerManager::BottomPaste);
    const bool isEdgeCuts = (layerId == PCBLayerManager::EdgeCuts);

    auto tryWritePadFlash = [&](const PadItem* pad, double expansion) {
        if (!pad) {
            return false;
        }
        const QSizeF size = pad->size();
        const double w = size.width() + expansion * 2.0;
        const double h = size.height() + expansion * 2.0;
        if (w <= 0.0 || h <= 0.0) {
            return false;
        }

        const bool isCopper = !(isTopMask || isBottomMask || isTopPaste || isBottomPaste || isEdgeCuts);
        const double drill = isCopper ? pad->drillSize() : 0.0;

        const QString shape = normalizedPadShape(pad->padShape());
        if (shape == "round") {
            const int ap = getFlashAperture("C", std::max(w, h), 0.0, drill);
            out << QString("D%1*\n").arg(ap);
            writeFlash(out, pad->scenePos().x(), pad->scenePos().y(), decimals);
            return true;
        }

        const double sceneRotation = itemSceneRotationDeg(pad);
        if (!rotationSupportsAxisAlignedFlash(sceneRotation)) {
            return false;
        }

        if (shape == "rect") {
            if (drill > 0.001) return false; // Avoid non-standard flash for pads with holes
            const bool swapAxes = std::abs(std::fmod(std::abs(sceneRotation), 180.0) - 90.0) < 1e-4;
            const int ap = getFlashAperture("R", swapAxes ? h : w, swapAxes ? w : h, 0.0);
            out << QString("D%1*\n").arg(ap);
            writeFlash(out, pad->scenePos().x(), pad->scenePos().y(), decimals);
            return true;
        }

        if (shape == "oblong") {
            if (drill > 0.001) return false; // Avoid non-standard flash for pads with holes
            const bool swapAxes = std::abs(std::fmod(std::abs(sceneRotation), 180.0) - 90.0) < 1e-4;
            const int ap = getFlashAperture("O", swapAxes ? h : w, swapAxes ? w : h, 0.0);
            out << QString("D%1*\n").arg(ap);
            writeFlash(out, pad->scenePos().x(), pad->scenePos().y(), decimals);
            return true;
        }

        return false;
    };

    // 3. Plot Items
    for (auto* gItem : scene->items()) {
        PCBItem* item = dynamic_cast<PCBItem*>(gItem);
        if (!item) continue;
        
        bool itemOnLayer = (item->layer() == layerId);
        
        // Special handling for through-hole items (Vias and TH Pads)
        if (auto* via = dynamic_cast<ViaItem*>(item)) {
            if (via->spansLayer(layerId)) itemOnLayer = true;
        } else if (auto* pad = dynamic_cast<PadItem*>(item)) {
            if (pad->drillSize() > 0.001) {
                const PCBLayer* l = PCBLayerManager::instance().layer(layerId);
                if (l && (l->isCopperLayer() || l->type() == PCBLayer::Soldermask)) {
                    itemOnLayer = true;
                }
            }
        }

        if (!(isTopMask || isBottomMask || isTopPaste || isBottomPaste)) {
            if (!itemOnLayer) continue;
        }

        if (isTopMask || isBottomMask) {
            if (auto* pad = dynamic_cast<PadItem*>(item)) {
                const PCBLayer* l = PCBLayerManager::instance().layer(pad->layer());
                if (!l) continue;
                if (isTopMask && l->side() != PCBLayer::Top) continue;
                if (isBottomMask && l->side() != PCBLayer::Bottom) continue;
                const double expansion = pad->maskExpansionOverrideEnabled() ? pad->maskExpansion() : stackup.solderMaskExpansion;
                if (!tryWritePadFlash(pad, expansion)) {
                    const QPainterPath expandedPad = buildPadPath(pad, expansion, 0.0);
                    if (!expandedPad.isEmpty()) {
                        writePathAsRegions(out, pad->sceneTransform().map(expandedPad), decimals);
                    }
                }
            } else if (auto* via = dynamic_cast<ViaItem*>(item)) {
                const bool show = (isTopMask && via->spansLayer(PCBLayerManager::TopCopper)) ||
                                  (isBottomMask && via->spansLayer(PCBLayerManager::BottomCopper));
                if (!show) continue;
                const double expansion = via->maskExpansionOverrideEnabled() ? via->maskExpansion() : stackup.solderMaskExpansion;
                const double d = std::max(0.01, via->diameter() + 2.0 * expansion);
                int ap = getFlashAperture("C", d, 0.0, 0.0); // Mask doesn't show drill
                out << QString("D%1*\n").arg(ap);
                writeFlash(out, via->scenePos().x(), via->scenePos().y(), decimals);
            }
            continue;
        }

        if (isTopPaste || isBottomPaste) {
            if (auto* pad = dynamic_cast<PadItem*>(item)) {
                const PCBLayer* l = PCBLayerManager::instance().layer(pad->layer());
                if (!l) continue;
                if (isTopPaste && l->side() != PCBLayer::Top) continue;
                if (isBottomPaste && l->side() != PCBLayer::Bottom) continue;
                const double expansion = pad->pasteExpansionOverrideEnabled() ? pad->pasteExpansion() : stackup.pasteExpansion;
                if (!tryWritePadFlash(pad, expansion)) {
                    const QPainterPath expandedPad = buildPadPath(pad, expansion, 0.0);
                    if (!expandedPad.isEmpty()) {
                        writePathAsRegions(out, pad->sceneTransform().map(expandedPad), decimals);
                    }
                }
            }
            continue;
        }

        if (auto* trace = dynamic_cast<TraceItem*>(item)) {
            int d = getAperture(trace->width());
            out << QString("D%1*\n").arg(d);
            const QPointF s = trace->mapToScene(trace->startPoint());
            const QPointF e = trace->mapToScene(trace->endPoint());
            writeMove(out, s.x(), s.y(), decimals);
            writeDraw(out, e.x(), e.y(), decimals);
        }
        else if (auto* pad = dynamic_cast<PadItem*>(item)) {
            if (!tryWritePadFlash(pad, 0.0)) {
                const QPainterPath path = buildPadPath(pad, 0.0, pad->drillSize());
                if (!path.isEmpty()) {
                    writePathAsRegions(out, pad->sceneTransform().map(path), decimals);
                }
            }
        }
        else if (auto* via = dynamic_cast<ViaItem*>(item)) {
            const int ap = getFlashAperture("C", via->diameter(), 0.0, via->drillSize());
            out << QString("D%1*\n").arg(ap);
            writeFlash(out, via->scenePos().x(), via->scenePos().y(), decimals);
        }
        else if (auto* comp = dynamic_cast<ComponentItem*>(item)) {
            for (QGraphicsItem* child : comp->childItems()) {
                if (dynamic_cast<PadItem*>(child)) continue;
                const QVariant footprintLayer = child->data(kFootprintPrimitiveLayerKey);
                if (footprintLayer.isValid()) {
                    if (!footprintLayerMatchesOutput(footprintLayer.toInt(), layerId)) continue;
                } else if (!defaultComponentGraphicMatchesOutput(comp, layerId)) {
                    continue;
                }
                exportChildGraphic(out, child, decimals, apertures, nextD);
            }
        }
        else if (auto* image = dynamic_cast<PCBImageItem*>(item)) {
            exportImagePreview(out, image, decimals);
        }
        else if (auto* shape = dynamic_cast<PCBShapeItem*>(item)) {
            if (isEdgeCuts) {
                const QPainterPath path = shape->sceneTransform().map(shape->shape());
                const QList<QPolygonF> polys = path.toSubpathPolygons();
                const int edgeAperture = getAperture(std::max(0.05, shape->strokeWidth()));
                out << QString("D%1*\n").arg(edgeAperture);
                for (const QPolygonF& poly : polys) {
                    if (poly.size() < 2) continue;
                    writeMove(out, poly.first().x(), poly.first().y(), decimals);
                    for (int i = 1; i < poly.size(); ++i) {
                        writeDraw(out, poly[i].x(), poly[i].y(), decimals);
                    }
                }
            } else {
                writePathAsRegions(out, shape->sceneTransform().map(shape->shape()), decimals);
            }
        }
        else if (auto* pour = dynamic_cast<CopperPourItem*>(item)) {
            if (isEdgeCuts) {
                QPolygonF poly = pour->polygon();
                if (poly.size() >= 2) {
                    const int edgeAperture = getAperture(0.10);
                    out << QString("D%1*\n").arg(edgeAperture);
                    writeMove(out, poly.first().x(), poly.first().y(), decimals);
                    for (int i = 1; i < poly.size(); ++i) {
                        writeDraw(out, poly[i].x(), poly[i].y(), decimals);
                    }
                    if (poly.last() != poly.first()) {
                        writeDraw(out, poly.first().x(), poly.first().y(), decimals);
                    }
                }
            } else {
                writePathAsRegions(out, pour->sceneTransform().map(pour->shape()), decimals);
            }
        }
    }

    // 4. Footer
    out << "M02*\n";
    return true;
}

QString GerberExporter::formatCoord(double val, int decimals) {
    long long intValue = static_cast<long long>(std::round(val * std::pow(10, decimals)));
    return QString::number(intValue);
}

bool GerberExporter::generateDrillFile(QGraphicsScene* scene, const QString& filePath) {
    // Delegate to the proper NC Drill exporter
    NCDrillExporter::DrillOptions opts;
    opts.separatePTH = false; // Single file for backward compatibility
    QString dir = QFileInfo(filePath).absolutePath();
    auto result = NCDrillExporter::exportDrills(scene, dir, opts);
    if (!result.success) return false;

    // Move the generated PTH file to the requested path
    QFile::remove(filePath);
    QFile::copy(result.pthFilePath, filePath);
    return true;
}

NCDrillExporter::DrillExportResult GerberExporter::exportDrills(QGraphicsScene* scene,
                                                                 const QString& outputDirectory,
                                                                 const NCDrillExporter::DrillOptions& options) {
    return NCDrillExporter::exportDrills(scene, outputDirectory, options);
}
