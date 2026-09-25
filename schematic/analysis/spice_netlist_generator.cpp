/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "passes/component_formatter.h"
#include "spice_netlist_generator.h"
#include "passes/component_extractor.h"
#include "passes/xspice_block_translator.h"
#include "passes/connectivity_evaluator.h"
#include "passes/model_injector.h"
#include "passes/netlist_formatter.h"
#include "passes/spice_compat_rewriter.h"
#include "../items/schematic_item.h"
#include "../items/smart_signal_item.h"
#include "../items/virtual_terminal_item.h"
#include "net_manager.h"
#include "../io/netlist_generator.h"
#include "../../symbols/symbol_library.h"
#include "../../symbols/models/symbol_definition.h"
#include "../../simulator/bridge/model_library_manager.h"
#include "../items/schematic_spice_directive_item.h"
#include "../items/tuning_slider_symbol_item.h"
#include <QGraphicsScene>
#include <QSet>
#include <QMap>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <cmath>
#include <limits>
#include <QJsonDocument>
#include <QJsonObject>
#include "config_manager.h"
#include "simulation_manager.h"
#include "../../simulator/core/sim_value_parser.h"
#include "../../simulator/mixedmode/NetlistManager.h"

using Flux::Model::SymbolDefinition;

namespace {
struct UserSpiceContentSummary {
    QSet<QString> declaredModelFiles;
    QSet<QString> declaredModelNames;
    QSet<QString> declaredElementRefs;
    QSet<QString> drivenRailNets;
    QStringList warnings;
    bool hasExplicitAnalysisCard = false;
    bool hasElementCards = false;
    bool hasLtStartup = false;
    bool hasExplicitSaveDirective = false;
    bool hasNetDirective = false;
    bool isSParameter = false;
};


} // namespace
























namespace {

QStringList splitTopLevelSpiceArgs(const QString& text) {
    QStringList args;
    QString current;
    int parenDepth = 0;
    int braceDepth = 0;

    for (QChar ch : text) {
        if (ch == ',' && parenDepth == 0 && braceDepth == 0) {
            args.append(current.trimmed());
            current.clear();
            continue;
        }
        if (ch == '(') ++parenDepth;
        else if (ch == ')' && parenDepth > 0) --parenDepth;
        else if (ch == '{') ++braceDepth;
        else if (ch == '}' && braceDepth > 0) --braceDepth;
        current += ch;
    }
    args.append(current.trimmed());
    return args;
}

int findMatchingParen(const QString& text, int openIndex) {
    if (openIndex < 0 || openIndex >= text.size() || text.at(openIndex) != '(') return -1;
    int depth = 0;
    for (int i = openIndex; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == '(') ++depth;
        else if (ch == ')') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return -1;
}






} // namespace









