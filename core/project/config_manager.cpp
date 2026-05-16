#include "config_manager.h"
#include <QDir>

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::~ConfigManager() = default;

ConfigManager::ConfigManager()
    : m_settings("viospice", "Settings") {
    load();
}

bool ConfigManager::autoSaveEnabled() const { return m_autoSaveEnabled; }
void ConfigManager::setAutoSaveEnabled(bool enabled) { m_autoSaveEnabled = enabled; }

int ConfigManager::autoSaveInterval() const { return m_autoSaveInterval; }
void ConfigManager::setAutoSaveInterval(int minutes) { m_autoSaveInterval = minutes; }

QString ConfigManager::currentTheme() const { return m_currentTheme; }
void ConfigManager::setCurrentTheme(const QString& themeName) { m_currentTheme = themeName; }

QString ConfigManager::geminiApiKey() const { return m_geminiApiKey; }
void ConfigManager::setGeminiApiKey(const QString& key) { 
    m_geminiApiKey = key; 
    m_settings.setValue("api/geminiKey", m_geminiApiKey);
    m_settings.sync();
}

QString ConfigManager::geminiGlobalInstructions() const { return m_geminiGlobalInstructions; }
void ConfigManager::setGeminiGlobalInstructions(const QString& instructions) {
    m_geminiGlobalInstructions = instructions;
    m_settings.setValue("api/geminiInstructions", m_geminiGlobalInstructions);
    m_settings.sync();
}

QString ConfigManager::geminiSelectedModel() const { return m_geminiSelectedModel; }
void ConfigManager::setGeminiSelectedModel(const QString& model) {
    m_geminiSelectedModel = model;
    m_settings.setValue("api/geminiSelectedModel", m_geminiSelectedModel);
    m_settings.sync();
}

QString ConfigManager::geminiOverlayModel() const { return m_geminiOverlayModel; }
void ConfigManager::setGeminiOverlayModel(const QString& model) {
    m_geminiOverlayModel = model;
    m_settings.setValue("api/geminiOverlayModel", m_geminiOverlayModel);
    m_settings.sync();
}

QString ConfigManager::geminiChatModel() const { return m_geminiChatModel; }
void ConfigManager::setGeminiChatModel(const QString& model) {
    m_geminiChatModel = model;
    m_settings.setValue("api/geminiChatModel", m_geminiChatModel);
    m_settings.sync();
}

QString ConfigManager::geminiSelectedMode() const { return m_geminiSelectedMode; }
void ConfigManager::setGeminiSelectedMode(const QString& mode) {
    m_geminiSelectedMode = mode;
    m_settings.setValue("api/geminiSelectedMode", m_geminiSelectedMode);
    m_settings.sync();
}

QStringList ConfigManager::availableGeminiModels() const { return m_availableGeminiModels; }
void ConfigManager::setAvailableGeminiModels(const QStringList& models) {
    m_availableGeminiModels = models;
    m_settings.setValue("api/availableModels", m_availableGeminiModels);
    m_settings.sync();
}

QString ConfigManager::octopartApiKey() const { return m_settings.value("api/octopartKey").toString(); }
void ConfigManager::setOctopartApiKey(const QString& key) {
    m_settings.setValue("api/octopartKey", key);
    m_settings.sync();
}

namespace {
QString sanitizeStoredPath(QString p) {
    p = QDir::fromNativeSeparators(p).trimmed();
    if ((p.startsWith('"') && p.endsWith('"')) || (p.startsWith('\'') && p.endsWith('\''))) {
        p = p.mid(1, p.size() - 2).trimmed();
    }
    while (p.startsWith('"') || p.startsWith('\'')) p.remove(0, 1);
    while (p.endsWith('"') || p.endsWith('\'')) p.chop(1);
    return p.trimmed();
}

QStringList uniquePaths(const QStringList& in) {
    QStringList out;
    for (const QString& p : in) {
        const QString trimmed = sanitizeStoredPath(p);
        if (trimmed.isEmpty()) continue;
        if (!out.contains(trimmed)) out.append(trimmed);
    }
    return out;
}
}

