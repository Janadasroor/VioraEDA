#include "model_library_manager.h"
#include "../core/sim_model_parser.h"
#include "config_manager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include <QDebug>
#include <QVector>
#include <QRegularExpression>

namespace {
QString decodeSpiceTextInLibrary(const QByteArray& raw) {
    if (raw.isEmpty()) return QString();

    auto decodeUtf16Le = [](const QByteArray& bytes, int start) {
        QVector<char16_t> u16;
        u16.reserve((bytes.size() - start) / 2);
        for (int i = start; i + 1 < bytes.size(); i += 2) {
            const char16_t ch = static_cast<char16_t>(static_cast<unsigned char>(bytes[i])) |
                                (static_cast<char16_t>(static_cast<unsigned char>(bytes[i + 1])) << 8);
            u16.push_back(ch);
        }
        return QString::fromUtf16(u16.constData(), u16.size());
    };

    auto decodeUtf16Be = [](const QByteArray& bytes, int start) {
        QVector<char16_t> u16;
        u16.reserve((bytes.size() - start) / 2);
        for (int i = start; i + 1 < bytes.size(); i += 2) {
            const char16_t ch = (static_cast<char16_t>(static_cast<unsigned char>(bytes[i])) << 8) |
                                 static_cast<char16_t>(static_cast<unsigned char>(bytes[i + 1]));
            u16.push_back(ch);
        }
        return QString::fromUtf16(u16.constData(), u16.size());
    };

    if (raw.size() >= 2) {
        const unsigned char b0 = static_cast<unsigned char>(raw[0]);
        const unsigned char b1 = static_cast<unsigned char>(raw[1]);
        if (b0 == 0xFF && b1 == 0xFE) return decodeUtf16Le(raw, 2);
        if (b0 == 0xFE && b1 == 0xFF) return decodeUtf16Be(raw, 2);
    }

    int oddZeros = 0;
    int evenZeros = 0;
    const int n = raw.size();
    for (int i = 0; i < n; ++i) {
        if (raw[i] == '\0') {
            if (i % 2 == 0) ++evenZeros;
            else ++oddZeros;
        }
    }
    if (oddZeros > n / 8) return decodeUtf16Le(raw, 0);
    if (evenZeros > n / 8) return decodeUtf16Be(raw, 0);

    return QString::fromUtf8(raw);
}

QString typeToString(SimComponentType type) {
    switch (type) {
        case SimComponentType::Diode: return "Diode";
        case SimComponentType::BJT_NPN: return "NPN";
        case SimComponentType::BJT_PNP: return "PNP";
        case SimComponentType::MOSFET_NMOS: return "NMOS";
        case SimComponentType::MOSFET_PMOS: return "PMOS";
        case SimComponentType::JFET_NJF: return "NJF";
        case SimComponentType::JFET_PJF: return "PJF";
        case SimComponentType::SubcircuitInstance: return "Subcircuit";
        default: return "Model";
    }
}

bool isKicadModelPath(const QString& p) {
    const QString n = QDir::cleanPath(QDir::fromNativeSeparators(p)).toLower();
    return n.contains("/kicad/") || n.endsWith("/kicad");
}
}

ModelLibraryManager& ModelLibraryManager::instance() {
    static ModelLibraryManager instance;
    return instance;
}

ModelLibraryManager::ModelLibraryManager() {
    reload();
}

ModelLibraryManager::~ModelLibraryManager() {}

void ModelLibraryManager::reload() {
    QWriteLocker locker(&m_lock);
    m_modelIndex.clear();
    m_masterNetlist = SimNetlist();
    m_loadedFiles.clear();
    locker.unlock();
    
    QStringList paths = ConfigManager::instance().modelPaths();
    bool skipKicad = ConfigManager::instance().kicadDisabled();
    
    for (const QString& path : paths) {
        if (skipKicad && path.contains("kicad", Qt::CaseInsensitive)) continue;
        if (isKicadModelPath(path)) {
            qDebug() << "Skipping KiCad model path:" << path;
            continue;
        }
        
        if (QFileInfo(path).isDir()) {
            scanDirectory(path);
        } else if (QFileInfo(path).isFile()) {
            loadLibraryFile(path);
        }
    }
    
    Q_EMIT libraryReloaded();
}

