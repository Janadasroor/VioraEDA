#include "nc_drill_exporter.h"
#include "../items/via_item.h"
#include "../items/pad_item.h"
#include "../items/pcb_item.h"
#include "../items/component_item.h"
#include "../layers/pcb_layer.h"
#include <QGraphicsScene>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QtMath>
#include <QSet>
#include <algorithm>

// ============================================================================
// Main export entry point
// ============================================================================

NCDrillExporter::DrillExportResult NCDrillExporter::exportDrills(QGraphicsScene* scene,
                                                                  const QString& outputDirectory,
                                                                  const DrillOptions& options) {
    DrillExportResult result;

    if (!scene) {
        result.error = "Invalid scene.";
        return result;
    }
    if (outputDirectory.isEmpty()) {
        result.error = "Output directory is empty.";
        return result;
    }

    QDir dir(outputDirectory);
    if (!dir.exists() && !dir.mkpath(".")) {
        result.error = "Cannot create output directory: " + outputDirectory;
        return result;
    }

    // Generate file names
    result.pthFilePath = generatePTHFileName(outputDirectory, options);
    if (options.separatePTH && hasNonPlatedHoles(scene)) {
        result.npthFilePath = generateNPTHFileName(outputDirectory, options);
    }

    // Collect holes and build tool tables
    QList<DrillHole> pthHoles = collectHoles(scene, true);
    QList<DrillHole> npthHoles = options.separatePTH ? collectHoles(scene, false) : QList<DrillHole>();

    QMap<double, int> pthTools = collectToolTable(scene, true);
    QMap<double, int> npthTools = options.separatePTH ? collectToolTable(scene, false) : QMap<double, int>();

    // Write PTH file
    {
        QFile pthFile(result.pthFilePath);
        if (!pthFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            result.error = "Cannot open PTH file: " + result.pthFilePath;
            return result;
        }
        QTextStream out(&pthFile);
        writeHeader(out, options);
        writeToolTable(out, pthTools, options);
        writeHoles(out, pthHoles, pthTools, options);
        if (options.includeToolList && !pthTools.isEmpty()) {
            out << "; Tool List:\n";
            for (auto it = pthTools.begin(); it != pthTools.end(); ++it) {
                double sizeMm = it.key();
                int toolCode = it.value();
                double displaySize = (options.unit == Inches) ? sizeMm * 0.0393701 : sizeMm;
                QString unitStr = (options.unit == Inches) ? "in" : "mm";
                out << "; T" << toolCode << " = " << QString::number(displaySize, 'f', 3) << " " << unitStr << "\n";
            }
        }
        writeFooter(out, options);
    }

    // Write NPTH file (if separate and has holes)
    if (!npthTools.isEmpty() && !npthHoles.isEmpty()) {
        QFile npthFile(result.npthFilePath);
        if (!npthFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            result.error = "Cannot open NPTH file: " + result.npthFilePath;
            return result;
        }
        QTextStream out(&npthFile);
        writeHeader(out, options);
        writeToolTable(out, npthTools, options);
        writeHoles(out, npthHoles, npthTools, options);
        if (options.includeToolList && !npthTools.isEmpty()) {
            out << "; Tool List:\n";
            for (auto it = npthTools.begin(); it != npthTools.end(); ++it) {
                double sizeMm = it.key();
                int toolCode = it.value();
                double displaySize = (options.unit == Inches) ? sizeMm * 0.0393701 : sizeMm;
                QString unitStr = (options.unit == Inches) ? "in" : "mm";
                out << "; T" << toolCode << " = " << QString::number(displaySize, 'f', 3) << " " << unitStr << "\n";
            }
        }
        writeFooter(out, options);
    }

    // Compute statistics
    result.totalHoles = pthHoles.size() + npthHoles.size();
    result.totalSlots = std::count_if(pthHoles.begin(), pthHoles.end(), [](const DrillHole& h) { return h.isSlot; }) +
                        std::count_if(npthHoles.begin(), npthHoles.end(), [](const DrillHole& h) { return h.isSlot; });
    result.toolCount = pthTools.size() + npthTools.size();
    result.success = true;

    // Summary
    result.summary = QString("Exported %1 holes (%2 PTH, %3 NPTH) using %4 tools")
        .arg(result.totalHoles).arg(pthHoles.size()).arg(npthHoles.size()).arg(result.toolCount);
    if (result.totalSlots > 0) {
        result.summary += QString(", %1 slots").arg(result.totalSlots);
    }

    return result;
}