namespace {

QString sanitizeDirectiveName(const QString& raw) {
    QString s = raw;
    s.replace(QRegularExpression("[^A-Za-z0-9_]"), "_");
    s.replace(QRegularExpression("_+"), "_");
    s.remove(QRegularExpression("^_+"));
    s.remove(QRegularExpression("_+$"));
    return s.isEmpty() ? QString("m") : s.left(40);
}

QString normalizeLtMeasDirective(const QString& cmd, QStringList* warnings = nullptr) {
    QString out = cmd;

    if (!out.startsWith(".meas", Qt::CaseInsensitive)) return out;

    if (out.contains("I(", Qt::CaseInsensitive)) {
        if (warnings) {
            warnings->append(QString("LT-style .meas current reference detected: %1").arg(cmd.trimmed()));
            warnings->append(QString("Consider measuring source current via I(Vsense) or converting resistor current measurements manually for ngspice."));
        }
    }

    if (out.contains(" PARAM ", Qt::CaseInsensitive)) {
        if (warnings) {
            warnings->append(QString(".meas PARAM detected and passed through unchanged: %1").arg(cmd.trimmed()));
        }
    }

    if (out.contains(" FIND ", Qt::CaseInsensitive) && out.contains(" AT=", Qt::CaseInsensitive)) {
        if (warnings) {
            warnings->append(QString(".meas FIND ... AT= detected; verify LT/ngspice syntax compatibility: %1").arg(cmd.trimmed()));
        }
    }

    if (out.contains(QRegularExpression("\\bDERIV\\b", QRegularExpression::CaseInsensitiveOption))) {
        if (warnings) {
            warnings->append(QString(".meas DERIV detected; verify LT/ngspice derivative measurement syntax compatibility: %1").arg(cmd.trimmed()));
        }
    }

    if (out.contains(QRegularExpression("\\bTRIG\\b", QRegularExpression::CaseInsensitiveOption)) ||
        out.contains(QRegularExpression("\\bTARG\\b", QRegularExpression::CaseInsensitiveOption))) {
        if (warnings) {
            warnings->append(QString(".meas TRIG/TARG interval form detected; verify LT/ngspice compatibility: %1").arg(cmd.trimmed()));
        }
    }

    if (out.contains(QRegularExpression("\\b(RISE|FALL|CROSS)\\s*=\\s*(LAST|\\d+)", QRegularExpression::CaseInsensitiveOption))) {
        if (warnings) {
            warnings->append(QString(".meas RISE/FALL/CROSS qualifier detected; verify LT/ngspice event counting compatibility: %1").arg(cmd.trimmed()));
        }
    }

    if (out.contains(QRegularExpression("\\b(AVG|MAX|MIN|PP|RMS|INTEG)\\b", QRegularExpression::CaseInsensitiveOption))) {
        if (warnings) {
            warnings->append(QString(".meas interval reduction keyword detected and passed through unchanged: %1").arg(cmd.trimmed()));
        }
    }

    return out;
}

QString normalizeMeanDirective(const QString& cmd) {
    static const QRegularExpression re(
        "^\\s*\\.mean\\s+(?:(avg|max|min|rms)\\s+)?([^\\s]+)(?:\\s+from\\s*=\\s*([^\\s]+))?(?:\\s+to\\s*=\\s*([^\\s]+))?\\s*$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(cmd.trimmed());
    if (!m.hasMatch()) return cmd;

    const QString mode = m.captured(1).isEmpty() ? QString("avg") : m.captured(1).toLower();
    const QString signal = m.captured(2).trimmed();
    const QString from = m.captured(3).trimmed();
    const QString to = m.captured(4).trimmed();
    if (signal.isEmpty()) return cmd;

    const QString name = QString("mean_%1_%2_%3")
        .arg(mode, sanitizeDirectiveName(signal))
        .arg(QString::number(qHash(cmd.toLower()), 16));

    QString out = QString(".meas tran %1 %2 %3").arg(name, mode, signal);
    if (!from.isEmpty()) out += QString(" from=%1").arg(from);
    if (!to.isEmpty()) out += QString(" to=%1").arg(to);
    return out;
}

UserSpiceContentSummary summarizeUserSpiceText(const QString& text, const QString& projectDir) {
    UserSpiceContentSummary summary;

    static const QRegularExpression includeDirectiveRe(
        "^\\s*\\.(lib|inc|include)\\s+(?:\"([^\"]+)\"|(\\S+))",
        QRegularExpression::CaseInsensitiveOption);
    static const QSet<QString> analysisCards = {
        ".tran", ".ac", ".op", ".dc", ".noise", ".four", ".tf",
        ".disto", ".meas", ".step", ".sens", ".sp", ".net"
    };

    const QStringList lines = SpiceCompatRewriter::collapseSpiceContinuationLines(text);
    QSet<QString> analysisSeen;
    QMap<QString, int> modelSeen;
    QMap<QString, int> refSeen;
    QStringList subcktStack;
    int lineNo = 0;
    for (const QString& rawLine : lines) {
        ++lineNo;
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('*') || line.startsWith(';') || line.startsWith('#')) continue;

        if (line.startsWith('.')) {
            QString card;
            int firstSpace = line.indexOf(' ');
            if (firstSpace > 0) card = line.left(firstSpace).trimmed().toLower();
            else card = line.trimmed().toLower();

            if (analysisCards.contains(card)) {
                summary.hasExplicitAnalysisCard = true;
                if (analysisSeen.contains(card)) {
                    summary.warnings.append(QString("Duplicate analysis card %1 in directive block (line %2).")
                        .arg(card, QString::number(lineNo)));
                } else {
                    analysisSeen.insert(card);
                }
            }

            if (card == ".save") {
                summary.hasExplicitSaveDirective = true;
            }
            if (card == ".sp" || card == ".net") {
                summary.isSParameter = true;
            }

            if (card == ".net") {
                summary.hasNetDirective = true;
            }

            if (card == ".tran" && line.contains("startup", Qt::CaseInsensitive)) {
                summary.hasLtStartup = true;
            }

            if (card == ".subckt") {
                const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    subcktStack.append(parts.at(1));
                }
            } else if (card == ".ends") {
                const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (subcktStack.isEmpty()) {
                    summary.warnings.append(QString(".ends has no matching .subckt (line %1).").arg(lineNo));
                } else {
                    const QString openName = subcktStack.takeLast();
                    if (parts.size() >= 2 && parts.at(1).compare(openName, Qt::CaseInsensitive) != 0) {
                        summary.warnings.append(QString(".ends %1 does not match open .subckt %2 (line %3).").arg(parts.at(1), openName, QString::number(lineNo)));
                    }
                }
            }

            const QRegularExpressionMatch includeMatch = includeDirectiveRe.match(line);
            if (includeMatch.hasMatch()) {
                const QString rawPath = includeMatch.captured(2).isEmpty()
                    ? includeMatch.captured(3)
                    : includeMatch.captured(2);
                const QString normalized = ModelInjector::normalizeIncludePathForNetlist(rawPath, projectDir);
                if (!normalized.isEmpty()) {
                    summary.declaredModelFiles.insert(normalized);
                }
            }

            if (card == ".model") {
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    const QString modelName = parts[1].toLower();
                    if (modelSeen.contains(modelName)) {
                        summary.warnings.append(QString("Duplicate .model %1 in directive block (lines %2 and %3).").arg(parts[1]).arg(modelSeen.value(modelName)).arg(lineNo));
                    } else {
                        modelSeen.insert(modelName, lineNo);
                    }
                    summary.declaredModelNames.insert(modelName);
                }

                if (line.contains(" D(", Qt::CaseInsensitive) &&
                    (line.contains("Ron=", Qt::CaseInsensitive) || line.contains("Roff=", Qt::CaseInsensitive) || line.contains("Vfwd=", Qt::CaseInsensitive))) {
                    summary.warnings.append(QString("LT-style diode model parameters detected in line %1; ngspice may reject Ron/Roff/Vfwd on .model D.").arg(lineNo));
                }
            }

            if (card == ".meas" && line.contains("I(", Qt::CaseInsensitive)) {
                summary.warnings.append(QString("Measurement current expression in line %1 may be LT-specific; ngspice is less reliable with I(R...) style expressions.").arg(lineNo));
            }

            if ((card == ".meas" || card == ".func" || card == ".param") && line.contains("table(", Qt::CaseInsensitive)) {
                summary.warnings.append(QString("table(...) detected in line %1; VioSpice will approximate inline point-pair forms for ngspice, but file/include-style forms may still differ.").arg(lineNo));
            }

            if (card == ".func") {
                summary.warnings.append(QString("LT .func detected in line %1; user-defined functions may rely on LT dynamic scoping, so verify ngspice compatibility when referenced inside subcircuits or with local .param overrides.").arg(lineNo));
            }

            if (card == ".step") {
                summary.warnings.append(QString("LT .step detected in line %1; this ngspice configuration reports .step as unimplemented, so VioSpice will omit it from the active netlist.").arg(lineNo));
            }

            if (card == ".four") {
                summary.warnings.append(QString("LT .four detected in line %1; verify Fourier-analysis compatibility and output behavior in ngspice.").arg(lineNo));
            }

            if (card == ".wave") {
                summary.warnings.append(QString("LT .wave detected in line %1; ngspice does not support LT WAV export directives.").arg(lineNo));
            }

            if ((card == ".param" || card == ".func") && line.contains("file=", Qt::CaseInsensitive)) {
                summary.warnings.append(QString("LT file= syntax detected in line %1; verify ngspice compatibility for file-driven expressions or sweeps.").arg(lineNo));
            }

            continue;
        }