QVector<SpiceModelInfo> ModelLibraryManager::allModels() const {
    QReadLocker locker(&m_lock);
    return m_modelIndex;
}

QVector<SpiceModelInfo> ModelLibraryManager::search(const QString& query) const {
    QReadLocker locker(&m_lock);
    if (query.isEmpty()) return m_modelIndex;
    
    QVector<SpiceModelInfo> results;
    for (const auto& info : m_modelIndex) {
        if (info.name.contains(query, Qt::CaseInsensitive) || 
            info.type.contains(query, Qt::CaseInsensitive) ||
            info.description.contains(query, Qt::CaseInsensitive)) {
            results.append(info);
        }
    }
    return results;
}

const SimModel* ModelLibraryManager::findModel(const QString& name) const {
    {
        QReadLocker locker(&m_lock);
        const SimModel* m = m_masterNetlist.findModel(name.toStdString());
        if (m) return m;
    }

    // Not found in memory, try to load from indexed library
    QString libPath = findLibraryPath(name);
    if (!libPath.isEmpty()) {
        const_cast<ModelLibraryManager*>(this)->loadLibraryFile(libPath);
        QReadLocker locker(&m_lock);
        return m_masterNetlist.findModel(name.toStdString());
    }
    return nullptr;
}

const SimSubcircuit* ModelLibraryManager::findSubcircuit(const QString& name) const {
    {
        QReadLocker locker(&m_lock);
        const SimSubcircuit* s = m_masterNetlist.findSubcircuit(name.toStdString());
        if (s) return s;
    }

    // Not found in memory, try to load from indexed library
    QString libPath = findLibraryPath(name);
    if (!libPath.isEmpty()) {
        const_cast<ModelLibraryManager*>(this)->loadLibraryFile(libPath);
        QReadLocker locker(&m_lock);
        return m_masterNetlist.findSubcircuit(name.toStdString());
    }
    return nullptr;
}

QString ModelLibraryManager::findLibraryPath(const QString& name) const {
    QReadLocker locker(&m_lock);
    for (const auto& info : m_modelIndex) {
        if (info.name.compare(name, Qt::CaseInsensitive) == 0) {
            return info.libraryPath;
        }
    }
    return QString();
}

void ModelLibraryManager::scanDirectory(const QString& path) {
    if (isKicadModelPath(path)) return;

    QStringList filters;
    filters << "*.lib" << "*.mod" << "*.sub" << "*.sp" << "*.inc" << "*.cmp" << "*.jft" << "*.bjt" << "*.mos";
    
    bool skipKicad = ConfigManager::instance().kicadDisabled();
    QDirIterator countIt(path, filters, QDir::Files, QDirIterator::Subdirectories);
    int total = 0;
    while (countIt.hasNext()) {
        QString f = countIt.next();
        if (skipKicad && f.contains("kicad", Qt::CaseInsensitive)) continue;
        if (isKicadModelPath(f)) continue;
        total++;
    }

    QDirIterator it(path, filters, QDir::Files, QDirIterator::Subdirectories);
    int current = 0;
    while (it.hasNext()) {
        QString f = it.next();
        if (skipKicad && f.contains("kicad", Qt::CaseInsensitive)) continue;
        if (isKicadModelPath(f)) continue;
        
        current++;
        if (current % 10 == 0 || current == total) {
            Q_EMIT progressUpdated(QString("Indexing library: %1").arg(QFileInfo(f).fileName()), current, total);
        }
        indexLibraryFile(f);
    }
}

