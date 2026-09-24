/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

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
    commentOutAppDirectives(result.lines);
    QStringList liftedAnalyses;
    stripControlBlocks(result.lines, &liftedAnalyses);
    // Decks that keep their analysis inside .control (e.g. "tran 2u 360m")
    // would otherwise simulate nothing after stripping: bg_run with no
    // analysis is a silent no-op and the scope keeps showing stale results.
    // Re-inject the lifted analyses as directives when the deck declares none.
    if (!liftedAnalyses.isEmpty() && !hasAnalysisDirective(result.lines)) {
        int endIdx = -1;
        for (int i = result.lines.size() - 1; i >= 0; --i) {
            if (result.lines.at(i).trimmed().toLower().startsWith(".end")) { endIdx = i; break; }
        }
        if (endIdx < 0) { result.lines << ".end"; endIdx = result.lines.size() - 1; }
        for (int i = liftedAnalyses.size() - 1; i >= 0; --i)
            result.lines.insert(endIdx, liftedAnalyses.at(i));
        qInfo() << "[NetlistProcessor] Lifted" << liftedAnalyses.size()
                << "analyse(s) from .control:" << liftedAnalyses;
    }
    ensureHeaderAndEnd(result.lines);

    result.success = true;
    return result;
}

void NetlistProcessor::resolveWavPaths(QStringList& lines, const QDir& baseDir) {
    static const QRegularExpression wavRe(R"REGEX(WAVEFILE\s*=?\s*"([^"]+)")REGEX", QRegularExpression::CaseInsensitiveOption);

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

void NetlistProcessor::stripControlBlocks(QStringList& lines, QStringList* liftedAnalyses) {
    // Bare analysis commands valid both as control lines and as directives.
    static const QRegularExpression analysisRe(
        QStringLiteral("^\\s*(tran|ac|dc|op|noise)\\b(.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    QStringList filtered;
    bool inControl = false;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        const QString lowered = trimmed.toLower();
        if (lowered.startsWith(".control")) { inControl = true; continue; }
        if (inControl) {
            if (lowered.startsWith(".endc")) { inControl = false; continue; }
            if (liftedAnalyses && !trimmed.isEmpty()
                && trimmed[0] != '*' && trimmed[0] != '#' && trimmed[0] != ';') {
                const auto m = analysisRe.match(trimmed);
                if (m.hasMatch())
                    liftedAnalyses->append(QStringLiteral(".%1%2").arg(m.captured(1).toLower(), m.captured(2)));
            }
            continue;
        }
        filtered << line;
    }
    lines = filtered;
}

bool NetlistProcessor::hasAnalysisDirective(const QStringList& lines) {
    static const QRegularExpression directiveRe(
        QStringLiteral("^\\s*\\.(tran|ac|dc|op|noise|disto|sens|tf|pz)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QString& line : lines) {
        if (directiveRe.match(line).hasMatch()) return true;
    }
    return false;
}

void NetlistProcessor::commentOutAppDirectives(QStringList& lines) {
    static const QRegularExpression appDirectiveRe(
        QStringLiteral("^\\s*\\.(interactive|sp|net)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    for (QString& line : lines) {
        if (appDirectiveRe.match(line).hasMatch()) {
            line = QStringLiteral("* VioSpice evaluates post-simulation: %1").arg(line);
        }
    }
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
