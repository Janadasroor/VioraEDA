#ifndef NC_DRILL_EXPORTER_H
#define NC_DRILL_EXPORTER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QTextStream>

class QGraphicsScene;

/**
 * @brief Proper NC Drill / Excellon-2 format exporter.
 * 
 * Produces industry-standard drill files with:
 * - Dynamic tool table (unique drill sizes → tool codes)
 * - PTH (Plated Through Hole) and NPTH (Non-Plated) separation
 * - Proper integer coordinate format (LZ/TZ)
 * - Slot/oblong hole support (G85 routing)
 * - Blind/buried via and microvia handling
 * - Programming origin offset
 * - Metadata comments (date, part name, format version)
 * 
 * Excellon-2 format reference:
 * https://www.excellon.com/manuals/ProgramManual.htm
 */
class NCDrillExporter {
public:
    enum Format {
        Excellon2,    // Standard Excellon-2 (most common)
        SiebMeyer,    // Sieb & Meyer format (some European fabs)
        Generic       // Simple generic format
    };

    enum Unit {
        Millimeters,
        Inches
    };

    enum ZeroSuppression {
        LeadingZeros,   // LZ: 0.12345 → .12345
        TrailingZeros   // TZ: 1.23450 → 1.2345
    };

    struct DrillOptions {
        Format format = Excellon2;
        Unit unit = Millimeters;
        ZeroSuppression zeroSuppression = LeadingZeros;
        int integerDigits = 3;     // Integer part digits (e.g., FSLAX34)
        int decimalDigits = 4;     // Decimal part digits
        QPointF programmingOrigin; // G04 offset (0,0 = absolute)
        bool separatePTH = true;   // Generate separate PTH and NPTH files
        bool includeSlotRouting = true; // G85 for oblong/slot holes
        bool includeToolList = true; // Tool list at end of file
        QString partName;          // PARTNAME comment
        bool includeHeaderComments = true;
    };

    /**
     * @brief Result of drill export, including file paths.
     */
    struct DrillExportResult {
        bool success = false;
        QString pthFilePath;      // Plated Through Holes
        QString npthFilePath;     // Non-Plated Through Holes
        QString error;
        int totalHoles = 0;
        int totalSlots = 0;
        int toolCount = 0;
        QString summary;
    };

    /**
     * @brief Export drill files from the PCB scene.
     * @param scene The PCB scene
     * @param outputDirectory Directory for output files
     * @param options Export options
     * @return Result with file paths and statistics
     */
    static DrillExportResult exportDrills(QGraphicsScene* scene,
                                           const QString& outputDirectory,
                                           const DrillOptions& options);

    /**
     * @brief Generate drill file content as a string (without writing to disk).
     * @param scene The PCB scene
     * @param plated true = PTH, false = NPTH
     * @param options Export options
     * @return Excellon format content
     */
    static QString generateDrillContent(QGraphicsScene* scene, bool plated,
                                         const DrillOptions& options);

    /**
     * @brief Collect unique drill sizes from the scene and assign tool codes.
     * @param scene The PCB scene
     * @param plated true = only plated holes, false = only non-plated
     * @return Map of drill size (mm) → tool code (T1, T2, ...)
     */
    static QMap<double, int> collectToolTable(QGraphicsScene* scene, bool plated);

    /**
     * @brief Check if the scene has any non-plated holes.
     */
    static bool hasNonPlatedHoles(QGraphicsScene* scene);

private:
    struct DrillHole {
        QPointF pos;           // Position in scene coords (mm)
        double diameter = 0;   // Hole diameter in mm
        double rotation = 0;   // Rotation in degrees
        bool isSlot = false;   // Oblong/slot hole
        double slotLength = 0; // For slots: total length (diameter + straight portion)
        QString netName;       // Associated net
        QString itemName;      // Component reference or "Via"
        bool isPlated = true;  // PTH vs NPTH
        bool isBlindBuried = false;
        bool isMicrovia = false;
        int startLayer = 0;
        int endLayer = 1;
    };

    static QList<DrillHole> collectHoles(QGraphicsScene* scene, bool plated);
    static QString formatCoordinate(double value, const DrillOptions& options);
    static QString generatePTHFileName(const QString& outputDirectory, const DrillOptions& options);
    static QString generateNPTHFileName(const QString& outputDirectory, const DrillOptions& options);
    static void writeHeader(QTextStream& out, const DrillOptions& options);
    static void writeToolTable(QTextStream& out, const QMap<double, int>& toolTable, const DrillOptions& options);
    static void writeHoles(QTextStream& out, const QList<DrillHole>& holes,
                           const QMap<double, int>& toolTable, const DrillOptions& options);
    static void writeFooter(QTextStream& out, const DrillOptions& options);
};

#endif // NC_DRILL_EXPORTER_H