QStringList ConfigManager::symbolPaths() const {
    QStringList paths = m_symbolPaths;
    for (const QString& root : m_libraryRoots) {
        if (root.trimmed().isEmpty()) continue;
        paths.append(QDir(root).filePath("sym"));
    }
    return uniquePaths(paths);
}
QStringList ConfigManager::rawSymbolPaths() const { return m_symbolPaths; }
void ConfigManager::setSymbolPaths(const QStringList& paths) { m_symbolPaths = paths; }

QStringList ConfigManager::modelPaths() const {
    QStringList paths = m_modelPaths;
    for (const QString& root : m_libraryRoots) {
        if (root.trimmed().isEmpty()) continue;
        paths.append(QDir(root).filePath("sub"));
        paths.append(QDir(root).filePath("cmp"));
        paths.append(QDir(root).filePath("lib"));
    }
    return uniquePaths(paths);
}
QStringList ConfigManager::rawModelPaths() const { return m_modelPaths; }
void ConfigManager::setModelPaths(const QStringList& paths) { m_modelPaths = paths; }

QStringList ConfigManager::libraryRoots() const { return m_libraryRoots; }
void ConfigManager::setLibraryRoots(const QStringList& roots) { m_libraryRoots = roots; }
bool ConfigManager::kicadDisabled() const { return m_kicadDisabled; }
void ConfigManager::setKicadDisabled(bool disabled) { m_kicadDisabled = disabled; }

bool ConfigManager::kicadBasicsOnly() const { return m_kicadBasicsOnly; }
void ConfigManager::setKicadBasicsOnly(bool enabled) { m_kicadBasicsOnly = enabled; }

// Simulator Settings
QString ConfigManager::defaultSolver() const { return m_defaultSolver; }
void ConfigManager::setDefaultSolver(const QString& solver) { m_defaultSolver = solver; }
QString ConfigManager::integrationMethod() const { return m_integrationMethod; }
void ConfigManager::setIntegrationMethod(const QString& method) { m_integrationMethod = method; }
double ConfigManager::reltol() const { return m_reltol; }
void ConfigManager::setReltol(double val) { m_reltol = val; }
double ConfigManager::abstol() const { return m_abstol; }
void ConfigManager::setAbstol(double val) { m_abstol = val; }
double ConfigManager::vntol() const { return m_vntol; }
void ConfigManager::setVntol(double val) { m_vntol = val; }
double ConfigManager::gmin() const { return m_gmin; }
void ConfigManager::setGmin(double val) { m_gmin = val; }
int ConfigManager::maxIterations() const { return m_maxIterations; }
void ConfigManager::setMaxIterations(int val) { m_maxIterations = val; }

void ConfigManager::saveWindowState(const QString& name, const QByteArray& geometry, const QByteArray& state) {
    m_settings.setValue("ui/" + name + "/geometry", geometry);
    m_settings.setValue("ui/" + name + "/state", state);
    m_settings.sync();
}

QByteArray ConfigManager::windowGeometry(const QString& name) const {
    QByteArray val = m_settings.value("ui/" + name + "/geometry").toByteArray();
    if (val.isEmpty()) {
        if (name == "SchematicEditor") return QByteArray::fromHex("01d9d0cb00030000000000000000001b00000555000002ff000000030000003400000554000002fe00000000020000000556000000000000003300000555000002ff");
        if (name == "PCBEditor") return QByteArray::fromHex("01d9d0cb00030000000000000000001b00000553000002ff000000000000003300000551000002fd00000000020000000554000000000000003300000553000002ff");
        if (name == "ProjectManager") return QByteArray::fromHex("01d9d0cb00030000000000000000001b00000555000002ff000000000000003300000551000002fd00000000020000000556000000000000003300000555000002ff");
        if (name == "SymbolEditor") return QByteArray::fromHex("01d9d0cb00030000000000000000001b00000553000002ff000000000000003300000551000002fd00000000020000000554000000000000003300000553000002ff");
        if (name == "FootprintEditor") return QByteArray::fromHex("01d9d0cb00030000000000000000001b00000553000002ff000000010000003400000552000002fe00000000020000000554000000000000003300000553000002ff");
    }
    return val;
}

