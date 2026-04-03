#ifndef MANUFACTURING_EXPORTER_H
#define MANUFACTURING_EXPORTER_H

#include <QString>

class QGraphicsScene;

class ManufacturingExporter {
public:
    static bool exportIPC2581(QGraphicsScene* scene, const QString& filePath, QString* error = nullptr);
    static bool exportODBppPackage(QGraphicsScene* scene, const QString& outputDirectory, QString* error = nullptr);

    // Pick-and-Place / Centroid file export
    enum PickPlaceFormat {
        CSV,       // Comma-separated values
        TSV        // Tab-separated values (for Excel)
    };

    struct PickPlaceOptions {
        PickPlaceFormat format = CSV;
        bool includeTopSide = true;
        bool includeBottomSide = true;
        bool includeFiducials = false;
        bool includeTestPoints = false;
        bool useMillimeters = true;  // false = inches
        bool includeValue = true;
        bool includeFootprint = true;
    };

    static bool exportPickPlace(QGraphicsScene* scene, const QString& filePath,
                                const PickPlaceOptions& options,
                                QString* error = nullptr);

    // Generate CSV/TSV content without writing to file
    static QString generatePickPlaceContent(QGraphicsScene* scene,
                                            const PickPlaceOptions& options,
                                            QString* error = nullptr);
};

#endif // MANUFACTURING_EXPORTER_H