// ============================================================================
// Content generation (string only, no file I/O)
// ============================================================================

QString NCDrillExporter::generateDrillContent(QGraphicsScene* scene, bool plated,
                                                const DrillOptions& options) {
    QString content;
    QTextStream out(&content);

    QList<DrillHole> holes = collectHoles(scene, plated);
    QMap<double, int> tools = collectToolTable(scene, plated);

    writeHeader(out, options);
    writeToolTable(out, tools, options);
    writeHoles(out, holes, tools, options);
    writeFooter(out, options);

    return content;
}

// ============================================================================
// Tool table collection
// ============================================================================

QMap<double, int> NCDrillExporter::collectToolTable(QGraphicsScene* scene, bool plated) {
    QMap<double, int> toolTable;
    int toolCode = 1;

    QSet<double> uniqueSizes;

    for (auto* item : scene->items()) {
        double drill = 0;
        bool isPlated = true;

        if (auto* via = dynamic_cast<ViaItem*>(item)) {
            drill = via->drillSize();
            isPlated = true; // Vias are always plated
        } else if (auto* pad = dynamic_cast<PadItem*>(item)) {
            drill = pad->drillSize();
            // Pads with drill > 0 are through-hole; assume plated unless marked otherwise
            isPlated = true;
        }

        if (drill > 0.001 && isPlated == plated) {
            uniqueSizes.insert(drill);
        }
    }

    // Sort sizes ascending and assign tool codes
    QList<double> sortedSizes = uniqueSizes.values();
    std::sort(sortedSizes.begin(), sortedSizes.end());

    for (double size : sortedSizes) {
        toolTable[size] = toolCode++;
    }

    return toolTable;
}

bool NCDrillExporter::hasNonPlatedHoles(QGraphicsScene* scene) {
    // Currently all pads are assumed plated. Return false until NPTH pad support is added.
    Q_UNUSED(scene);
    return false;
}

// ============================================================================
// Hole collection
// ============================================================================

QList<NCDrillExporter::DrillHole> NCDrillExporter::collectHoles(QGraphicsScene* scene, bool plated) {
    QList<DrillHole> holes;

    for (auto* gItem : scene->items()) {
        DrillHole hole;
        bool isPlated = true;
        double drill = 0;
        bool isSlot = false;
        double slotLength = 0;
        QPointF pos;
        QString netName;
        QString itemName;
        bool isBlindBuried = false;
        bool isMicrovia = false;
        int startLayer = 0;
        int endLayer = 1;

        if (auto* via = dynamic_cast<ViaItem*>(gItem)) {
            drill = via->drillSize();
            pos = via->pos();
            netName = via->netName();
            itemName = "Via";
            startLayer = via->startLayer();
            endLayer = via->endLayer();
            isBlindBuried = via->viaType() != "Through";
            isMicrovia = via->isMicrovia();

            // Blind/buried vias: mark as different from through-hole
            if (isBlindBuried || isMicrovia) {
                // Still plated, but tracked differently
            }
        } else if (auto* pad = dynamic_cast<PadItem*>(gItem)) {
            drill = pad->drillSize();
            if (drill <= 0.001) continue; // SMD pad, no drill

            pos = pad->scenePos();
            netName = pad->netName();

            // Find parent component for reference designator
            if (auto* parent = dynamic_cast<ComponentItem*>(pad->parentItem())) {
                itemName = parent->name();
            } else {
                itemName = "Pad";
            }

            isPlated = true; // Default: all pads with drills are plated through-hole

            // Check for oblong/slot pads
            if (pad->padShape() == "Oblong" || pad->padShape() == "Slot") {
                isSlot = true;
                // Approximate slot length from pad size
                QSizeF size = pad->size();
                slotLength = qMax(size.width(), size.height());
            }
        } else {
            continue;
        }

        if (isPlated == plated && drill > 0.001) {
            hole.pos = pos;
            hole.diameter = drill;
            hole.isSlot = isSlot;
            hole.slotLength = slotLength;
            hole.netName = netName;
            hole.itemName = itemName;
            hole.isPlated = isPlated;
            hole.isBlindBuried = isBlindBuried;
            hole.isMicrovia = isMicrovia;
            hole.startLayer = startLayer;
            hole.endLayer = endLayer;
            holes.append(hole);
        }
    }

    // Sort holes by tool code (drill size) for efficient drilling
    QMap<double, int> toolTable = collectToolTable(scene, plated);
    std::sort(holes.begin(), holes.end(), [&toolTable](const DrillHole& a, const DrillHole& b) {
        int toolA = toolTable.value(a.diameter, 999);
        int toolB = toolTable.value(b.diameter, 999);
        if (toolA != toolB) return toolA < toolB;
        return a.diameter < b.diameter;
    });

    return holes;
}

