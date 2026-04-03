#include "gerber_exporter.h"
#include "nc_drill_exporter.h"
#include "../items/pcb_item.h"
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

bool GerberExporter::exportLayer(QGraphicsScene* scene, int layerId, const QString& filePath, const GerberExportSettings& settings) {
    if (!scene) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);

    // 1. Header
    out << "%FSLAX34Y34*%\n";
    out << "%MOMM*%\n";
    out << "%LPD*%\n";
    
    // 2. Define Apertures
    QMap<double, int> apertures;
    int nextD = 10;

    auto getAperture = [&](double width) {
        if (!apertures.contains(width)) {
            apertures[width] = nextD++;
            out << QString("%%ADD%1C,%2*%%\n").arg(apertures[width]).arg(width, 0, 'f', 3);
        }
        return apertures[width];
    };

    const PCBLayerManager::BoardStackup stackup = PCBLayerManager::instance().stackup();
    const bool isTopMask = (layerId == PCBLayerManager::TopSoldermask);
    const bool isBottomMask = (layerId == PCBLayerManager::BottomSoldermask);
    const bool isTopPaste = (layerId == PCBLayerManager::TopPaste);
    const bool isBottomPaste = (layerId == PCBLayerManager::BottomPaste);

    // 3. Plot Items
    for (auto* gItem : scene->items()) {
        PCBItem* item = dynamic_cast<PCBItem*>(gItem);
        if (!item) continue;
        if (!(isTopMask || isBottomMask || isTopPaste || isBottomPaste)) {
            if (ViaItem* via = dynamic_cast<ViaItem*>(item)) {
                if (!via->spansLayer(layerId)) continue;
            } else if (item->layer() != layerId) {
                continue;
            }
        }

        if (isTopMask || isBottomMask) {
            if (auto* pad = dynamic_cast<PadItem*>(item)) {
                const PCBLayer* l = PCBLayerManager::instance().layer(pad->layer());
                if (!l) continue;
                if (isTopMask && l->side() != PCBLayer::Top) continue;
                if (isBottomMask && l->side() != PCBLayer::Bottom) continue;
                const double expansion = pad->maskExpansionOverrideEnabled() ? pad->maskExpansion() : stackup.solderMaskExpansion;
                const double d = std::max(0.01, std::max(pad->size().width(), pad->size().height()) + 2.0 * expansion);
                int ap = getAperture(d);
                out << QString("D%1*\n").arg(ap);
                out << "X" << formatCoord(pad->scenePos().x(), 4) << "Y" << formatCoord(pad->scenePos().y(), 4) << "D03*\n";
            } else if (auto* via = dynamic_cast<ViaItem*>(item)) {
                const bool show = (isTopMask && via->spansLayer(PCBLayerManager::TopCopper)) ||
                                  (isBottomMask && via->spansLayer(PCBLayerManager::BottomCopper));
                if (!show) continue;
                const double expansion = via->maskExpansionOverrideEnabled() ? via->maskExpansion() : stackup.solderMaskExpansion;
                const double d = std::max(0.01, via->diameter() + 2.0 * expansion);
                int ap = getAperture(d);
                out << QString("D%1*\n").arg(ap);
                out << "X" << formatCoord(via->scenePos().x(), 4) << "Y" << formatCoord(via->scenePos().y(), 4) << "D03*\n";
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
                const double d = std::max(0.0, std::max(pad->size().width(), pad->size().height()) + 2.0 * expansion);
                if (d <= 0.0) continue;
                int ap = getAperture(d);
                out << QString("D%1*\n").arg(ap);
                out << "X" << formatCoord(pad->scenePos().x(), 4) << "Y" << formatCoord(pad->scenePos().y(), 4) << "D03*\n";
            }
            continue;
        }

        if (auto* trace = dynamic_cast<TraceItem*>(item)) {
            int d = getAperture(trace->width());
            out << QString("D%1*\n").arg(d);
            out << "X" << formatCoord(trace->startPoint().x(), 4) << "Y" << formatCoord(trace->startPoint().y(), 4) << "D02*\n";
            out << "X" << formatCoord(trace->endPoint().x(), 4) << "Y" << formatCoord(trace->endPoint().y(), 4) << "D01*\n";
        }
        else if (auto* pad = dynamic_cast<PadItem*>(item)) {
            int d = getAperture(std::max(pad->size().width(), pad->size().height()));
            out << QString("D%1*\n").arg(d);
            out << "X" << formatCoord(pad->scenePos().x(), 4) << "Y" << formatCoord(pad->scenePos().y(), 4) << "D03*\n";
        }
        else if (auto* via = dynamic_cast<ViaItem*>(item)) {
            int d = getAperture(via->diameter());
            out << QString("D%1*\n").arg(d);
            out << "X" << formatCoord(via->scenePos().x(), 4) << "Y" << formatCoord(via->scenePos().y(), 4) << "D03*\n";
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
