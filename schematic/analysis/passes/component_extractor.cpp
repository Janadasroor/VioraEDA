/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "component_extractor.h"
#include "../../../symbols/symbol_library.h"
#include "../../../symbols/models/symbol_definition.h"
#include "../../../simulator/bridge/model_library_manager.h"
#include "../../items/schematic_item.h"
#include "../../../simulator/core/sim_netlist.h"
#include "../../../core/project/config_manager.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QHash>
#include <QRegularExpression>

using Flux::Model::SymbolDefinition;

namespace {

QString spicetypeToString(SimComponentType type) {
    switch (type) {
        case SimComponentType::Diode:           return "D";
        case SimComponentType::BJT_NPN:         return "NPN";
        case SimComponentType::BJT_PNP:         return "PNP";
        case SimComponentType::MOSFET_NMOS:     return "NMOS";
        case SimComponentType::MOSFET_PMOS:     return "PMOS";
        case SimComponentType::JFET_NJF:        return "NJF";
        case SimComponentType::JFET_PJF:        return "PJF";
        case SimComponentType::Switch:          return "SW";
        case SimComponentType::CSW:             return "CSW";
        default: return "";
    }
}

QString modelLevelToSpiceType(const QString& modelLevel) {
    QString up = modelLevel.toUpper();
    // VDMOS uses its own type name, not NMOS/PMOS + LEVEL=N
    if (up == "VDMOS" || up == "VDMOSN") return "VDMOS";
    if (up == "VDMOSP") return "VDMOSP";
    if (up == "HFET") return "NHFET"; // Default to NHFET, dialog logic handles P-channel
    if (up == "MESFET") return "NMES";
    return QString();
}

QString modelLevelToLevelParam(const QString& modelLevel) {
    QString up = modelLevel.toUpper();
    if (up == "BSIM4")   return "LEVEL=14";
    if (up == "BSIM3")   return "LEVEL=8";
    if (up == "BSIMSOI") return "LEVEL=10";  // B4SOI (BSIM4-based SOI)
    if (up == "BSIM3SOI-FD") return "LEVEL=55";
    if (up == "BSIM3SOI-PD") return "LEVEL=56";
    if (up == "BSIM3SOI-DD") return "LEVEL=57";
    if (up == "HISIM2")  return "LEVEL=68";
    if (up == "HISIM_HV") return "LEVEL=73";
    if (up == "BSIM1")   return "LEVEL=4";
    if (up == "BSIM2")   return "LEVEL=5";
    if (up == "MOS1")    return "LEVEL=1";
    if (up == "MOS2")    return "LEVEL=2";
    if (up == "MOS3")    return "LEVEL=3";
    if (up == "MOS6")    return "LEVEL=6";
    if (up == "MOS9")    return "LEVEL=9";
    if (up == "SOI3")    return "LEVEL=60";
    if (up == "HFET")    return "LEVEL=1";
    if (up == "MESFET")  return "LEVEL=1";
    if (up == "VBIC")    return "LEVEL=4";
    if (up == "HICUM2")  return "LEVEL=8";
    if (up == "MEXTRAM") return "LEVEL=6";
    return QString();
}

} // namespace