QByteArray ConfigManager::windowState(const QString& name) const {
    QByteArray val = m_settings.value("ui/" + name + "/state").toByteArray();
    if (val.isEmpty()) {
        if (name == "SchematicEditor") return QByteArray::fromHex("000000ff00000000fd00000003000000000000019000000289fc0200000001fc0000002d00000289000001840100001cfa000000000200000005fb0000001a0043006f006d0070006f006e0065006e00740044006f0063006b0100000000ffffffff0000016700fffffffb0000002600500072006f006a006500630074004500780070006c006f0072006500720044006f0063006b0100000000ffffffff000000b900fffffffb0000001a0048006900650072006100720063006800790044006f0063006b0100000000ffffffff0000007300fffffffb0000001400470065006d0069006e00690044006f0063006b0100000000ffffffff0000007100fffffffb0000001c0046006c007500780053006300720069007000740044006f0063006b0100000000ffffffff0000000000000000000000010000018c00000289fc0200000002fc0000002f000002710000000000fffffffa000000000200000001fb00000018005400650072006d0069006e0061006c0044006f0063006b0100000000ffffffff0000000000000000fc0000002d000002890000000000fffffffaffffffff0200000002fb0000000e0045005200430044006f0063006b0000000000ffffffff0000007100fffffffb000000220053006f00750072006300650043006f006e00740072006f006c0044006f0063006b0000000000ffffffff0000025e00ffffff000000030000035e0000012cfc0100000001fb0000002c0041006e0061006c006f0067004f007300630069006c006c006f00730063006f007000650044006f0063006b01000001c70000035e0000007800ffffff0000035e0000015700000004000000040000000100000002fc000000030000000000000002000000200053006300680065006d00610074006900630054006f006f006c0062006100720300000000ffffffff00000000000000000000001a004c00610079006f007500740054006f006f006c006200610072030000027dffffffff000000000000000000000001000000010000001c00440072006100770069006e00670054006f006f006c0062006100720300000000ffffffff0000000000000000000000020000000200000016004d00610069006e0054006f006f006c0062006100720100000000ffffffff00000000000000000000001600500072006f007000650072007400790042006100720100000499ffffffff0000000000000000");
        if (name == "PCBEditor") return QByteArray::fromHex("000000ff00000000fd00000002000000000000010000000235fc0200000001fb00000016004c0069006200720061007200790044006f0063006b010000007a00000235000001c900ffffff000000010000019900000235fc0200000001fc0000007a00000235000001b40100001cfa000000000200000004fb00000012004c00610079006500720044006f0063006b0100000000ffffffff0000019700fffffffb0000001c00500072006f00700065007200740069006500730044006f0063006b0100000000ffffffff0000013100fffffffb0000000e0044005200430044006f0063006b0100000000ffffffff000000ff00fffffffb0000001400470065006d0069006e00690044006f0063006b0100000000ffffffff0000002900ffffff000002800000023500000004000000040000006200000062fc00000003000000000000000200000016004d00610069006e0054006f006f006c0062006100720300000000ffffffff00000000000000000000001a004c00610079006f007500740054006f006f006c00620061007203000001f4ffffffff000000000000000000000002000000020000001600500072006f007000650072007400790042006100720100000000ffffffff00000000000000000000001c0054006f0070004d00610069006e0054006f006f006c0062006100720100000076ffffffff000000000000000000000002000000010000001c004f007000740069006f006e00730054006f006f006c0062006100720100000000ffffffff0000000000000000");
        if (name == "ProjectManager") return QByteArray::fromHex("000000ff00000000fd0000000100000003000005540000012cfc0100000001fb000000220050007900740068006f006e0043006f006e0073006f006c00650044006f0063006b0100000000000005540000000000000000000005560000029500000004000000040000006200000062fc00000000");
        if (name == "SymbolEditor") return QByteArray::fromHex("000000ff00000000fd0000000300000000000001620000025afc0200000001fc000000520000025a0000028400fffffffa000000010200000002fb00000016004c0069006200720061007200790044006f0063006b0100000000ffffffff0000012600fffffffb0000001400570069007a0061007200640044006f0063006b0100000000ffffffff0000028400ffffff00000001000001f70000025afc0200000004fc0000005b000001910000000000fffffffa000000000200000002fb0000001c00530079006d0062006f006c0049006e0066006f0044006f0063006b0100000000ffffffff0000000000000000fb0000001400470065006d0069006e00690044006f0063006b0100000000ffffffff0000000000000000fb00000018004d00650074006100640061007400610044006f0063006b0100000162000000710000000000000000fb0000001a00530065006c0065006300740069006f006e0044006f0063006b010000017f0000006d0000000000000000fb0000001c00500072006f00700065007200740069006500730044006f0063006b01000000520000025a0000028d00ffffff00000003000005210000009cfc0100000001fc00000033000005210000000000fffffffaffffffff0100000003fb0000000e00500069006e0044006f0063006b02000000320000022f0000051b000000a2fb000000100043006f006400650044006f0063006b020000003300000225000005210000009cfb0000000e0053005200430044006f0063006b020000003300000225000005210000009c000001c00000025a00000004000000040000006200000062fc00000002000000000000000100000016004c0065006600740054006f006f006c0042006100720300000000ffffffff00000000000000000000000200000001000000140054006f00700054006f006f006c0062006100720100000000ffffffff0000000000000000");
    }
    return val;
}