        summary.hasElementCards = true;
        const bool emulateStartupOnLine = summary.hasLtStartup && subcktStack.isEmpty();
        const QString rewrittenLine = SpiceCompatRewriter::rewriteLtDirectiveLine(line, &summary.warnings, emulateStartupOnLine, projectDir);
        if (rewrittenLine.contains("if(", Qt::CaseInsensitive)) {
            summary.warnings.append(QString("LT-style if(...) expression remains in line %1 and may fail in ngspice.").arg(lineNo));
        }
        if (line.contains("table(", Qt::CaseInsensitive)) {
            summary.warnings.append(QString("table(...) detected in line %1; VioSpice will approximate inline point-pair forms for ngspice, but file/include-style forms may still differ.").arg(lineNo));
        }
        if (line.contains("wavefile=", Qt::CaseInsensitive)) {
            summary.warnings.append(QString("LT wavefile= source detected in line %1; ngspice compatibility for WAV-backed sources is not implemented in VioSpice.").arg(lineNo));
        }
        if (line.contains("chan=", Qt::CaseInsensitive) && line.contains("wavefile=", Qt::CaseInsensitive)) {
            summary.warnings.append(QString("LT chan= option for wavefile-backed sources detected in line %1; verify channel-selection compatibility manually.").arg(lineNo));
        }
        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        const QString ref = parts.first().toUpper();
        if (refSeen.contains(ref)) {
            summary.warnings.append(QString("Duplicate element reference %1 in directive block (lines %2 and %3).").arg(parts.first()).arg(refSeen.value(ref)).arg(lineNo));
        } else {
            refSeen.insert(ref, lineNo);
        }
        summary.declaredElementRefs.insert(ref);