// ============================================================================
// Coordinate formatting
// ============================================================================

QString NCDrillExporter::formatCoordinate(double value, const DrillOptions& options) {
    // Convert to target unit
    double coord = value;
    if (options.unit == Inches) {
        coord = value * 0.0393701; // mm to inches
    }

    // Format with proper digits
    int integerDigits = options.integerDigits;
    int decimalDigits = options.decimalDigits;

    // Split into integer and decimal parts
    double intPart;
    double fracPart = modf(coord, &intPart);

    int intVal = qAbs(static_cast<int>(intPart));
    int fracVal = qRound(fracPart * qPow(10, decimalDigits));

    QString result;
    if (options.zeroSuppression == LeadingZeros) {
        // LZ: omit leading zeros
        result = QString("%1%2").arg(intVal).arg(fracVal, decimalDigits, 10, QChar('0'));
        // Remove leading zeros from integer part (but keep at least one digit)
        while (result.length() > 1 && result[0] == '0') {
            result.remove(0, 1);
        }
    } else {
        // TZ: omit trailing zeros
        result = QString("%1%2").arg(intVal).arg(fracVal, decimalDigits, 10, QChar('0'));
        // Remove trailing zeros
        while (result.length() > 1 && result.endsWith('0')) {
            result.chop(1);
        }
    }

    return result;
}

// ============================================================================
// File name generation
// ============================================================================

QString NCDrillExporter::generatePTHFileName(const QString& outputDirectory, const DrillOptions& options) {
    Q_UNUSED(options);
    return QDir(outputDirectory).filePath("PTH.drl");
}

QString NCDrillExporter::generateNPTHFileName(const QString& outputDirectory, const DrillOptions& options) {
    Q_UNUSED(options);
    return QDir(outputDirectory).filePath("NPTH.drl");
}

// ============================================================================
// Header writing
// ============================================================================

