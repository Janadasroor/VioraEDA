#ifndef SIM_REPORT_GENERATOR_H
#define SIM_REPORT_GENERATOR_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QJsonObject>
#include <QImage>
#include "sim_results.h"

class SimReportGenerator {
public:
    struct ReportOptions {
        bool includeSchematic = true;
        bool includeWaveforms = true;
        bool includeMeasurements = true;
        bool includeNetlist = true;
        bool includeMetadata = true;
        int maxWaveformPoints = 1000;
        QString title;
        QString author;
    };

    SimReportGenerator();
    ~SimReportGenerator();

    void setSchematicPath(const QString& path);
    void setSchematicImage(const QImage& image);
    void setSimulationResults(const SimResults& results);
    void setNetlist(const QString& netlist);
    void setOptions(const ReportOptions& opts);

    QString generateHtml();
    bool saveToFile(const QString& path);

private:
    QString generateHtmlHeader();
    QString generateHtmlFooter();
    QString generateSchematicSection();
    QString generateWaveformSection();
    QString generateMeasurementsSection();
    QString generateNetlistSection();
    QString generateMetadataSection();
    QString generateCssStyles();
    QString generateJsScripts();

    QString waveformToSvg(const SimWaveform& wf, const QString& color, int width, int height);
    QString escapeHtml(const QString& str);
    QString imageToBase64(const QImage& image);

    QString m_schematicPath;
    QImage m_schematicImage;
    QString m_netlist;
    SimResults m_results;
    ReportOptions m_options;
};

#endif // SIM_REPORT_GENERATOR_H