#include "mos_properties_dialog.h"
#include "mos_model_picker_dialog.h"
#include "../../pcb/dialogs/footprint_browser_dialog.h"
#include "../items/schematic_item.h"
#include "theme_manager.h"
#include "../../simulator/bridge/model_library_manager.h"

#include <QCompleter>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollArea>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>
#include <QDir>

const QVector<MosPropertiesDialog::LevelInfo>& MosPropertiesDialog::knownLevels() {
    static const QVector<LevelInfo> levels = {
        {"MOS1",     "mos1.json"},
        {"MOS2",     "mos2.json"},
        {"MOS3",     "mos3.json"},
        {"BSIM3",    "bsim3.json"},
        {"BSIM4",    "bsim4.json"},
        {"BSIMSOI",  "bsimsoi.json"},
        {"HISIM2",   "hisim2.json"},
    };
    return levels;
}

MosPropertiesDialog::MosPropertiesDialog(SchematicItem* item, QWidget* parent)
    : QDialog(parent), m_item(item) {
    setWindowTitle(QString("MOSFET Properties - %1").arg(item ? item->reference() : "M?"));
    setModal(true);
    setMinimumWidth(580);
    resize(620, 700);

    setupUI();
    loadValues();
    updateCommandPreview();

    if (ThemeManager::theme()) {
        setStyleSheet(ThemeManager::theme()->widgetStylesheet());
    }
}

void MosPropertiesDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // === Top form: Model Name, Type, Level ===
    auto* topForm = new QFormLayout();
    topForm->setLabelAlignment(Qt::AlignRight);

    m_modelNameEdit = new QLineEdit();
    m_modelNameEdit->setPlaceholderText("e.g. 2N7000 / BS250");
    topForm->addRow("Model Name:", m_modelNameEdit);

    {
        QStringList mosModels;
        for (const auto& info : ModelLibraryManager::instance().allModels()) {
            QString t = info.type.toUpper();
            if (t == "NMOS" || t == "PMOS" || t == "VDMOS" || t == "NMF" || t == "PMF" ||
                t == "BSIM4" || t == "BSIM3" || t == "BSIMSOI") {
                mosModels.append(info.name);
            }
        }
        mosModels.sort(Qt::CaseInsensitive);
        mosModels.removeDuplicates();
        auto* modelCompleter = new QCompleter(mosModels, this);
        modelCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        modelCompleter->setFilterMode(Qt::MatchContains);
        modelCompleter->setCompletionMode(QCompleter::PopupCompletion);
        m_modelNameEdit->setCompleter(modelCompleter);
        connect(modelCompleter, QOverload<const QString&>::of(&QCompleter::activated),
                this, &MosPropertiesDialog::fillFromModel);
    }

    auto* typeLevelLayout = new QHBoxLayout();

    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("NMOS");
    m_typeCombo->addItem("PMOS");
    typeLevelLayout->addWidget(new QLabel("Type:"));
    typeLevelLayout->addWidget(m_typeCombo);

    m_levelCombo = new QComboBox();
    for (const auto& lvl : knownLevels()) {
        m_levelCombo->addItem(lvl.name);
    }
    typeLevelLayout->addWidget(new QLabel("Level:"));
    typeLevelLayout->addWidget(m_levelCombo);
    typeLevelLayout->addStretch();
    topForm->addRow("", typeLevelLayout);

    auto* pickLayout = new QHBoxLayout();
    m_pickModelButton = new QPushButton(isPmos() ? "Pick PMOS Model" : "Pick NMOS Model");
    m_pickModelButton->setFixedHeight(26);
    connect(m_pickModelButton, &QPushButton::clicked, this, [this]() {
        MosModelPickerDialog dlg(isPmosSelected(), this);
        if (dlg.exec() == QDialog::Accepted && !dlg.selectedModel().isEmpty()) {
            fillFromModel(dlg.selectedModel());
        }
    });
    pickLayout->addWidget(m_pickModelButton);
    topForm->addRow("", pickLayout);

    mainLayout->addLayout(topForm);

    // === Scrollable parameter area ===
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    auto* scrollContent = new QWidget();
    m_paramLayout = new QVBoxLayout(scrollContent);
    m_paramLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(m_scrollArea, 1);

    // === Raw params text area ===
    mainLayout->addWidget(new QLabel("Raw Parameters (key=val, one per line):"));
    m_rawParamsEdit = new QTextEdit();
    m_rawParamsEdit->setPlaceholderText("e.g.\nTOX=5e-9\nNCH=2.5e17\nXJ=1.5e-7");
    m_rawParamsEdit->setMaximumHeight(80);
    m_rawParamsEdit->setStyleSheet("font-family: 'Courier New'; font-size: 10pt;");
    mainLayout->addWidget(m_rawParamsEdit);

    // === Footprint ===
    auto* fpRow = new QHBoxLayout();
    m_footprintEdit = new QLineEdit();
    m_footprintEdit->setPlaceholderText("Select a footprint");
    m_footprintEdit->setReadOnly(true);
    auto* fpBtn = new QPushButton("Pick Footprint");
    connect(fpBtn, &QPushButton::clicked, this, &MosPropertiesDialog::pickFootprint);
    fpRow->addWidget(m_footprintEdit, 1);
    fpRow->addWidget(fpBtn);
    mainLayout->addLayout(fpRow);

    // === SPICE Preview ===
    mainLayout->addWidget(new QLabel("SPICE Preview:"));
    m_commandPreview = new QLineEdit();
    m_commandPreview->setReadOnly(true);
    m_commandPreview->setStyleSheet("color: #3b82f6; font-family: 'Courier New';");
    mainLayout->addWidget(m_commandPreview);

    // === Buttons ===
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &MosPropertiesDialog::applyChanges);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    // === Connections ===
    connect(m_modelNameEdit, &QLineEdit::textChanged, this, &MosPropertiesDialog::updateCommandPreview);
    connect(m_modelNameEdit, &QLineEdit::editingFinished, this, &MosPropertiesDialog::autoMatchModel);
    connect(m_typeCombo, &QComboBox::currentTextChanged, this, [this]() {
        if (m_pickModelButton) {
            m_pickModelButton->setText(isPmosSelected() ? "Pick PMOS Model" : "Pick NMOS Model");
        }
        updateCommandPreview();
    });
    connect(m_levelCombo, &QComboBox::currentIndexChanged, this, &MosPropertiesDialog::onLevelChanged);
    connect(m_rawParamsEdit, &QTextEdit::textChanged, this, &MosPropertiesDialog::updateCommandPreview);
}