QStringList ConfigManager::workspaceFolders() const {
    return m_settings.value("workspaceFolders").toStringList();
}

void ConfigManager::setWorkspaceFolders(const QStringList& folders) {
    m_settings.setValue("workspaceFolders", folders);
    m_settings.sync();
}

bool ConfigManager::snapToGrid() const { return m_snapToGrid; }
void ConfigManager::setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }
bool ConfigManager::autoFocusOnCrossProbe() const { return m_autoFocusOnCrossProbe; }
void ConfigManager::setAutoFocusOnCrossProbe(bool enabled) { m_autoFocusOnCrossProbe = enabled; }

bool ConfigManager::autoShowSimulationTab() const { return m_settings.value("ui/autoShowSimulationTab", false).toBool(); }
void ConfigManager::setAutoShowSimulationTab(bool enabled) { 
    m_settings.setValue("ui/autoShowSimulationTab", enabled); 
    m_settings.sync();
}
bool ConfigManager::showNetTableOverlay() const { return m_settings.value("simulator/showNetTableOverlay", true).toBool(); }
void ConfigManager::setShowNetTableOverlay(bool enabled) {
    m_settings.setValue("simulator/showNetTableOverlay", enabled);
    m_settings.sync();
}

void ConfigManager::setToolProperty(const QString& toolName, const QString& propName, const QVariant& value) {
    m_settings.setValue(QString("tools/%1/%2").arg(toolName, propName), value);
    m_settings.sync();
}

QVariant ConfigManager::toolProperty(const QString& toolName, const QString& propName, const QVariant& defaultValue) const {
    return m_settings.value(QString("tools/%1/%2").arg(toolName, propName), defaultValue);
}

bool ConfigManager::isFeatureEnabled(const QString& name, bool defaultVal) const {
    return m_settings.value("features/" + name, defaultVal).toBool();
}

void ConfigManager::setFeatureEnabled(const QString& name, bool enabled) {
    m_settings.setValue("features/" + name, enabled);
    m_settings.sync();
}

bool ConfigManager::aiOverlayEnabled() const { return isFeatureEnabled("ai_overlay", true); }
void ConfigManager::setAiOverlayEnabled(bool enabled) { setFeatureEnabled("ai_overlay", enabled); }
bool ConfigManager::aiChatEnabled() const { return isFeatureEnabled("ai_chat", true); }
void ConfigManager::setAiChatEnabled(bool enabled) { setFeatureEnabled("ai_chat", enabled); }
bool ConfigManager::aiErcEnabled() const { return isFeatureEnabled("ai_erc", true); }
void ConfigManager::setAiErcEnabled(bool enabled) { setFeatureEnabled("ai_erc", enabled); }