ComponentExtractor::ExtractionResult ComponentExtractor::extract(
    const ECOPackage& pkg,
    const QString& projectDir,
    QSet<QString>& switchModelsAdded)
{
    ExtractionResult result;

    // Collect include paths from symbol metadata (subcircuit .inc/.lib)
    // and embedded subcircuit code
    for (const auto& comp : pkg.components) {
        SymbolDefinition* sym = SymbolLibraryManager::instance().findSymbol(comp.typeName);
        if (!sym) continue;
        if (!sym->modelPath().isEmpty()) {
            const QString resolved = resolveModelPath(sym->modelPath(), projectDir);
            if (!resolved.isEmpty()) {
                if (resolved.toLower().endsWith(".lib")) result.libPaths.insert(resolved);
                else result.includePaths.insert(resolved);
            }
        }
        // Collect unique embedded subcircuit code
        if (sym->modelSource() == "embedded" && !sym->spiceSubcircuitCode().isEmpty()) {
            const QString name = sym->modelName().trimmed();
            if (!name.isEmpty()) {
                result.embeddedSubcircuits.insert(name, sym->spiceSubcircuitCode());
            }
        }
    }

    // Auto-embed .model lines for referenced component models
    for (const auto& comp : pkg.components) {
        if (comp.excludeFromSim) continue;
        // Interactive switches are emitted as R/S/W devices with embedded
        // .model lines by the formatter; their display value ("Switch") is
        // not a model name and must never trigger library/subcircuit
        // resolution (it matches e.g. a thyristor .MODEL SWITCH in SCR.LIB,
        // dragging a whole foreign library into the deck).
        const QString swTypeLower = comp.typeName.trimmed().toLower();
        const bool isInteractiveSwitch =
            swTypeLower == "switch" || swTypeLower == "sw" ||
            swTypeLower == "voltage controlled switch" || swTypeLower == "csw" ||
            comp.reference.startsWith("SW", Qt::CaseInsensitive);
        if (isInteractiveSwitch) continue;
        QString modelName = comp.value.trimmed();
        const QString typeLower = comp.typeName.trimmed().toLower();
        const bool isJfet = (typeLower == "njf" || typeLower == "pjf") ||
                            comp.reference.startsWith("J", Qt::CaseInsensitive);
        const bool isMos = (typeLower == "transistor_nmos" || typeLower == "transistor_pmos" ||
                            typeLower == "nmos" || typeLower == "nmos4" ||
                            typeLower == "pmos" || typeLower == "pmos4") ||
                           comp.reference.startsWith("M", Qt::CaseInsensitive);
        const bool isBjt = (typeLower == "transistor" || typeLower == "transistor_pnp" ||
                            typeLower == "npn" || typeLower == "npn2" || typeLower == "npn3" || typeLower == "npn4" ||
                            typeLower == "pnp" || typeLower == "pnp2" || typeLower == "pnp4" || typeLower == "lpnp") ||
                           comp.reference.startsWith("Q", Qt::CaseInsensitive);
        if (isMos || isJfet || isBjt) {
            modelName = splitLeadingSpiceToken(modelName).head;
        }
        if (modelName.isEmpty() && isJfet) {
            modelName = (typeLower == "pjf" || comp.reference.startsWith("JP", Qt::CaseInsensitive)) ? "PJF" : "NJF";
        }
        if (modelName.isEmpty() && isBjt) {
            modelName = (typeLower == "transistor_pnp" || typeLower == "pnp" || typeLower == "pnp2" ||
                         typeLower == "pnp4" || typeLower == "lpnp" ||
                         comp.reference.startsWith("QP", Qt::CaseInsensitive)) ? "2N3906" : "2N2222";
        }
        if (modelName.isEmpty() && isMos) {
            modelName = (typeLower == "transistor_pmos" || typeLower == "pmos" || typeLower == "pmos4" ||
                         comp.reference.startsWith("MP", Qt::CaseInsensitive)) ? "BS250" : "2N7000";
        }
        if (modelName.isEmpty()) continue;
        if (switchModelsAdded.contains(modelName.toLower())) {
            result.runtimeWarnings.append(QString("Skipped auto-generated model %1 because it is already declared manually.").arg(modelName));
            continue;
        }

        const SimModel* mdl = ModelLibraryManager::instance().findModel(modelName);
        if (mdl) {
            const QString line = modelToSpiceLine(*mdl);
            if (!line.isEmpty()) {
                result.embeddedModelLines.append(line);
                switchModelsAdded.insert(modelName.toLower());
            }
        } else if (ModelLibraryManager::instance().findSubcircuit(modelName) || 
                   comp.reference.startsWith("X", Qt::CaseInsensitive) || 
                   typeLower.contains("amplifier") || typeLower.contains("opamp") || typeLower.contains("ic") ||
                   ([&]() {
                       const SymbolDefinition* sym = SymbolLibraryManager::instance().findSymbol(comp.typeName);
                       return sym && !sym->spiceNodeMapping().isEmpty();
                   }()) ||
                   // Standalone .viosym symbols with spiceNodeMapping (e.g. LT1221 from project)
                   (!modelName.isEmpty() && !isJfet && !isMos && !isBjt)) {
            // If it's a subcircuit, we MUST ensure we have its pin names/order from the model library
            const SimSubcircuit* sub = ModelLibraryManager::instance().findSubcircuit(modelName);
            QString subLib = ModelLibraryManager::instance().findLibraryPath(modelName);
            const SymbolDefinition* sym2 = SymbolLibraryManager::instance().findSymbol(comp.typeName);
            if (sym2 && !sym2->modelPath().isEmpty()) {
                const QString symModelFile = resolveModelPath(sym2->modelPath(), projectDir);
                if (!symModelFile.isEmpty() && QFileInfo::exists(symModelFile)) {
                    subLib.clear();
                }
            }
            if (!subLib.isEmpty()) {
                if (subLib.endsWith(".lib", Qt::CaseInsensitive)) {
                    result.libPaths.insert(subLib);
                } else {
                    result.includePaths.insert(subLib);
                }
                switchModelsAdded.insert(modelName.toLower());
            }
        } else if (!switchModelsAdded.contains(modelName.toLower()) &&
                   (comp.type == SchematicItem::DiodeType || typeLower.contains("diode"))) {
            // Generate .model from component paramExpressions for user-customized diodes
            const auto& pe = comp.paramExpressions;
            if (!pe.isEmpty()) {
                QString line = QString(".model %1 D(").arg(modelName);
                QStringList params;
                auto addParam = [&](const QString& key) {
                    QString val = pe.value(key).trimmed();
                    if (!val.isEmpty()) {
                        params.append(QString("%1=%2").arg(key, val));
                    }
                };
                addParam("diode.Is");
                addParam("diode.N");
                addParam("diode.Rs");
                addParam("diode.Vj");
                addParam("diode.Cjo");
                addParam("diode.M");
                addParam("diode.tt");
                addParam("diode.BV");
                addParam("diode.IBV");

                // Strip "diode." prefix for SPICE format
                for (int i = 0; i < params.size(); ++i) {
                    params[i].replace("diode.", "");
                }

                line += params.join(" ") + ")";
                result.embeddedModelLines.append(line);
                switchModelsAdded.insert(modelName.toLower());
            }
        } else if (isJfet) {
            // Generate .model from component paramExpressions for user-customized JFETs
            const auto& pe = comp.paramExpressions;
            if (!pe.isEmpty()) {
                const QString modelType = (typeLower == "pjf" || comp.reference.startsWith("JP", Qt::CaseInsensitive)) ? "PJF" : "NJF";
                QString line = QString(".model %1 %2(").arg(modelName, modelType);
                QStringList params;
                auto addParam = [&](const QString& key) {
                    QString val = pe.value(key).trimmed();
                    if (!val.isEmpty()) {
                        params.append(QString("%1=%2").arg(key, val));
                    }
                };
                addParam("jfet.Beta");
                addParam("jfet.Vto");
                addParam("jfet.Lambda");
                addParam("jfet.Rd");
                addParam("jfet.Rs");
                addParam("jfet.Cgs");
                addParam("jfet.Cgd");
                addParam("jfet.Is");

                for (int i = 0; i < params.size(); ++i) {
                    params[i].replace("jfet.", "");
                }

                line += params.join(" ") + ")";
                result.embeddedModelLines.append(line);
                switchModelsAdded.insert(modelName.toLower());
            }
        } else if (isBjt) {
            // Generate .model from component paramExpressions for user-customized BJTs
            const auto& pe = comp.paramExpressions;
            if (!pe.isEmpty()) {
                const QString bjtTypeExpr = pe.value("bjt.type").trimmed();
                const bool pnpFromExpr = bjtTypeExpr.compare("PNP", Qt::CaseInsensitive) == 0;
                const QString modelType = (pnpFromExpr ||
                                           typeLower == "transistor_pnp" || typeLower == "pnp" || typeLower == "pnp2" ||
                                           typeLower == "pnp4" || typeLower == "lpnp" ||
                                           comp.reference.startsWith("QP", Qt::CaseInsensitive)) ? "PNP" : "NPN";
                QString line = QString(".model %1 %2(").arg(modelName, modelType);
                QStringList params;
                auto addParam = [&](const QString& key) {
                    const QString val = pe.value(key).trimmed();
                    if (!val.isEmpty()) {
                        params.append(QString("%1=%2").arg(key, val));
                    }
                };
                addParam("bjt.Is");
                addParam("bjt.Bf");
                addParam("bjt.Vaf");
                addParam("bjt.Cje");
                addParam("bjt.Cjc");
                addParam("bjt.Tf");
                addParam("bjt.Tr");

                for (int i = 0; i < params.size(); ++i) {
                    params[i].replace("bjt.", "");
                }

                line += params.join(" ") + ")";
                result.embeddedModelLines.append(line);
                switchModelsAdded.insert(modelName.toLower());
            }
        } else if (isMos) {
            // Generate .model from component paramExpressions for user-customized MOSFETs
            const auto& pe = comp.paramExpressions;
            if (!pe.isEmpty()) {
                const QString mosTypeExpr = pe.value("mos.type").trimmed();
                const bool pmosFromExpr = mosTypeExpr.compare("PMOS", Qt::CaseInsensitive) == 0;
                const QString nmosPmos = (pmosFromExpr ||
                                          typeLower == "transistor_pmos" || typeLower == "pmos" || typeLower == "pmos4" ||
                                          comp.reference.startsWith("MP", Qt::CaseInsensitive)) ? "PMOS" : "NMOS";

                const bool isPickedLevel = pe.contains("mos.pickedLevel");
                const QString mosLevel = isPickedLevel
                    ? pe.value("mos.pickedLevel").trimmed().toUpper()
                    : pe.value("mos.level").trimmed().toUpper();
                QString spiceType = nmosPmos;
                QString levelInsert;

                if (mosLevel == "VDMOS" || mosLevel == "VDMOSN") {
                    spiceType = "VDMOS";
                } else if (mosLevel == "VDMOSP") {
                    spiceType = "VDMOSP";
                } else if (mosLevel == "BSIM4") {
                    levelInsert = "LEVEL=14";
                } else if (mosLevel == "BSIMSOI") {
                    levelInsert = "LEVEL=10";
                } else if (mosLevel == "BSIM3") {
                    levelInsert = "LEVEL=8";
                } else if (mosLevel == "BSIM3SOI") {
                    levelInsert = "LEVEL=55";
                } else if (mosLevel == "HISIM2") {
                    levelInsert = "LEVEL=68";
                } else if (mosLevel == "HISIM_HV") {
                    levelInsert = "LEVEL=73";
                } else if (mosLevel == "MOS2") {
                    levelInsert = "LEVEL=2";
                } else if (mosLevel == "MOS3") {
                    levelInsert = "LEVEL=3";
                } else if (mosLevel == "BSIM1") {
                    levelInsert = "LEVEL=4";
                } else if (mosLevel == "BSIM2") {
                    levelInsert = "LEVEL=5";
                } else if (mosLevel == "MOS6") {
                    levelInsert = "LEVEL=6";
                } else if (mosLevel == "MOS9") {
                    levelInsert = "LEVEL=9";
                }

                QString line = QString(".model %1 %2(").arg(modelName, spiceType);
                QStringList params;
                if (!levelInsert.isEmpty()) {
                    params.append(levelInsert);
                }

                // Add all mos.* params (strip prefix), skip type/level
                for (auto it = pe.begin(); it != pe.end(); ++it) {
                    const QString& key = it.key().toLower();
                    const QString& val = it.value();
                    if (key == "mos.type" || key == "mos.level" || key == "mos.raw") continue;
                    if (val.trimmed().isEmpty()) continue;

                    if (key.startsWith("mos.", Qt::CaseInsensitive)) {
                        // BSIM4 requires Toxp > 0; sanitize stored 0 to a reasonable default
                        QString cleanKey = key.mid(4);
                        QString cleanVal = val;
                        if ((mosLevel == "BSIM4" || mosLevel == "BSIMSOI") &&
                            (cleanKey.toUpper() == "TOXP" || cleanKey.toUpper() == "TOXE") && cleanVal == "0") {
                            cleanVal = "1e-10";
                        }
                        // HiSIM_HV safety: VMAX must be > 1e6, and XLD must be 0 for small L
                        if (mosLevel.startsWith("HISIM", Qt::CaseInsensitive)) {
                            if (cleanKey.toUpper() == "VMAX" && cleanVal.toDouble() < 1e6) {
                                cleanVal = "2e6";
                            }
                            if (cleanKey.toUpper() == "XLD" && cleanVal.toDouble() < 0) {
                                cleanVal = "0";
                            }
                        }
                        params.append(QString("%1=%2").arg(cleanKey, cleanVal));
                    }
                }

                // HiSIM safety: Ensure XLD exists if not provided (default can be dangerous)
                if (mosLevel.startsWith("HISIM", Qt::CaseInsensitive)) {
                    bool hasXld = false;
                    for (const auto& p : params) {
                        if (p.startsWith("XLD=", Qt::CaseInsensitive)) { hasXld = true; break; }
                    }
                    if (!hasXld) params.append("XLD=0");
                }

                line += params.join(" ") + ")";
                result.embeddedModelLines.append(line);
                switchModelsAdded.insert(modelName.toLower());
            }
        }
    }

    return result;
}