bool MosPropertiesDialog::isPmos() const {
    if (!m_item) return false;
    const QString t = m_item->itemTypeName().trimmed().toLower();
    return t == "transistor_pmos" ||
           t == "pmos" ||
           t == "pmos4" ||
           m_item->referencePrefix().compare("MP", Qt::CaseInsensitive) == 0;
}

bool MosPropertiesDialog::isPmosSelected() const {
    if (m_typeCombo) {
        return m_typeCombo->currentText().compare("PMOS", Qt::CaseInsensitive) == 0;
    }
    return isPmos();
}

void MosPropertiesDialog::loadModelDef(const QString& levelName) {
    m_currentDef = MosModelDef();

    // First try to load from the model_params directory
    QString searchPath;
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/../core/simulation/model_params/",
        QCoreApplication::applicationDirPath() + "/../../core/simulation/model_params/",
        QDir::currentPath() + "/core/simulation/model_params/",
    };

    for (const auto& lvl : knownLevels()) {
        if (lvl.name == levelName) {
            searchPath = lvl.jsonFile;
            break;
        }
    }

    if (searchPath.isEmpty()) return;

    // Try to find the JSON file
    QString jsonPath;
    for (const auto& base : searchPaths) {
        QString candidate = base + searchPath;
        if (QFile::exists(candidate)) {
            jsonPath = candidate;
            break;
        }
    }

    // Also check alongside the executable
    if (jsonPath.isEmpty()) {
        QString candidate = QCoreApplication::applicationDirPath() + "/" + searchPath;
        if (QFile::exists(candidate)) {
            jsonPath = candidate;
        }
    }

    if (jsonPath.isEmpty()) {
        // Fallback: create inline defaults
        if (levelName == "MOS1") {
            m_currentDef.model = "MOS1";
            m_currentDef.level = 1;
            m_currentDef.spiceType = "NMOS";
            MosParamCategory cat;
            cat.name = "DC";
            cat.params = {{"Vto", "2", "V", "Threshold voltage"},
                          {"Kp", "100u", "A/V^2", "Transconductance parameter"},
                          {"Lambda", "0.02", "V^-1", "Channel length modulation"},
                          {"Gamma", "0", "V^0.5", "Body effect coefficient"},
                          {"Phi", "0.6", "V", "Surface potential"},
                          {"Rd", "1", "ohm", "Drain resistance"},
                          {"Rs", "1", "ohm", "Source resistance"},
                          {"Rg", "0", "ohm", "Gate resistance"}};
            m_currentDef.categories.append(cat);

            MosParamCategory capCat;
            capCat.name = "Capacitance";
            capCat.params = {{"Cgso", "50p", "F/m", "Gate-source overlap capacitance"},
                             {"Cgdo", "50p", "F/m", "Gate-drain overlap capacitance"},
                             {"Cgbo", "0", "F/m", "Gate-body overlap capacitance"},
                             {"Cbd", "0", "F", "Bulk-drain capacitance"},
                             {"Cbs", "0", "F", "Bulk-source capacitance"},
                             {"Pb", "0.8", "V", "Bulk junction potential"}};
            m_currentDef.categories.append(capCat);
        }
        return;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    m_currentDef.model = root["model"].toString();
    m_currentDef.description = root["description"].toString();
    m_currentDef.level = root["level"].toInt(1);
    m_currentDef.spiceType = root["spiceType"].toString("NMOS");
    m_currentDef.spiceLevelParam = root["spiceLevelParam"].toString();

    QJsonArray cats = root["categories"].toArray();
    for (const auto& catVal : cats) {
        QJsonObject catObj = catVal.toObject();
        MosParamCategory cat;
        cat.name = catObj["name"].toString();
        QJsonArray params = catObj["params"].toArray();
        for (const auto& pVal : params) {
            QJsonObject pObj = pVal.toObject();
            MosParamDef def;
            def.name = pObj["name"].toString();
            def.defaultVal = pObj["default"].toString();
            def.unit = pObj["unit"].toString();
            def.desc = pObj["desc"].toString();
            cat.params.append(def);
        }
        m_currentDef.categories.append(cat);
    }
}