        const QChar prefix = ref.isEmpty() ? QChar() : ref.at(0);
        if ((prefix == 'V' || prefix == 'I') && parts.size() >= 2) {
            summary.drivenRailNets.insert(parts.at(1).trimmed().toUpper());
        }
    }

    if (!subcktStack.isEmpty()) {
        for (const QString& openSubckt : subcktStack) {
            summary.warnings.append(QString("Missing .ends for subcircuit %1.").arg(openSubckt));
        }
    }

    return summary;
}
}

SpiceNetlistGenerator::GeneratedNetlist SpiceNetlistGenerator::generate(QGraphicsScene* scene, const QString& projectDir, NetManager* /*netManager*/, const SimulationParams& paramsIn) {
    if (!scene) return { "* Missing scene\n", {} };

    SimulationParams params = paramsIn;
    // Fallback: If AC/S-Param mode and no source specified, pick first V source
    if ((params.type == AC || params.type == SParameter) && params.rfPort1Source.trimmed().isEmpty()) {
        for (QGraphicsItem* item : scene->items()) {
            if (auto* si = dynamic_cast<SchematicItem*>(item)) {
                if (si->itemType() == SchematicItem::VoltageSourceType && !si->reference().isEmpty()) {
                    params.rfPort1Source = si->reference();
                    break;
                }
            }
        }
    }

    qDebug() << "[SpiceNetlistGenerator] type=" << params.type << "rfPort1Source=" << params.rfPort1Source << "rfPort2Node=" << params.rfPort2Node << "rfZ0=" << params.rfZ0;

    QString netlist;
    netlist += "* viospice Automated Hierarchical SPICE Netlist\n";
    netlist += "* Generated: " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n";
    netlist += ".options ngbehavior=ltps\n";
    // Guardrail:
    // Keep a permissive deck-level default here. The actual native-logic switch
    // happens in SimulationManager before load, where `vicompat=lt` is applied
    // only for VioMATRIXC. A deck-only `.options vicompat=lt` is too late for
    // parse-time A-device rewriting and must not be relied on.
    netlist += ".options vicompat=all\n";
    if (params.type == AC || params.type == SParameter) {
        netlist += ".options savecurrents\n";
        netlist += ".save all v(all)\n";
        
        // Use .probe for branch currents - much more robust in VioMATRIXC/Ngspice
        for (QGraphicsItem* item : scene->items()) {
            if (auto* si = dynamic_cast<SchematicItem*>(item)) {
                QString ref = si->reference().trimmed();
                if (ref.isEmpty()) continue;
                
                if (ref.startsWith("R", Qt::CaseInsensitive) || 
                    ref.startsWith("C", Qt::CaseInsensitive) ||
                    ref.startsWith("L", Qt::CaseInsensitive)) {
                    netlist += QString(".probe i(%1)\n").arg(ref);
                } else if (ref.startsWith("D", Qt::CaseInsensitive)) {
                    netlist += QString(".probe i(%1)\n").arg(ref);
                } else if (ref.startsWith("Q", Qt::CaseInsensitive)) {
                    // For transistors, probe terminal currents
                    netlist += QString(".probe ic(%1) ib(%1) ie(%1)\n").arg(ref);
                }
            }
        }
    }

    // 0. Append SPICE Directives from schematic at the TOP 
    // This ensures .params and .model are defined before use
    netlist += "* Custom SPICE Directives\n";
    
    // --- Tuning Slider Parameters ---
    for (QGraphicsItem* item : scene->items()) {
        if (auto* slider = dynamic_cast<TuningSliderSymbolItem*>(item)) {
            QString ref = slider->reference().trimmed();
            if (!ref.isEmpty()) {
                netlist += QString(".param %1 = %2\n").arg(ref).arg(slider->currentValue());
            }
        }
    }
    QSet<QString> switchModelsAdded;
    QSet<QString> userDeclaredModelFiles;
    QSet<QString> userElementRefs;
    QSet<QString> userDrivenRailNets;
    QStringList directiveWarnings;
    bool hasExplicitSaveDirective = false;
    bool hasNetDirective = false;
    bool isSParameterDirective = false;
    bool hasExplicitAnalysisCard = false;
    bool hasUserElementCards = false;

    // Helper to determine if a card string matches the current simulation type
    auto cardMatchesType = [&](const QString& card, SimulationType type) {
        if (type == Transient && card == ".tran") return true;
        if (type == AC && card == ".ac") return true;
        if (type == SParameter && (card == ".sp" || card == ".net")) return true;
        if (type == DC && card == ".dc") return true;
        if (type == OP && card == ".op") return true;
        if (type == Noise && card == ".noise") return true;
        if (type == TF && card == ".tf") return true;
        if (type == Sens && card == ".sens") return true;
        if (type == Disto && card == ".disto") return true;
        return false;
    };

    for (QGraphicsItem* item : scene->items()) {
        if (auto* si = dynamic_cast<SchematicItem*>(item)) {
            if (si->itemType() == SchematicItem::SpiceDirectiveType) {
                if (auto* dir = dynamic_cast<SchematicSpiceDirectiveItem*>(si)) {
                    QString cmd = dir->text().trimmed();
                    if (!cmd.isEmpty()) {
                        const UserSpiceContentSummary summary = summarizeUserSpiceText(cmd, projectDir);
                        userDeclaredModelFiles.unite(summary.declaredModelFiles);
                        switchModelsAdded.unite(summary.declaredModelNames);
                        userElementRefs.unite(summary.declaredElementRefs);
                        userDrivenRailNets.unite(summary.drivenRailNets);
                        directiveWarnings.append(summary.warnings);
                        hasExplicitAnalysisCard = hasExplicitAnalysisCard || summary.hasExplicitAnalysisCard;
                        hasUserElementCards = hasUserElementCards || summary.hasElementCards;
                        hasExplicitSaveDirective = hasExplicitSaveDirective || summary.hasExplicitSaveDirective;
                        hasNetDirective = hasNetDirective || summary.hasNetDirective;
                        isSParameterDirective = isSParameterDirective || summary.isSParameter;

                        const QStringList cmdLines = SpiceCompatRewriter::collapseSpiceContinuationLines(cmd);
                        int subcktDepth = 0;
                        for (const QString& rawCmdLine : cmdLines) {
                            const QString trimmedCmdLine = rawCmdLine.trimmed();
                            if (trimmedCmdLine.isEmpty()) {
                                netlist += "\n";
                                continue;
                            }

                            // Do NOT skip .net for SParameter - we need it!
                            // We only skip if we are sure we are replacing it later.


                            // Skip analysis directives that don't match the current simulation type
                            if (params.type != SParameter &&
                                (trimmedCmdLine.startsWith(".sp", Qt::CaseInsensitive) ||
                                 trimmedCmdLine.startsWith(".net", Qt::CaseInsensitive))) {
                                netlist += "* Skipped directive (not applicable to current analysis): " + trimmedCmdLine + "\n";
                                continue;
                            }
                            if (params.type == SParameter && trimmedCmdLine.startsWith(".ac", Qt::CaseInsensitive)) {
                                netlist += "* Skipped user .ac directive (auto-generated for SParameter analysis): " + trimmedCmdLine + "\n";
                                continue;
                            }

                            const bool emulateStartupOnLine = summary.hasLtStartup && subcktDepth == 0;
                            QString lineToWrite = SpiceCompatRewriter::rewriteLtDirectiveLine(trimmedCmdLine, &directiveWarnings, emulateStartupOnLine, projectDir);

                            // Resolve relative .include/.lib/.inc paths to absolute so ngspice can always
                            // find them regardless of its CWD.
                            {
                                static const QRegularExpression incLineRe(
                                    "^(\\s*\\.(?:include|lib|inc)\\s+)(?:\"([^\"]+)\"|(\\S+))\\s*$",
                                    QRegularExpression::CaseInsensitiveOption);
                                const QRegularExpressionMatch m = incLineRe.match(lineToWrite);
                                if (m.hasMatch()) {
                                    const QString keyword = m.captured(1);
                                    const QString rawPath = m.captured(2).isEmpty() ? m.captured(3) : m.captured(2);
                                    QFileInfo rfInfo(rawPath);
                                    if (!rfInfo.isAbsolute()) {
                                        const QString absPath = ModelInjector::normalizeIncludePathForNetlist(rawPath, projectDir);
                                        if (!absPath.isEmpty() && absPath != rawPath && QFileInfo::exists(absPath)) {
                                            lineToWrite = QString("%1\"%2\"").arg(keyword.trimmed() + " ", absPath);
                                            directiveWarnings.append(QString("Resolved relative include '%1' to '%2'.").arg(rawPath, absPath));
                                        }
                                    }
                                }
                            }

        if (trimmedCmdLine.startsWith(".mean", Qt::CaseInsensitive)) {
            const QString converted = normalizeMeanDirective(trimmedCmdLine);
            if (converted != trimmedCmdLine) {
                netlist += "* " + trimmedCmdLine + "\n";
                netlist += converted + "\n";
                SpiceCompatRewriter::updateSubcktDepthForLine(trimmedCmdLine, subcktDepth);
                continue;
            }
        }

        if (trimmedCmdLine.startsWith(".step", Qt::CaseInsensitive)) {
            netlist += "* " + trimmedCmdLine + "\n";
            netlist += "* LT .step omitted: this ngspice configuration reports .step as unimplemented\n";
            SpiceCompatRewriter::updateSubcktDepthForLine(trimmedCmdLine, subcktDepth);
            continue;
        }

                            if (trimmedCmdLine.startsWith(".meas", Qt::CaseInsensitive)) {
                                lineToWrite = normalizeLtMeasDirective(lineToWrite, &directiveWarnings);
                            }

                            if (lineToWrite != trimmedCmdLine) {
                                netlist += "* LT rewrite: " + trimmedCmdLine + "\n";
                            }
                            netlist += lineToWrite + "\n";
                            SpiceCompatRewriter::updateSubcktDepthForLine(trimmedCmdLine, subcktDepth);
                        }
                    }
                }
            }
        }
    }

    if (params.type == OP && isSParameterDirective) {
        // Upgrade to SParameter mode if .sp/.net cards are present in Auto-Detect
        const_cast<SimulationParams&>(params).type = SParameter;
    }

    netlist += "\n";

    // 0.5 Collect model includes from symbols
    QSet<QString> includePaths;
    QSet<QString> libPaths;

    // 1. Get Flattened ECO Package (Components and Nets)
    qDebug() << "[SpiceNetlistGenerator] Generating ECO package...";
    ECOPackage pkg = NetlistGenerator::generateECOPackage(scene, projectDir, nullptr);
    qDebug() << "[SpiceNetlistGenerator] ECO package generated. Components:" << pkg.components.size() << "Nets:" << pkg.nets.size();
    
    // Evaluate connectivity
    ConnectivityResult connResult = ConnectivityEvaluator::evaluate(pkg, userDrivenRailNets);
    QMap<QString, QMap<QString, QString>>& componentPins = connResult.componentPins;
    qDebug() << "[SpiceNetlistGenerator] Pin mapping built.";

        // Use ComponentExtractor to get include/lib paths, embedded subcircuits, and model lines
    ComponentExtractor::ExtractionResult extraction = ComponentExtractor::extract(pkg, projectDir, switchModelsAdded);
    includePaths.unite(extraction.includePaths);
    libPaths.unite(extraction.libPaths);
    QMap<QString, QString> embeddedSubcircuits = extraction.embeddedSubcircuits;
    QStringList embeddedModelLines = extraction.embeddedModelLines;
    QStringList runtimeWarnings = extraction.runtimeWarnings;
    runtimeWarnings.append(connResult.runtimeWarnings);

    // Standalone-sheet guard (mirrors SimSchematicBridge preflight [fatal]):
    // top-level hierarchical ports dangle without a parent sheet, so mark
    // the deck loudly instead of emitting a fragment that dies in ngspice
    // with a cryptic singular-matrix error.
    for (QGraphicsItem* gi : scene->items()) {
        auto* si = dynamic_cast<SchematicItem*>(gi);
        if (!si || si->itemType() != SchematicItem::HierarchicalPortType) continue;
        QString portName = si->value().trimmed();
        if (portName.isEmpty()) portName = si->reference().trimmed();
        if (portName.isEmpty()) portName = "?";
        runtimeWarnings.append(QString("Hierarchical port '%1' cannot be simulated standalone: run through the parent schematic that instantiates this sheet.").arg(portName));
        netlist += QString("* ERROR: Hierarchical port '%1' dangles without a parent sheet; this deck cannot simulate standalone\n").arg(portName);
    }

    ModelInjector::inject(includePaths,
                          libPaths,
                          embeddedModelLines,
                          embeddedSubcircuits,
                          projectDir,
                          userDeclaredModelFiles,
                          netlist);

    // 3. Global Power Net Mapping for hidden pin auto-connection
    QMap<QString, QString>& powerNetMapping = connResult.powerNetMapping;

    // 4. Export components
    QMap<QString, QString>& powerNetVoltages = connResult.powerNetVoltages;
    QStringList savedCurrentVectors;
    QSet<QString> emittedRefs;
    QSet<QString>& digitalDrivenNets = connResult.digitalDrivenNets;
    NetlistManager::BridgeModels mixedModeBridgeModels;

    if (!digitalDrivenNets.isEmpty()) {
        netlist += "* Mixed-mode XSPICE bridges\n";
        netlist += QString(".model __viospice_adc_bridge adc_bridge(in_low=%1 in_high=%2 rise_delay=%3 fall_delay=%4)\n")
                       .arg(QString::number(mixedModeBridgeModels.adcLow, 'g', 12),
                            QString::number(mixedModeBridgeModels.adcHigh, 'g', 12),
                            QString::number(mixedModeBridgeModels.adcRiseDelay, 'g', 12),
                            QString::number(mixedModeBridgeModels.adcFallDelay, 'g', 12));
        netlist += QString(".model __viospice_dac_bridge dac_bridge(out_low=%1 out_high=%2 out_undef=%3 input_load=%4 t_rise=%5 t_fall=%6)\n")
                       .arg(QString::number(mixedModeBridgeModels.dacLow, 'g', 12),
                            QString::number(mixedModeBridgeModels.dacHigh, 'g', 12),
                            QString::number(mixedModeBridgeModels.dacUndef, 'g', 12),
                            QString::number(mixedModeBridgeModels.dacInputLoad, 'g', 12),
                            QString::number(mixedModeBridgeModels.dacRiseTime, 'g', 12),
                            QString::number(mixedModeBridgeModels.dacFallTime, 'g', 12));
        netlist += ".subckt __viospice_adc_wrap ANA DIG\n";
        netlist += "* XSPICE adc_bridge: analog wire into event-driven digital logic.\n";
        netlist += "A_ADC [ANA] [DIG] __viospice_adc_bridge\n";
        netlist += ".ends __viospice_adc_wrap\n";
        netlist += ".subckt __viospice_dac_wrap DIG ANA\n";
        netlist += "* XSPICE dac_bridge: event-driven digital logic into analog wire.\n";
        netlist += "A_DAC [DIG] [ANA] __viospice_dac_bridge\n";
        netlist += ".ends __viospice_dac_wrap\n\n";
    }

    for (const auto& comp : pkg.components) {
        ComponentFormatter::format(comp,
                                   scene,
                                   projectDir,
                                   params,
                                   componentPins,
                                   powerNetMapping,
                                   powerNetVoltages,
                                   userElementRefs,
                                   digitalDrivenNets,
                                   pkg.nets,
                                   emittedRefs,
                                   switchModelsAdded,
                                   runtimeWarnings,
                                   directiveWarnings,
                                   savedCurrentVectors,
                                   netlist);
    }

    NetlistFormatter::format(params,
                             powerNetVoltages,
                             userDrivenRailNets,
                             savedCurrentVectors,
                             directiveWarnings,
                             runtimeWarnings,
                             hasUserElementCards,
                             hasNetDirective,
                             hasExplicitAnalysisCard,
                             hasExplicitSaveDirective,
                             netlist);
    return { netlist, componentPins };
}