QString ComponentExtractor::resolveModelPath(const QString& modelPath, const QString& projectDir) {
    if (modelPath.trimmed().isEmpty()) return QString();
    QFileInfo fi(modelPath);
    if (fi.isAbsolute()) return fi.absoluteFilePath();

    const QString source = modelPath;
    if (!projectDir.isEmpty()) {
        const QString candidate = QDir(projectDir).filePath(source);
        if (QFileInfo::exists(candidate)) return candidate;
    }

    const QStringList roots = ConfigManager::instance().libraryRoots();
    for (const QString& root : roots) {
        if (root.trimmed().isEmpty()) continue;
        const QString candidate = QDir(root).filePath(source);
        if (QFileInfo::exists(candidate)) return candidate;
    }

    // Fallback: default Viospice subcircuit library
    {
        const QString libRoot = ConfigManager::defaultLibraryPath();
        
        // If path starts with "sub/" try it relative to ViospiceLib root (correct path)
        if (source.startsWith("sub/", Qt::CaseInsensitive)) {
            const QString candidate = QDir(libRoot).filePath(source);
            if (QFileInfo::exists(candidate)) return candidate;
            // Extension fallback: .lib <-> .sub
            QFileInfo fic(candidate);
            const QString altExt = fic.suffix().toLower() == "lib" ? ".sub" : ".lib";
            const QString altCandidate = fic.dir().filePath(fic.completeBaseName() + altExt);
            if (QFileInfo::exists(altCandidate)) return altCandidate;
        } else {
            // Try inside sub/ subdirectory
            const QString candidate = QDir(libRoot + "/sub").filePath(source);
            if (QFileInfo::exists(candidate)) return candidate;
            // Extension fallback inside sub/
            QFileInfo fic(candidate);
            const QString altExt = fic.suffix().toLower() == "lib" ? ".sub" : ".lib";
            const QString altCandidate = fic.dir().filePath(fic.completeBaseName() + altExt);
            if (QFileInfo::exists(altCandidate)) return altCandidate;
        }
        
        // Backwards compatibility for paths saved as spice/filename
        if (source.startsWith("spice/")) {
            const QString compatSource = source.mid(6);
            const QString candidate = QDir(libRoot + "/sub").filePath(compatSource);
            if (QFileInfo::exists(candidate)) return candidate;
            // Extension fallback
            QFileInfo fic(candidate);
            const QString altExt = fic.suffix().toLower() == "lib" ? ".sub" : ".lib";
            const QString altCandidate = fic.dir().filePath(fic.completeBaseName() + altExt);
            if (QFileInfo::exists(altCandidate)) return altCandidate;
        }
    }

    // Fallback: resolve bare model filenames (e.g. "REG.LIB") against the
    // models/ tree of every known library root. Files like REG.LIB live at
    // <root>/models/<vendor>/<family>/REG.LIB, which none of the sub/ or
    // spice/ heuristics above can reach.
    {
        static QHash<QString, QString> modelFileNameIndex;
        static bool modelFileNameIndexBuilt = false;
        if (!modelFileNameIndexBuilt) {
            QStringList searchRoots = ConfigManager::instance().libraryRoots();
            searchRoots.append(QDir::homePath() + "/ViospiceLib");
            searchRoots.append(ConfigManager::defaultLibraryPath());
            for (const QString& root : searchRoots) {
                if (root.trimmed().isEmpty()) continue;
                const QString modelsDir = QDir(root).filePath("models");
                if (!QDir(modelsDir).exists()) continue;
                QDirIterator itr(modelsDir, QDir::Files, QDirIterator::Subdirectories);
                while (itr.hasNext()) {
                    itr.next();
                    const QString fn = itr.fileInfo().fileName().toLower();
                    if (!fn.isEmpty() && !modelFileNameIndex.contains(fn)) {
                        modelFileNameIndex.insert(fn, QDir::cleanPath(QDir::fromNativeSeparators(itr.filePath())));
                    }
                }
            }
            modelFileNameIndexBuilt = true;
        }
        const auto foundIt = modelFileNameIndex.constFind(QFileInfo(source).fileName().toLower());
        if (foundIt != modelFileNameIndex.constEnd() && !foundIt.value().isEmpty()) {
            return foundIt.value();
        }
    }

    return modelPath;
}