void MosPropertiesDialog::rebuildParamForm(const MosModelDef& def) {
    // Clear existing param widgets
    for (auto* w : m_categoryWidgets) {
        m_paramLayout->removeWidget(w);
        w->deleteLater();
    }
    m_categoryWidgets.clear();
    m_paramEdits.clear();

    if (def.categories.isEmpty()) return;

    for (const auto& cat : def.categories) {
        auto* groupBox = new QGroupBox(cat.name);
        groupBox->setCheckable(false);
        auto* form = new QFormLayout(groupBox);
        form->setLabelAlignment(Qt::AlignRight);

        for (const auto& param : cat.params) {
            auto* edit = new QLineEdit();
            edit->setText(param.defaultVal);
            QString unitStr = param.unit.isEmpty() ? "" : QString(" [%1]").arg(param.unit);
            QString tooltip = param.desc;
            if (!param.unit.isEmpty()) tooltip += QString("\nUnit: %1").arg(param.unit);
            edit->setToolTip(tooltip);

            QString label = param.name;
            if (!unitStr.isEmpty()) label += unitStr;
            form->addRow(label + ":", edit);

            connect(edit, &QLineEdit::textChanged, this, &MosPropertiesDialog::updateCommandPreview);
            m_paramEdits[param.name.toUpper()] = edit;
        }

        m_paramLayout->addWidget(groupBox);
        m_categoryWidgets.append(groupBox);
    }

    // Add stretch at end
    m_paramLayout->addStretch();
}