QString SpiceNetlistGenerator::buildCommand(const SimulationParams& params) {
    switch (params.type) {
        case Transient:
        {
            if (!params.transientMaxStep.trimmed().isEmpty()) {
                const QString tstart = (params.start.trimmed().isEmpty() || params.start.trimmed() == "0")
                    ? QString("0") : params.start.trimmed();
                QString command = QString(".tran %1 %2 %3 %4").arg(params.step, params.stop, tstart, params.transientMaxStep);
                if (params.transientSteady) {
                    command += " steady";
                }
                return command;
            }
            QString command = QString(".tran %1 %2").arg(params.step, params.stop);
            if (!params.start.trimmed().isEmpty() && params.start.trimmed() != "0") {
                command += QString(" %1").arg(params.start.trimmed());
            }
            if (params.transientSteady) {
                command += " steady";
            }
            return command;
        }
        case DC:
            return QString(".dc %1 %2 %3 %4").arg(params.dcSource, params.dcStart, params.dcStop, params.dcStep);
        case AC: {
            auto safeNumber = [](const QString& text, double fallback) {
                double parsed = 0.0;
                if (SimValueParser::parseSpiceNumber(text, parsed) && parsed > 0.0) {
                    return text.trimmed();
                }
                return QString::number(fallback, 'g', 12);
            };
            const QString pts = safeNumber(params.step, 10.0);
            const QString start = safeNumber(params.start, 10.0);
            const QString stop = safeNumber(params.stop, 1e6);
            QString type = "dec";
            if (params.acSweepType == SimAcSweepType::Octave) type = "oct";
            else if (params.acSweepType == SimAcSweepType::Linear) type = "lin";
            return QString(".ac %1 %2 %3 %4").arg(type, pts, start, stop);
        }
        case OP:
            return ".op";
        case Noise: {
            const QString output = params.noiseOutput.isEmpty() ? "V(out)" : params.noiseOutput;
            const QString source = params.noiseSource.isEmpty() ? "V1" : params.noiseSource;
            const QString pts = params.step.isEmpty() ? "10" : params.step;
            const QString fstart = params.start.isEmpty() ? "1" : params.start;
            const QString fstop = params.stop.isEmpty() ? "1Meg" : params.stop;
            return QString(".noise %1 %2 %3 %4 %5").arg(output, source, pts, fstart, fstop);
        }
        case Fourier: {
            const QString freq = params.fourFreq.isEmpty() ? "1k" : params.fourFreq;
            QStringList outputs = params.fourOutputs;
            if (outputs.isEmpty()) outputs << "V(out)";
            return QString(".four %1 %2").arg(freq, outputs.join(" "));
        }
        case TF: {
            const QString output = params.tfOutput.isEmpty() ? "V(out)" : params.tfOutput;
            const QString source = params.tfSource.isEmpty() ? "V1" : params.tfSource;
            return QString(".tf %1 %2").arg(output, source);
        }
        case Disto: {
            const QString pts = params.step.isEmpty() ? "10" : params.step;
            const QString fstart = params.start.isEmpty() ? "1" : params.start;
            const QString fstop = params.stop.isEmpty() ? "1Meg" : params.stop;
            if (!params.distoF2OverF1.isEmpty()) {
                return QString(".disto %1 %2 %3 %4").arg(pts, fstart, fstop, params.distoF2OverF1);
            }
            return QString(".disto %1 %2 %3").arg(pts, fstart, fstop);
        }
        case Meas:
            return params.measRaw.isEmpty() ? ".meas" : params.measRaw;
        case Step:
            return params.stepRaw.isEmpty() ? ".step" : params.stepRaw;
        case Sens: {
            const QString output = params.sensOutput.isEmpty() ? "V(out)" : params.sensOutput;
            return QString(".sens %1").arg(output);
        }
        case FFT:
            return ".fft";
        case SParameter: {
            const QString pts = params.step.isEmpty() ? "10" : params.step;
            const QString start = params.start.isEmpty() ? "1Meg" : params.start;
            const QString stop = params.stop.isEmpty() ? "1G" : params.stop;
            return QString(".sp dec %1 %2 %3").arg(pts, start, stop);
        }
    }
    return ".op";
}