ComponentExtractor::SpiceTokenSplit ComponentExtractor::splitLeadingSpiceToken(const QString& raw) {
    const QString text = raw.trimmed();
    if (text.isEmpty()) return {};

    const QRegularExpression ws("\\s+");
    const QRegularExpressionMatch match = ws.match(text);
    if (!match.hasMatch()) {
        return {text, QString()};
    }

    const int start = match.capturedStart();
    const int len = match.capturedLength();
    return {text.left(start).trimmed(), text.mid(start + len).trimmed()};
}

QString ComponentExtractor::modelToSpiceLine(const SimModel& model) {
    // Check if model has a dedicated SPICE type name (BSIM4, BSIMSOI, etc.)
    QString spiceType = modelLevelToSpiceType(QString::fromStdString(model.modelLevel));
    if (spiceType.isEmpty()) {
        spiceType = spicetypeToString(model.type);
    }
    // For HFET/MESFET, adjust type based on polarity
    if (spiceType == "NHFET" && model.type == SimComponentType::MOSFET_PMOS) spiceType = "PHFET";
    if (spiceType == "NMES" && model.type == SimComponentType::MOSFET_PMOS) spiceType = "PMES";

    if (spiceType.isEmpty()) return QString();

    const QString ml = QString::fromStdString(model.modelLevel).toUpper();
    QSet<QString> allowed;
    bool isAdvanced = false;

    switch (model.type) {
        case SimComponentType::Diode:
            allowed = {"IS", "N", "RS", "VJ", "CJO", "M", "TT", "BV", "IBV"};
            break;
        case SimComponentType::BJT_NPN:
        case SimComponentType::BJT_PNP:
            isAdvanced = (ml == "VBIC" || ml == "HICUM2" || ml == "MEXTRAM" ||
                          ml == "NBJT" || ml == "NBJT2");
            if (!isAdvanced) {
                allowed = {"IS", "BF", "BR", "VAF", "VAR", "CJE", "CJC", "TF", "TR", "RB", "RE", "RC", "LEVEL"};
            }
            break;
        case SimComponentType::MOSFET_NMOS:
        case SimComponentType::MOSFET_PMOS: {
            // For advanced models (BSIM4, BSIMSOI, etc.), allow all params
            isAdvanced = (ml == "BSIM4" || ml == "BSIM3" || ml == "BSIMSOI" ||
                          ml.startsWith("BSIM3SOI") || ml == "HISIM2" || ml == "HISIM_HV" ||
                          ml == "BSIM1" || ml == "BSIM2" || ml == "MOS6" || ml == "MOS9" ||
                          ml == "VDMOS" || ml == "VDMOSN" || ml == "VDMOSP" ||
                          ml == "SOI3" || ml == "HFET" || ml == "MESFET");
            if (!isAdvanced) {
                allowed = {"VTO", "KP", "LAMBDA", "RD", "RS", "RG", "CGSO", "CGDO", "CGBO", "CBD", "CBS", "PB", "GAMMA", "PHI", "LEVEL"};
            }
            break;
        }
        case SimComponentType::JFET_NJF:
        case SimComponentType::JFET_PJF:
            allowed = {"BETA", "VTO", "LAMBDA", "RD", "RS", "CGS", "CGD", "IS", "PB", "FC"};
            break;
        default:
            break;
    }

    QString line = QString(".model %1 %2(").arg(
        QString::fromStdString(model.name), spiceType);

    // Add LEVEL= param if needed
    QString levelParam = modelLevelToLevelParam(QString::fromStdString(model.modelLevel));
    if (!levelParam.isEmpty()) {
        line += levelParam;
    }

    bool first = levelParam.isEmpty();
    for (const auto& [key, val] : model.params) {
        const QString qkey = QString::fromStdString(key).toUpper();
        if (!isAdvanced && !allowed.isEmpty() && !allowed.contains(qkey)) continue;
        if (qkey == "LEVEL") continue; // Already handled via modelLevel

        // BSIM4 requires Toxp > 0; sanitize stored 0
        QString valStr;
        if (isAdvanced && qkey == "TOXP" && val <= 0.0) {
            valStr = "1e-10";
        } else {
            valStr = QString::number(val, 'g', 12);
        }

        // HiSIM safety sanitization
        if (ml.startsWith("HISIM")) {
            if (qkey == "VMAX" && valStr.toDouble() < 1e6) {
                valStr = "2e6";
            }
            if (qkey == "XLD" && valStr.toDouble() < 0.0) {
                valStr = "0";
            }
        }

        if (!first) line += " ";
        line += QString("%1=%2").arg(qkey, valStr);
        first = false;
    }

    // HiSIM safety: Ensure XLD=0 if missing (required for stability with small L)
    if (ml.startsWith("HISIM")) {
        bool hasXld = false;
        for (const auto& [k, v] : model.params) {
            if (QString::fromStdString(k).toUpper() == "XLD") { hasXld = true; break; }
        }
        if (!hasXld) {
            if (!first) line += " ";
            line += "XLD=0";
            first = false;
        }
    }

    line += ")";
    return line;
}