void NCDrillExporter::writeHeader(QTextStream& out, const DrillOptions& options) {
    if (!options.includeHeaderComments) {
        out << "M48\n";
        QString unitStr = (options.unit == Millimeters) ? "METRIC" : "INCH";
        QString zeroStr = (options.zeroSuppression == LeadingZeros) ? "LZ" : "TZ";
        out << QString("%1,%2\n").arg(unitStr, zeroStr);
        out << "FMAT,2\n"; // Excellon-2
        out << "%\n";
        out << "G90\n"; // Absolute mode
        out << "G05\n"; // Drill mode
        return;
    }

    out << ";================================================================\n";
    out << "; NC Drill File (Excellon-2 Format)\n";
    out << "; Generated by VioraEDA\n";
    out << "; Date: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    out << ";================================================================\n";

    if (!options.partName.isEmpty()) {
        out << "; PARTNAME: " << options.partName << "\n";
    }

    out << "M48\n";
    out << "; DRILL file for VioraEDA PCB\n";
    out << "; FORMAT={";

    // Format specification
    QString unitStr = (options.unit == Millimeters) ? "METRIC" : "INCH";
    QString zeroStr = (options.zeroSuppression == LeadingZeros) ? "LZ" : "TZ";
    out << unitStr << "," << zeroStr << "}\n";

    out << QString("FMAT,%1\n").arg(options.format == Excellon2 ? 2 : 1);

    // Coordinate format specification
    // FSLAX{integer}{decimal}Y{integer}{decimal}
    out << QString("FSLAX%1%2Y%1%2\n").arg(options.integerDigits).arg(options.decimalDigits);

    if (options.programmingOrigin != QPointF(0, 0)) {
        // G04 offset comment (not all parsers support G04, use comment)
        out << QString("; PROGRAMMING ORIGIN: X=%1 Y=%2\n")
            .arg(options.programmingOrigin.x(), 0, 'f', 3)
            .arg(options.programmingOrigin.y(), 0, 'f', 3);
    }

    out << "%\n";
    out << "G90\n"; // Absolute positioning mode
    out << "G05\n"; // Drill mode
}

// ============================================================================
// Tool table writing
// ============================================================================

void NCDrillExporter::writeToolTable(QTextStream& out, const QMap<double, int>& toolTable,
                                      const DrillOptions& options) {
    if (toolTable.isEmpty()) return;

    for (auto it = toolTable.begin(); it != toolTable.end(); ++it) {
        double sizeMm = it.key();
        int toolCode = it.value();

        // Convert to display unit
        double displaySize = sizeMm;
        if (options.unit == Inches) {
            displaySize = sizeMm * 0.0393701;
        }

        // T{code}C{diameter} - Circular hole
        out << QString("T%1C%2\n").arg(toolCode).arg(displaySize, 0, 'f', options.decimalDigits);
    }
}

// ============================================================================
// Hole writing
// ============================================================================

void NCDrillExporter::writeHoles(QTextStream& out, const QList<DrillHole>& holes,
                                  const QMap<double, int>& toolTable, const DrillOptions& options) {
    int currentTool = -1;

    for (const DrillHole& hole : holes) {
        int toolCode = toolTable.value(hole.diameter, -1);
        if (toolCode < 0) continue;

        // Tool change if needed
        if (toolCode != currentTool) {
            out << QString("T%1\n").arg(toolCode);
            currentTool = toolCode;
        }

        // Apply programming origin offset
        QPointF offsetPos = hole.pos - options.programmingOrigin;

        if (hole.isSlot && options.includeSlotRouting) {
            // G85 slot routing for oblong holes
            // For a slot: start point and end point along the long axis
            double halfLen = (hole.slotLength - hole.diameter) / 2.0;
            // Simplified: assume horizontal slot
            QPointF startPos = offsetPos - QPointF(halfLen, 0);
            QPointF endPos = offsetPos + QPointF(halfLen, 0);

            out << "G85\n"; // Slot routing mode
            out << QString("X%1Y%2\n")
                .arg(formatCoordinate(startPos.x(), options))
                .arg(formatCoordinate(startPos.y(), options));
            out << QString("X%1Y%2\n")
                .arg(formatCoordinate(endPos.x(), options))
                .arg(formatCoordinate(endPos.y(), options));
            out << "G05\n"; // Back to drill mode
        } else {
            // Standard drill
            out << QString("X%1Y%2\n")
                .arg(formatCoordinate(offsetPos.x(), options))
                .arg(formatCoordinate(offsetPos.y(), options));
        }
    }
}

// ============================================================================
// Footer writing
// ============================================================================

void NCDrillExporter::writeFooter(QTextStream& out, const DrillOptions& options) {
    Q_UNUSED(options);
    out << "T0\n";    // Deselect tool
    out << "M30\n";   // End of program
    out << "%\n";     // End of file
}