QString SpiceNetlistGenerator::formatValue(double value) {
    if (value == 0) return "0";
    if (value < 1e-9) return QString::number(value * 1e12) + "p";
    if (value < 1e-6) return QString::number(value * 1e9) + "n";
    if (value < 1e-3) return QString::number(value * 1e06) + "u";
    if (value < 1) return QString::number(value * 1e3) + "m";
    return QString::number(value);
}

QString SpiceNetlistGenerator::generateCompatibilityLayer(const QString& rawNetlist) {
    if (rawNetlist.isEmpty()) return QString();

    QString processed = rawNetlist;
    processed = processed.trimmed();

    // Split by newline and handle potential different newline formats
    QStringList lines = processed.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    QStringList outLines;
    QStringList warnings;

    for (QString line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('*') || trimmed.startsWith(';') || trimmed.startsWith('#') || trimmed.startsWith('+')) {
            outLines << line; // Keep continuation lines as-is, don't try to rewrite them
            continue;
        }

        // If the line already uses native VioMATRIXC pwlfile, don't mess with it
        if (trimmed.contains("pwlfile=", Qt::CaseInsensitive)) {
            outLines << line;
            continue;
        }

        line = SpiceCompatRewriter::rewriteLtDirectiveLine(line, &warnings);

        outLines << line;
    }

    return outLines.join('\n');
}