void MosPropertiesDialog::onLevelChanged(int index) {
    if (index < 0 || index >= knownLevels().size()) return;
    const QString levelName = knownLevels()[index].name;
    loadModelDef(levelName);
    rebuildParamForm(m_currentDef);

    // Try to fill values from existing item data
    if (m_item) {
        const auto pe = m_item->paramExpressions();
        for (auto it = pe.begin(); it != pe.end(); ++it) {
            QString key = it.key().toUpper();
            if (key.startsWith("MOS.")) key = key.mid(4);
            if (m_paramEdits.contains(key)) {
                m_paramEdits[key]->setText(it.value());
            }
        }
    }

    // Update default model name when level changes
    if (m_modelNameEdit) {
        const QString currentName = m_modelNameEdit->text().trimmed();
        const bool pmos = isPmosSelected();
        const QString oldDefaultNMOS = "2N7000";
        const QString oldDefaultPMOS = "BS250";

        // Compute the expected default for the new level
        QString newDefault;
        if (levelName == "BSIM4") {
            newDefault = pmos ? "BSIM4_PMOS" : "BSIM4_NMOS";
        } else if (levelName == "BSIM3") {
            newDefault = pmos ? "BSIM3_PMOS" : "BSIM3_NMOS";
        } else if (levelName == "BSIMSOI") {
            newDefault = pmos ? "BSIMSOI_PMOS" : "BSIMSOI_NMOS";
        } else if (levelName == "HISIM2") {
            newDefault = pmos ? "HISIM2_PMOS" : "HISIM2_NMOS";
        } else {
            newDefault = pmos ? oldDefaultPMOS : oldDefaultNMOS;
        }

        // All level-specific defaults (to detect stale names from other levels)
        const QStringList otherDefaults = {
            oldDefaultNMOS, oldDefaultPMOS,
            pmos ? "BSIM4_PMOS"   : "BSIM4_NMOS",
            pmos ? "BSIM3_PMOS"   : "BSIM3_NMOS",
            pmos ? "BSIMSOI_PMOS" : "BSIMSOI_NMOS",
            pmos ? "HISIM2_PMOS"  : "HISIM2_NMOS",
        };

        if (otherDefaults.contains(currentName) && currentName != newDefault) {
            m_modelNameEdit->setText(newDefault);
        }
    }

    updateCommandPreview();
}

void MosPropertiesDialog::loadValues() {
    if (!m_item) return;

    const auto pe = m_item->paramExpressions();
    const QString typeExpr = pe.value("mos.type").trimmed();
    const bool pmos = typeExpr.isEmpty() ? isPmos() : (typeExpr.compare("PMOS", Qt::CaseInsensitive) == 0);
    const QString defaultModel = pmos ? "BS250" : "2N7000";

    if (m_typeCombo) {
        m_typeCombo->setCurrentText(pmos ? "PMOS" : "NMOS");
    }

    QString modelName = m_item->spiceModel().trimmed();
    if (modelName.isEmpty()) {
        modelName = m_item->value().trimmed();
        if (modelName.isEmpty() || modelName.compare("NMOS", Qt::CaseInsensitive) == 0 ||
            modelName.compare("PMOS", Qt::CaseInsensitive) == 0) {
            modelName = defaultModel;
        }
    }
    m_modelNameEdit->setText(modelName);

    // Determine model level
    QString level = pe.value("mos.level").trimmed();
    if (level.isEmpty()) {
        const SimModel* mdl = ModelLibraryManager::instance().findModel(modelName);
        if (mdl && !mdl->modelLevel.empty()) {
            level = QString::fromStdString(mdl->modelLevel);
        }
    }
    if (level.isEmpty()) {
        level = "MOS1";
    }

    // Set level combo
    for (int i = 0; i < m_levelCombo->count(); ++i) {
        if (m_levelCombo->itemText(i).compare(level, Qt::CaseInsensitive) == 0) {
            m_levelCombo->setCurrentIndex(i);
            break;
        }
    }

    // Load model definition and rebuild form
    onLevelChanged(m_levelCombo->currentIndex());

    // Fill values from stored params
    for (auto it = pe.begin(); it != pe.end(); ++it) {
        QString key = it.key();
        if (key == "mos.type" || key == "mos.level") continue;

        // Strip "mos." prefix
        QString cleanKey = key;
        if (cleanKey.startsWith("mos.", Qt::CaseInsensitive))
            cleanKey = cleanKey.mid(4);

        QString upperKey = cleanKey.toUpper();
        if (m_paramEdits.contains(upperKey)) {
            m_paramEdits[upperKey]->setText(it.value());
        }
    }

    // Fill raw params from stored "mos.raw" key
    QString rawParams = pe.value("mos.raw").trimmed();
    if (!rawParams.isEmpty()) {
        m_rawParamsEdit->setPlainText(rawParams);
    }

    if (m_footprintEdit) {
        m_footprintEdit->setText(m_item->footprint());
    }

    // Load params from model library if available
    const SimModel* mdl = ModelLibraryManager::instance().findModel(modelName);
    if (mdl && m_typeCombo) {
        if (mdl->type == SimComponentType::MOSFET_PMOS) m_typeCombo->setCurrentText("PMOS");
        else if (mdl->type == SimComponentType::MOSFET_NMOS) m_typeCombo->setCurrentText("NMOS");

        // Fill params from model
        for (const auto& [k, v] : mdl->params) {
            QString key = QString::fromStdString(k).toUpper();
            if (m_paramEdits.contains(key) && m_paramEdits[key]->text().trimmed().isEmpty()) {
                m_paramEdits[key]->setText(QString::number(v, 'g', 12));
            }
        }
    }
}

