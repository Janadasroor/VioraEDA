#include "sim_report_generator.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QByteArray>
#include <QBuffer>
#include <cmath>

SimReportGenerator::SimReportGenerator() {
}

SimReportGenerator::~SimReportGenerator() {
}

void SimReportGenerator::setSchematicPath(const QString& path) {
    m_schematicPath = path;
}

void SimReportGenerator::setSchematicImage(const QImage& image) {
    m_schematicImage = image;
}

void SimReportGenerator::setSimulationResults(const SimResults& results) {
    m_results = results;
}

void SimReportGenerator::setNetlist(const QString& netlist) {
    m_netlist = netlist;
}

void SimReportGenerator::setOptions(const ReportOptions& opts) {
    m_options = opts;
}

QString SimReportGenerator::imageToBase64(const QImage& image) {
    if (image.isNull()) return QString();
    
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    
    return QString::fromLatin1(ba.toBase64());
}

QString SimReportGenerator::escapeHtml(const QString& str) {
    QString escaped = str;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

QString SimReportGenerator::generateHtml() {
    QString html;
    html += generateHtmlHeader();
    html += generateCssStyles();
    html += "</head><body>\n";
    
    if (m_options.includeMetadata) {
        html += generateMetadataSection();
    }
    if (m_options.includeSchematic) {
        html += generateSchematicSection();
    }
    if (m_options.includeWaveforms) {
        html += generateWaveformSection();
    }
    if (m_options.includeMeasurements) {
        html += generateMeasurementsSection();
    }
    if (m_options.includeNetlist) {
        html += generateNetlistSection();
    }
    
    html += generateJsScripts();
    html += generateHtmlFooter();
    return html;
}

QString SimReportGenerator::generateHtmlHeader() {
    QString title = m_options.title.isEmpty() ? "VioSpice Design Review" : m_options.title;
    return QString(
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>%1</title>\n"
    ).arg(escapeHtml(title));
}

QString SimReportGenerator::generateHtmlFooter() {
    return QString(
        "</body>\n"
        "</html>\n"
    );
}

QString SimReportGenerator::generateCssStyles() {
    return QString(
        "  <style>\n"
        "    :root {\n"
        "      --bg-primary: #0d1117;\n"
        "      --bg-secondary: #161b22;\n"
        "      --bg-tertiary: #21262d;\n"
        "      --text-primary: #e6edf3;\n"
        "      --text-secondary: #8b949e;\n"
        "      --accent: #58a6ff;\n"
        "      --accent-hover: #79c0ff;\n"
        "      --border: #30363d;\n"
        "      --success: #3fb950;\n"
        "      --warning: #d29922;\n"
        "      --error: #f85149;\n"
        "    }\n"
        "    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
        "    body {\n"
        "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;\n"
        "      background: var(--bg-primary);\n"
        "      color: var(--text-primary);\n"
        "      line-height: 1.6;\n"
        "      padding: 20px;\n"
        "    }\n"
        "    .container { max-width: 1200px; margin: 0 auto; }\n"
        "    h1, h2, h3 { margin-top: 1.5em; margin-bottom: 0.5em; }\n"
        "    h1 { color: var(--accent); border-bottom: 2px solid var(--border); padding-bottom: 0.5em; }\n"
        "    h2 { color: var(--text-primary); }\n"
        "    .card {\n"
        "      background: var(--bg-secondary);\n"
        "      border: 1px solid var(--border);\n"
        "      border-radius: 8px;\n"
        "      padding: 20px;\n"
        "      margin: 1em 0;\n"
        "    }\n"
        "    .schematic-img {\n"
        "      max-width: 100%;\n"
        "      height: auto;\n"
        "      border-radius: 4px;\n"
        "    }\n"
        "    table {\n"
        "      width: 100%;\n"
        "      border-collapse: collapse;\n"
        "      margin: 1em 0;\n"
        "    }\n"
        "    th, td {\n"
        "      padding: 10px;\n"
        "      text-align: left;\n"
        "      border-bottom: 1px solid var(--border);\n"
        "    }\n"
        "    th { background: var(--bg-tertiary); color: var(--text-secondary); }\n"
        "    tr:hover { background: var(--bg-tertiary); }\n"
        "    .waveform-container {\n"
        "      overflow-x: auto;\n"
        "      padding: 10px;\n"
        "    }\n"
        "    .waveform-container svg { display: block; }\n"
        "    .netlist-code {\n"
        "      background: var(--bg-tertiary);\n"
        "      padding: 15px;\n"
        "      border-radius: 6px;\n"
        "      overflow-x: auto;\n"
        "      font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Fira Code', monospace;\n"
        "      font-size: 13px;\n"
        "      line-height: 1.5;\n"
        "      white-space: pre;\n"
        "    }\n"
        "    .metadata-grid {\n"
        "      display: grid;\n"
        "      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\n"
        "      gap: 15px;\n"
        "    }\n"
        "    .metadata-item {\n"
        "      background: var(--bg-tertiary);\n"
        "      padding: 12px;\n"
        "      border-radius: 6px;\n"
        "    }\n"
        "    .metadata-label {\n"
        "      color: var(--text-secondary);\n"
        "      font-size: 0.85em;\n"
        "    }\n"
        "    .metadata-value {\n"
        "      font-size: 1.1em;\n"
        "      font-weight: 500;\n"
        "    }\n"
        "    .badge {\n"
        "      display: inline-block;\n"
        "      padding: 3px 8px;\n"
        "      border-radius: 12px;\n"
        "      font-size: 0.8em;\n"
        "      font-weight: 500;\n"
        "    }\n"
        "    .badge-op { background: #388bfd; color: white; }\n"
        "    .badge-tran { background: #a371f7; color: white; }\n"
        "    .badge-ac { background: #3fb950; color: white; }\n"
        "    .footer {\n"
        "      margin-top: 3em;\n"
        "      padding-top: 1em;\n"
        "      border-top: 1px solid var(--border);\n"
        "      color: var(--text-secondary);\n"
        "      text-align: center;\n"
        "      font-size: 0.9em;\n"
        "    }\n"
        "    @media print {\n"
        "      body { background: white; color: black; }\n"
        "      .card { border: 1px solid #ccc; }\n"
        "    }\n"
        "  </style>\n"
    );
}

QString SimReportGenerator::generateMetadataSection() {
    QString analysisTypeStr;
    switch (m_results.analysisType) {
        case SimAnalysisType::OP: analysisTypeStr = "Operating Point"; break;
        case SimAnalysisType::Transient: analysisTypeStr = "Transient"; break;
        case SimAnalysisType::AC: analysisTypeStr = "AC Analysis"; break;
        case SimAnalysisType::Noise: analysisTypeStr = "Noise"; break;
        default: analysisTypeStr = "Unknown";
    }

    QString badgeClass;
    switch (m_results.analysisType) {
        case SimAnalysisType::OP: badgeClass = "badge-op"; break;
        case SimAnalysisType::Transient: badgeClass = "badge-tran"; break;
        case SimAnalysisType::AC: badgeClass = "badge-ac"; break;
        default: badgeClass = "badge-op";
    }

    QString title = m_options.title.isEmpty() ? "VioSpice Design Review" : m_options.title;
    QString author = m_options.author.isEmpty() ? "VioSpice" : m_options.author;
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    return QString(
        "  <div class=\"container\">\n"
        "    <h1>%1</h1>\n"
        "    <div class=\"card\">\n"
        "      <div class=\"metadata-grid\">\n"
        "        <div class=\"metadata-item\">\n"
        "          <div class=\"metadata-label\">Analysis Type</div>\n"
        "          <div class=\"metadata-value\"><span class=\"badge %5\">%2</span></div>\n"
        "        </div>\n"
        "        <div class=\"metadata-item\">\n"
        "          <div class=\"metadata-label\">Generated</div>\n"
        "          <div class=\"metadata-value\">%3</div>\n"
        "        </div>\n"
        "        <div class=\"metadata-item\">\n"
        "          <div class=\"metadata-label\">Author</div>\n"
        "          <div class=\"metadata-value\">%4</div>\n"
        "        </div>\n"
        "        <div class=\"metadata-item\">\n"
        "          <div class=\"metadata-label\">Waveforms</div>\n"
        "          <div class=\"metadata-value\">%6</div>\n"
        "        </div>\n"
        "        <div class=\"metadata-item\">\n"
        "          <div class=\"metadata-label\">Measurements</div>\n"
        "          <div class=\"metadata-value\">%7</div>\n"
        "        </div>\n"
        "      </div>\n"
        "    </div>\n"
    ).arg(escapeHtml(title))
     .arg(analysisTypeStr)
     .arg(timestamp)
     .arg(escapeHtml(author))
     .arg(badgeClass)
     .arg(m_results.waveforms.size())
     .arg(m_results.measurements.size());
}

QString SimReportGenerator::generateSchematicSection() {
    if (m_schematicPath.isEmpty() && m_schematicImage.isNull()) {
        return QString();
    }

    QString section = "    <h2>Schematic</h2>\n    <div class=\"card\">\n";
    
    if (!m_schematicImage.isNull()) {
        QString base64 = imageToBase64(m_schematicImage);
        section += QString(
            "      <img src=\"data:image/png;base64,%1\" class=\"schematic-img\" alt=\"Schematic\">\n"
        ).arg(base64);
    }
    
    if (!m_schematicPath.isEmpty()) {
        section += QString(
            "      <p style=\"color: var(--text-secondary); margin-top: 1em;\">\n"
            "        Source: %1\n"
            "      </p>\n"
        ).arg(escapeHtml(m_schematicPath));
    }
    
    section += "    </div>\n";
    return section;
}

QString SimReportGenerator::generateWaveformSection() {
    if (m_results.waveforms.empty()) {
        return QString();
    }

    QString html = "    <h2>Waveforms</h2>\n    <div class=\"card\">\n";
    
    static const QStringList colors = {
        "#58a6ff", "#3fb950", "#f85149", "#d29922", 
        "#a371f7", "#79c0ff", "#7ee787", "#ffa657"
    };

    for (size_t i = 0; i < m_results.waveforms.size(); ++i) {
        const auto& wf = m_results.waveforms[i];
        const QString& color = colors[i % colors.size()];
        QString svg = waveformToSvg(wf, color, 800, 200);
        
        html += QString(
            "      <div class=\"waveform-container\">\n"
            "        <h3 style=\"color: %2;\">%1</h3>\n"
            "        %3\n"
            "      </div>\n"
        ).arg(escapeHtml(QString::fromStdString(wf.name)))
         .arg(color)
         .arg(svg);
    }

    html += "    </div>\n";
    return html;
}

QString SimReportGenerator::waveformToSvg(const SimWaveform& wf, const QString& color, int width, int height) {
    if (wf.xData.empty() || wf.yData.empty()) {
        return "<p style=\"color: var(--text-secondary);\">No data</p>";
    }

    int numPoints = qMin(static_cast<int>(wf.xData.size()), m_options.maxWaveformPoints);
    if (numPoints < 2) {
        return "<p style=\"color: var(--text-secondary);\">Insufficient data points</p>";
    }

    double minX = wf.xData[0];
    double maxX = wf.xData[wf.xData.size() - 1];
    double minY = wf.yData[0];
    double maxY = wf.yData[0];
    
    for (int i = 0; i < wf.xData.size(); ++i) {
        minX = qMin(minX, wf.xData[i]);
        maxX = qMax(maxX, wf.xData[i]);
        minY = qMin(minY, wf.yData[i]);
        maxY = qMax(maxY, wf.yData[i]);
    }
    
    double rangeX = maxX - minX;
    double rangeY = maxY - minY;
    if (rangeX < 1e-12) rangeX = 1;
    if (rangeY < 1e-12) rangeY = 1;

    auto scaleX = [&](double x) -> double {
        return 50 + ((x - minX) / rangeX) * (width - 100);
    };
    auto scaleY = [&](double y) -> double {
        return height - 30 - ((y - minY) / rangeY) * (height - 60);
    };

    QString path;
    for (size_t i = 0; i < wf.xData.size(); ++i) {
        double x = scaleX(wf.xData[i]);
        double y = scaleY(wf.yData[i]);
        if (i == 0) {
            path += QString("M %1 %2").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        } else {
            path += QString(" L %1 %2").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        }
    }

    QString xAxisLabel = QString::fromStdString(m_results.xAxisName);
    QString yAxisLabel = QString::fromStdString(m_results.yAxisName);

    return QString(
        "<svg width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\" style=\"background: var(--bg-tertiary); border-radius: 4px;\">\n"
        "  <line x1=\"50\" y1=\"%3\" x2=\"%4\" y2=\"%3\" stroke=\"%5\" stroke-width=\"1\"/>\n"
        "  <line x1=\"50\" y1=\"30\" x2=\"50\" y2=\"%3\" stroke=\"%5\" stroke-width=\"1\"/>\n"
        "  <text x=\"%6\" y=\"%7\" fill=\"%5\" font-size=\"12\">%8</text>\n"
        "  <text x=\"%9\" y=\"%10\" fill=\"%5\" font-size=\"12\">%11</text>\n"
        "  <path d=\"%12\" fill=\"none\" stroke=\"%13\" stroke-width=\"2\" stroke-linejoin=\"round\"/>\n"
        "</svg>"
    ).arg(width).arg(height)
     .arg(height - 30)
     .arg(width - 50)
     .arg("#8b949e")
     .arg(width / 2).arg(height - 5)
     .arg(escapeHtml(xAxisLabel))
     .arg(30).arg(20)
     .arg(escapeHtml(yAxisLabel))
     .arg(path)
     .arg(color);
}

QString SimReportGenerator::generateMeasurementsSection() {
    if (m_results.measurements.empty()) {
        return QString();
    }

    QString html = "    <h2>Measurements</h2>\n    <div class=\"card\">\n      <table>\n        <thead>\n          <tr>\n            <th>Name</th>\n            <th>Value</th>\n            <th>Unit</th>\n          </tr>\n        </thead>\n        <tbody>\n";

    for (const auto& entry : m_results.measurements) {
        QString name = QString::fromStdString(entry.first);
        double value = entry.second;
        
        QString unit = "";
        if (qAbs(value) >= 1e6) {
            unit = "M";
            value /= 1e6;
        } else if (qAbs(value) >= 1e3) {
            unit = "k";
            value /= 1e3;
        } else if (qAbs(value) < 1e-6) {
            unit = "u";
            value *= 1e6;
        } else if (qAbs(value) < 1e-3) {
            unit = "m";
            value *= 1e3;
        }

        html += QString(
            "          <tr>\n"
            "            <td>%1</td>\n"
            "            <td>%2</td>\n"
            "            <td>%3</td>\n"
            "          </tr>\n"
        ).arg(escapeHtml(name))
         .arg(value, 0, 'f', 4)
         .arg(unit);
    }

    html += "        </tbody>\n      </table>\n    </div>\n";
    return html;
}

QString SimReportGenerator::generateNetlistSection() {
    if (m_netlist.isEmpty()) {
        return QString();
    }

    QString escapedNetlist = escapeHtml(m_netlist);
    return QString(
        "    <h2>Netlist</h2>\n"
        "    <div class=\"card\">\n"
        "      <pre class=\"netlist-code\">%1</pre>\n"
        "    </div>\n"
    ).arg(escapedNetlist);
}

QString SimReportGenerator::generateJsScripts() {
    return QString(
        "  <script>\n"
        "    document.addEventListener('DOMContentLoaded', function() {\n"
        "      console.log('VioSpice Design Review Report loaded');\n"
        "    });\n"
        "  </script>\n"
    );
}

bool SimReportGenerator::saveToFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << generateHtml();
    file.close();
    
    return true;
}