void ConfigManager::save() {
    m_settings.setValue("autoSave/enabled", m_autoSaveEnabled);
    m_settings.setValue("autoSave/interval", m_autoSaveInterval);
    m_settings.setValue("appearance/theme", m_currentTheme);
    m_settings.setValue("api/geminiApiKey", m_geminiApiKey);
    m_settings.setValue("api/geminiInstructions", m_geminiGlobalInstructions);
    m_settings.setValue("api/geminiSelectedModel", m_geminiSelectedModel);
    m_settings.setValue("api/geminiOverlayModel", m_geminiOverlayModel);
    m_settings.setValue("api/geminiChatModel", m_geminiChatModel);
    m_settings.setValue("api/geminiSelectedMode", m_geminiSelectedMode);
    m_settings.setValue("api/availableModels", m_availableGeminiModels);
    m_settings.setValue("libraries/symbols", m_symbolPaths);
    m_settings.setValue("libraries/models", m_modelPaths);
    m_settings.setValue("libraries/roots", m_libraryRoots);
    m_settings.setValue("editor/snapToGrid", m_snapToGrid);
    m_settings.setValue("editor/autoFocusOnCrossProbe", m_autoFocusOnCrossProbe);
    m_settings.setValue("editor/realtimeWireUpdate", m_realtimeWireUpdate);
    
    m_settings.setValue("simulator/defaultSolver", m_defaultSolver);
    m_settings.setValue("simulator/integrationMethod", m_integrationMethod);
    m_settings.setValue("simulator/reltol", m_reltol);
    m_settings.setValue("simulator/abstol", m_abstol);
    m_settings.setValue("simulator/vntol", m_vntol);
    m_settings.setValue("simulator/gmin", m_gmin);
    m_settings.setValue("simulator/maxIterations", m_maxIterations);
    m_settings.setValue("libraries/kicadDisabled", m_kicadDisabled);
    m_settings.setValue("libraries/kicadBasicsOnly", m_kicadBasicsOnly);
    
    m_settings.sync();
}

void ConfigManager::load() {
    m_autoSaveEnabled = m_settings.value("autoSave/enabled", false).toBool();
    m_autoSaveInterval = m_settings.value("autoSave/interval", 5).toInt();
    m_currentTheme = m_settings.value("appearance/theme", "Dark").toString();
    m_geminiApiKey = m_settings.value("api/geminiKey", "").toString();
    m_geminiGlobalInstructions = m_settings.value("api/geminiInstructions", "").toString();
    m_geminiSelectedModel = m_settings.value("api/geminiSelectedModel", "gemini-flash-latest").toString();
    m_geminiOverlayModel = m_settings.value("api/geminiOverlayModel", "gemini-3.1-flash-lite-preview").toString();
    m_geminiChatModel = m_settings.value("api/geminiChatModel", "gemini-3.1-flash-lite-preview").toString();
    m_geminiSelectedMode = m_settings.value("api/geminiSelectedMode", "Direct").toString();
    m_availableGeminiModels = m_settings.value("api/availableModels").toStringList();
    m_symbolPaths = m_settings.value("libraries/symbols").toStringList();
    m_modelPaths = m_settings.value("libraries/models").toStringList();
    m_libraryRoots = m_settings.value("libraries/roots").toStringList();
    m_snapToGrid = m_settings.value("editor/snapToGrid", true).toBool();
    m_autoFocusOnCrossProbe = m_settings.value("editor/autoFocusOnCrossProbe", false).toBool();
    m_realtimeWireUpdate = m_settings.value("editor/realtimeWireUpdate", false).toBool();

    m_defaultSolver = m_settings.value("simulator/defaultSolver", "SparseLU").toString();
    m_integrationMethod = m_settings.value("simulator/integrationMethod", "Trapezoidal").toString();
    m_reltol = m_settings.value("simulator/reltol", 1e-3).toDouble();
    m_abstol = m_settings.value("simulator/abstol", 1e-12).toDouble();
    m_vntol = m_settings.value("simulator/vntol", 1e-6).toDouble();
    m_gmin = m_settings.value("simulator/gmin", 1e-12).toDouble();
    m_maxIterations = m_settings.value("simulator/maxIterations", 100).toInt();
    m_kicadDisabled = m_settings.value("libraries/kicadDisabled", false).toBool();
    m_kicadBasicsOnly = m_settings.value("libraries/kicadBasicsOnly", false).toBool();

    // Seed default library root on first run if none are configured.
    if (m_libraryRoots.isEmpty() && m_symbolPaths.isEmpty() && m_modelPaths.isEmpty()) {
        const QString base = QDir::homePath() + "/ViospiceLib";
        QDir().mkpath(QDir(base).filePath("sym"));
        QDir().mkpath(QDir(base).filePath("sub"));
        QDir().mkpath(QDir(base).filePath("cmp"));
        QDir().mkpath(QDir(base).filePath("lib"));
        m_libraryRoots << base;
        save();
    }
}