void MosPropertiesDialog::fillFromModel(const QString& modelName) {
    const SimModel* mdl = ModelLibraryManager::instance().findModel(modelName);
    if (!mdl) return;

    m_modelNameEdit->setText(modelName);
    if (m_typeCombo) {
        if (mdl->type == SimComponentType::MOSFET_PMOS) m_typeCombo->setCurrentText("PMOS");
        else if (mdl->type == SimComponentType::MOSFET_NMOS) m_typeCombo->setCurrentText("NMOS");
    }

    // Set level from model
    if (!mdl->modelLevel.empty()) {
        QString lvl = QString::fromStdString(mdl->modelLevel);
        for (int i = 0; i < m_levelCombo->count(); ++i) {
            if (m_levelCombo->itemText(i).compare(lvl, Qt::CaseInsensitive) == 0) {
                m_levelCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // Fill param fields
    for (const auto& [k, v] : mdl->params) {
        QString key = QString::fromStdString(k).toUpper();
        if (m_paramEdits.contains(key)) {
            m_paramEdits[key]->setText(QString::number(v, 'g', 12));
        }
    }

    updateCommandPreview();
}

void MosPropertiesDialog::autoMatchModel() {
    const QString name = m_modelNameEdit->text().trimmed();
    if (name.isEmpty()) return;

    const SimModel* mdl = ModelLibraryManager::instance().findModel(name);
    if (!mdl) return;

    bool typeMatch = false;
    if (mdl->type == SimComponentType::MOSFET_NMOS && m_typeCombo->currentText() == "NMOS") typeMatch = true;
    if (mdl->type == SimComponentType::MOSFET_PMOS && m_typeCombo->currentText() == "PMOS") typeMatch = true;
    if (!typeMatch) return;

    // Set level from matched model
    if (!mdl->modelLevel.empty()) {
        QString lvl = QString::fromStdString(mdl->modelLevel);
        for (int i = 0; i < m_levelCombo->count(); ++i) {
            if (m_levelCombo->itemText(i).compare(lvl, Qt::CaseInsensitive) == 0) {
                m_levelCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    for (const auto& [k, v] : mdl->params) {
        QString key = QString::fromStdString(k).toUpper();
        if (m_paramEdits.contains(key)) {
            const QString current = m_paramEdits[key]->text().trimmed();
            if (current.isEmpty()) {
                m_paramEdits[key]->setText(QString::number(v, 'g', 12));
            }
        }
    }

    updateCommandPreview();
}

void MosPropertiesDialog::updateCommandPreview() {
    QString model = modelName();
    if (model.isEmpty()) {
        model = isPmosSelected() ? "BS250" : "2N7000";
    }

    QStringList params;

    // Add params from form fields
    for (auto it = m_paramEdits.begin(); it != m_paramEdits.end(); ++it) {
        const QString v = it.value()->text().trimmed();
        if (!v.isEmpty()) {
            params << QString("%1=%2").arg(it.key(), v);
        }
    }

    // Add raw params
    QString rawText = m_rawParamsEdit->toPlainText().trimmed();
    if (!rawText.isEmpty()) {
        QStringList rawLines = rawText.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : rawLines) {
            QString trimmed = line.trimmed();
            if (trimmed.contains('=')) {
                params << trimmed;
            }
        }
    }

    // Determine SPICE model type — all use NMOS/PMOS + LEVEL=N
    const QString levelName = m_levelCombo->currentText();
    const bool pmos = isPmosSelected();
    QString spiceType = pmos ? "PMOS" : "NMOS";
    QString levelInsert;
    if (levelName == "BSIM4") {
        levelInsert = "LEVEL=14";
    } else if (levelName == "BSIM3") {
        levelInsert = "LEVEL=8";
    } else if (levelName == "BSIMSOI") {
        levelInsert = "LEVEL=10";
    } else if (levelName == "BSIM3SOI") {
        levelInsert = "LEVEL=55";
    } else if (levelName == "HISIM2") {
        levelInsert = "LEVEL=68";
    } else if (levelName == "HISIM_HV") {
        levelInsert = "LEVEL=73";
    } else if (levelName == "MOS2") {
        levelInsert = "LEVEL=2";
    } else if (levelName == "MOS3") {
        levelInsert = "LEVEL=3";
    }

    if (!levelInsert.isEmpty()) {
        params.prepend(levelInsert);
    }

    m_commandPreview->setText(QString(".model %1 %2(%3)").arg(model, spiceType, params.join(" ")));
}

void MosPropertiesDialog::applyChanges() {
    accept();
}

QString MosPropertiesDialog::modelName() const {
    return m_modelNameEdit ? m_modelNameEdit->text().trimmed() : QString();
}

QString MosPropertiesDialog::modelLevel() const {
    return m_levelCombo ? m_levelCombo->currentText() : QString();
}

QMap<QString, QString> MosPropertiesDialog::paramExpressions() const {
    QMap<QString, QString> pe;

    auto add = [&](const QString& key, const QString& val) {
        if (!val.isEmpty()) pe[key] = val;
    };

    for (auto it = m_paramEdits.begin(); it != m_paramEdits.end(); ++it) {
        add(QString("mos.%1").arg(it.key()), it.value()->text().trimmed());
    }

    // Raw params
    QString rawText = m_rawParamsEdit->toPlainText().trimmed();
    if (!rawText.isEmpty()) {
        add("mos.raw", rawText);

        // Also add individual raw params for netlist generation
        QStringList rawLines = rawText.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : rawLines) {
            QString trimmed = line.trimmed();
            int eqPos = trimmed.indexOf('=');
            if (eqPos > 0) {
                QString key = "mos." + trimmed.left(eqPos).trimmed().toUpper();
                QString val = trimmed.mid(eqPos + 1).trimmed();
                if (!pe.contains(key)) {
                    add(key, val);
                }
            }
        }
    }

    add("mos.type", isPmosSelected() ? "PMOS" : "NMOS");
    add("mos.level", m_levelCombo->currentText());

    return pe;
}

QString MosPropertiesDialog::newSymbolName() const {
    return isPmosSelected() ? "pmos" : "nmos";
}

QString MosPropertiesDialog::footprint() const {
    return m_footprintEdit ? m_footprintEdit->text().trimmed() : QString();
}

void MosPropertiesDialog::pickFootprint() {
    FootprintBrowserDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        FootprintDefinition fp = dlg.selectedFootprint();
        if (!fp.name().isEmpty()) {
            m_footprintEdit->setText(fp.name());
        }
    }
}