void ModelLibraryManager::indexLibraryFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith(".model", Qt::CaseInsensitive)) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString name = parts[1];
                QString type = parts[2];
                if (type.contains('(')) type = type.section('(', 0, 0);
                
                QString modelLevel;
                QString typeUpper = type.toUpper();
                if (typeUpper == "BSIM4" || typeUpper == "BSIM3" ||
                    typeUpper == "BSIMSOI" || typeUpper == "BSIM3SOI" ||
                    typeUpper == "HISIM2" || typeUpper == "HISIM_HV" ||
                    typeUpper == "VDMOS" || typeUpper == "SOI3") {
                    modelLevel = typeUpper;
                } else {
                    // Try to extract LEVEL=N from the line
                    QRegularExpression levelRe("LEVEL\\s*=\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
                    auto levelMatch = levelRe.match(line);
                    if (levelMatch.hasMatch()) {
                        int lv = levelMatch.captured(1).toInt();
                        switch (lv) {
                            case 1: modelLevel = "MOS1"; break;
                            case 2: modelLevel = "MOS2"; break;
                            case 3: modelLevel = "MOS3"; break;
                            case 4: modelLevel = "BSIM1"; break;
                            case 5: modelLevel = "BSIM2"; break;
                            case 6: modelLevel = "MOS6"; break;
                            case 8: modelLevel = "BSIM3"; break;
                            case 9: modelLevel = "MOS9"; break;
                            case 10: modelLevel = "BSIM3SOI"; break;
                            case 14: modelLevel = "BSIM4"; break;
                            case 20: modelLevel = "BSIMSOI"; break;
                            case 53: modelLevel = "HISIM2"; break;
                            case 55: modelLevel = "HISIM_HV"; break;
                            case 56: modelLevel = "HISIM_HV"; break;
                            default: break;
                        }
                    }
                }
                
                QWriteLocker locker(&m_lock);
                SpiceModelInfo info;
                info.name = name;
                info.type = typeUpper;
                info.modelLevel = modelLevel;
                info.libraryPath = path;
                m_modelIndex.append(info);
            }
        } else if (line.startsWith(".subckt", Qt::CaseInsensitive)) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString name = parts[1];
                QWriteLocker locker(&m_lock);
                SpiceModelInfo info;
                info.name = name;
                info.type = "Subcircuit";
                info.libraryPath = path;
                m_modelIndex.append(info);
            }
        }
    }
}

void ModelLibraryManager::loadLibraryFile(const QString& path) {
    if (isKicadModelPath(path)) return;

    {
        QReadLocker locker(&m_lock);
        if (m_loadedFiles.contains(path)) return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open library file:" << path;
        return;
    }
    
    const QString content = decodeSpiceTextInLibrary(file.readAll());
    file.close();

    // Quick sniff for non-SPICE formats
    const QString trimmed = content.trimmed();
    if (trimmed.startsWith("CSI SCH LIB", Qt::CaseInsensitive) || 
        trimmed.startsWith("V2.0", Qt::CaseInsensitive) ||
        trimmed.startsWith("[")) {
        return; // Skip non-SPICE schematic libraries
    }
    
    SimModelParseOptions options;
    options.sourceName = path.toStdString();
    
    // Use a temporary netlist to see what's in THIS file
    SimNetlist tempNetlist;
    std::vector<SimParseDiagnostic> diagnostics;
    
    if (SimModelParser::parseLibrary(tempNetlist, content.toStdString(), options, &diagnostics)) {
        QWriteLocker locker(&m_lock);
        m_loadedFiles.insert(path);
        
        // Add models to master netlist
        for (const auto& [name, model] : tempNetlist.models()) {
            m_masterNetlist.addModel(model);

            // Update model index with modelLevel from parsed SimModel
            for (auto& info : m_modelIndex) {
                if (info.name.compare(QString::fromStdString(name), Qt::CaseInsensitive) == 0) {
                    if (!model.modelLevel.empty()) {
                        info.modelLevel = QString::fromStdString(model.modelLevel);
                    }
                    QStringList paramList;
                    for (const auto& [k, v] : model.params) {
                        paramList.append(QString::fromStdString(k));
                    }
                    info.params = paramList;
                    break;
                }
            }
        }
        
        // Add subcircuits to master netlist
        for (const auto& [name, sub] : tempNetlist.subcircuits()) {
            m_masterNetlist.addSubcircuit(sub);
        }
    }
}
