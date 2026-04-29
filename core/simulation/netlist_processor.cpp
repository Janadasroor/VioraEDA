#include "netlist_processor.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

namespace Flux {

NetlistProcessor::Result NetlistProcessor::process(const QString& netlistPath) {
    Result result;
    result.success = false;

    QFile file(netlistPath);
    if (!file.exists()) {
        result.error = "Netlist file not found.";
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = "Cannot open netlist file.";
        return result;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        result.lines << in.readLine();
    }
    file.close();

    if (result.lines.isEmpty()) {
        result.error = "Netlist file is empty.";
        return result;
    }

    // Remove BOM if present
    if (result.lines.first().startsWith(QChar(0xFEFF))) {
        result.lines.first().remove(0, 1);
    }

    QDir baseDir = QFileInfo(netlistPath).absoluteDir();
    resolveWavPaths(result.lines, baseDir);
    stripControlBlocks(result.lines);
    ensureHeaderAndEnd(result.lines);

    result.success = true;
    return result;
}

void NetlistProcessor::resolveWavPaths(QStringList& lines, const QDir& baseDir) {
    QRegularExpression wavRe(R"REGEX(WAVEFILE\s*=?\s*"([^"]+)")REGEX", QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < lines.size(); ++i) {
        auto match = wavRe.match(lines[i]);
        if (match.hasMatch()) {
            QString rawPath = match.captured(1);
            QString fullPath = QFileInfo(rawPath).isAbsolute() ? rawPath : baseDir.absoluteFilePath(rawPath);
            QString resolved = resolveCaseInsensitiveFilePath(fullPath);
            
            if (resolved != fullPath) {
                lines[i].replace(rawPath, resolved);
                qInfo() << "[NetlistProcessor] Auto-corrected WAV path case:" << resolved;
            }
        }
    }
}

QString NetlistProcessor::resolveCaseInsensitiveFilePath(const QString& path) {
    QFileInfo fi(path);
    if (fi.exists()) return path;
    
    QDir dir(fi.absolutePath());
    if (!dir.exists()) return path;
    
    QString target = fi.fileName().toLower();
    const auto entries = dir.entryList(QDir::Files);
    for (const QString& entry : entries) {
        if (entry.toLower() == target) {
            return dir.absoluteFilePath(entry);
        }
    }
    return path;
}

void NetlistProcessor::stripControlBlocks(QStringList& lines) {
    QStringList filtered;
    bool inControl = false;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed().toLower();
        if (trimmed.startsWith(".control")) { inControl = true; continue; }
        if (inControl) {
            if (trimmed.startsWith(".endc")) inControl = false;
            continue;
        }
        filtered << line;
    }
    lines = filtered;
}

void NetlistProcessor::ensureHeaderAndEnd(QStringList& lines) {
    int firstNonEmpty = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (!lines.at(i).trimmed().isEmpty()) { firstNonEmpty = i; break; }
    }
    
    if (firstNonEmpty >= 0) {
        const QString head = lines.at(firstNonEmpty).trimmed();
        if (head.startsWith(".") || head.startsWith("*")) {
            lines.insert(firstNonEmpty, "Viospice Netlist");
        }
    } else {
        lines << "Viospice Netlist";
    }

    bool hasEnd = false;
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString trimmed = lines.at(i).trimmed();
        if (trimmed.isEmpty()) continue;
        hasEnd = trimmed.toLower().startsWith(".end");
        break;
    }
    if (!hasEnd) lines << ".end";
}

} // namespace Flux
