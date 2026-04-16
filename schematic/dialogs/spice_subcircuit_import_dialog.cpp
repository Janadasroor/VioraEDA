#include "spice_subcircuit_import_dialog.h"

#include "../ui/spice_highlighter.h"
#include "../../simulator/bridge/model_library_manager.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProgressBar>
#include "../../python/cpp/core/flux_script_manager.h"

namespace {

QStringList collapseContinuationLines(const QString& text) {
    QStringList collapsed;
    QString current;

    const QStringList lines = text.split('\n');
    for (const QString& rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.startsWith('+')) {
            const QString continuation = trimmed.mid(1).trimmed();
            if (current.isEmpty()) {
                current = continuation;
            } else if (!continuation.isEmpty()) {
                if (!current.endsWith(' ')) current += ' ';
                current += continuation;
            }
            continue;
        }

        if (!current.isEmpty()) collapsed.append(current);
        current = rawLine;
    }

    if (!current.isEmpty()) collapsed.append(current);
    return collapsed;
}

QString sanitizeFileStem(QString text) {
    text = text.trimmed();
    text.replace(QRegularExpression("[^A-Za-z0-9_.-]+"), "_");
    text.replace(QRegularExpression("_+"), "_");
    text.remove(QRegularExpression("^_+|_+$"));
    return text.isEmpty() ? QString("subckt_model") : text;
}

}

SpiceSubcircuitImportDialog::SpiceSubcircuitImportDialog(const QString& projectDir,
                                                         const QString& currentFilePath,
                                                         QWidget* parent)
    : QDialog(parent)
    , m_projectDir(projectDir)
    , m_currentFilePath(currentFilePath)
    , m_textEdit(nullptr)
    , m_nameEdit(nullptr)
    , m_fileNameEdit(nullptr)
    , m_pathPreviewLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_pinTable(nullptr)
    , m_insertIncludeCheck(nullptr)
    , m_openSymbolEditorCheck(nullptr)
    , m_autoPlaceAfterSaveCheck(nullptr)
    , m_buttonBox(nullptr)
    , m_highlighter(nullptr) {
    setupUi();
    updateFromText();
}

void SpiceSubcircuitImportDialog::setupUi() {
    setWindowTitle("Import SPICE Subcircuit");
    resize(860, 680);

    auto* mainLayout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        "Paste a reusable .subckt/.ends block. The dialog extracts the subcircuit name and pins, "
        "saves it into your project library, and can insert an .include directive into the schematic.",
        this);
    intro->setWordWrap(true);
    mainLayout->addWidget(intro);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(
        ".subckt opamp 1 2 3 4 5\n"
        "E1 3 0 1 2 100k\n"
        "R1 3 0 1k\n"
        ".ends opamp");
    QFont mono("Courier New");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(10);
    m_textEdit->setFont(mono);
    m_highlighter = new SpiceHighlighter(m_textEdit->document());
    
    // AI Generate Button row
    auto* aiLayout = new QHBoxLayout();
    m_aiGenerateBtn = new QPushButton("✨ Generate with Viora AI...", this);
    m_aiGenerateBtn->setStyleSheet(
        "QPushButton { background-color: #3b82f6; color: white; font-weight: bold; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #2563eb; }"
        "QPushButton:disabled { background-color: #94a3b8; }"
    );
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMaximumHeight(4);
    m_progressBar->hide();
    
    aiLayout->addWidget(m_aiGenerateBtn);
    aiLayout->addStretch();
    aiLayout->addWidget(m_progressBar, 1);
    mainLayout->addLayout(aiLayout);

    mainLayout->addWidget(m_textEdit, 1);

    auto* formLayout = new QGridLayout();
    formLayout->addWidget(new QLabel("Subcircuit Name", this), 0, 0);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setReadOnly(true);
    formLayout->addWidget(m_nameEdit, 0, 1);

    m_subcktPickerLabel = new QLabel("Selected Subckt", this);
    m_subcktPicker = new QComboBox(this);
    m_subcktPicker->setEnabled(false);
    formLayout->addWidget(m_subcktPickerLabel, 1, 0);
    formLayout->addWidget(m_subcktPicker, 1, 1);

    formLayout->addWidget(new QLabel("Library File", this), 2, 0);
    m_fileNameEdit = new QLineEdit(this);
    m_fileNameEdit->setPlaceholderText("opamp.lib");
    formLayout->addWidget(m_fileNameEdit, 2, 1);

    formLayout->addWidget(new QLabel("Save Path", this), 3, 0);
    m_pathPreviewLabel = new QLabel(this);
    m_pathPreviewLabel->setWordWrap(true);
    m_pathPreviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    formLayout->addWidget(m_pathPreviewLabel, 3, 1);
    mainLayout->addLayout(formLayout);

    auto* pinsLabel = new QLabel("Pin Mapping Setup", this);
    mainLayout->addWidget(pinsLabel);

    m_pinTable = new QTableWidget(this);
    m_pinTable->setColumnCount(3);
    m_pinTable->setHorizontalHeaderLabels({"Subckt Pin", "Symbol Pin #", "Symbol Label"});
    m_pinTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_pinTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_pinTable->horizontalHeader()->setStretchLastSection(true);
    m_pinTable->verticalHeader()->setVisible(false);
    m_pinTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_pinTable->setMinimumHeight(160);
    mainLayout->addWidget(m_pinTable);

    m_insertIncludeCheck = new QCheckBox("Insert an .include directive onto the current schematic", this);
    m_insertIncludeCheck->setChecked(true);
    mainLayout->addWidget(m_insertIncludeCheck);

    m_openSymbolEditorCheck = new QCheckBox("Generate a mapped symbol template and open it in Symbol Editor", this);
    m_openSymbolEditorCheck->setChecked(true);
    mainLayout->addWidget(m_openSymbolEditorCheck);

    m_autoPlaceAfterSaveCheck = new QCheckBox("Place the generated symbol in the schematic after the first save", this);
    m_autoPlaceAfterSaveCheck->setChecked(true);
    mainLayout->addWidget(m_autoPlaceAfterSaveCheck);
    m_autoPlaceAfterSaveCheck->setEnabled(m_openSymbolEditorCheck->isChecked());

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);
    mainLayout->addWidget(m_statusLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SpiceSubcircuitImportDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &SpiceSubcircuitImportDialog::updateFromText);
    connect(m_fileNameEdit, &QLineEdit::textChanged, this, &SpiceSubcircuitImportDialog::refreshPathPreview);
    connect(m_subcktPicker, &QComboBox::currentIndexChanged, this, &SpiceSubcircuitImportDialog::onSelectedSubcktChanged);
    connect(m_openSymbolEditorCheck, &QCheckBox::toggled, m_autoPlaceAfterSaveCheck, &QWidget::setEnabled);
    connect(m_aiGenerateBtn, &QPushButton::clicked, this, &SpiceSubcircuitImportDialog::onAiGenerateClicked);
}

QString SpiceSubcircuitImportDialog::baseDirectory() const {
    return QDir::homePath() + "/ViospiceLib";
}

QString SpiceSubcircuitImportDialog::suggestedFileName(const QString& subcktName) const {
    return sanitizeFileStem(subcktName) + ".lib";
}

QString SpiceSubcircuitImportDialog::targetAbsolutePath() const {
    const QString baseDir = baseDirectory();
    const QString fileName = m_fileNameEdit->text().trimmed();
    return QDir(baseDir).filePath(QStringLiteral("sub/%1").arg(fileName));
}

void SpiceSubcircuitImportDialog::refreshPathPreview() {
    m_pathPreviewLabel->setText(targetAbsolutePath());
}

QList<SpiceSubcircuitImportDialog::ParsedSubckt> SpiceSubcircuitImportDialog::parseSubckts(const QString& text,
                                                                                            QStringList* errors,
                                                                                            QStringList* warnings) const {
    const QStringList lines = collapseContinuationLines(text);
    QList<ParsedSubckt> parsed;
    QStringList subcktStack;

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        const int lineNo = i + 1;
        if (line.isEmpty() || line.startsWith('*') || line.startsWith(';') || line.startsWith('#')) continue;
        if (!line.startsWith('.')) continue;

        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        const QString card = parts.first().toLower();

        if (card == ".subckt") {
            if (parts.size() < 2) {
                if (errors) errors->append(QString("Line %1: .subckt is missing a subcircuit name.").arg(lineNo));
                continue;
            }

            ParsedSubckt subckt;
            subckt.name = parts.at(1);
            for (int p = 2; p < parts.size(); ++p) {
                if (parts.at(p).contains('=')) break;
                subckt.pins.append(parts.at(p));
            }
            parsed.append(subckt);
            subcktStack.append(parts.at(1));
        } else if (card == ".ends") {
            if (subcktStack.isEmpty()) {
                if (errors) errors->append(QString("Line %1: .ends has no matching .subckt.").arg(lineNo));
            } else {
                const QString openName = subcktStack.takeLast();
                if (parts.size() >= 2 && parts.at(1).compare(openName, Qt::CaseInsensitive) != 0) {
                    if (errors) errors->append(QString("Line %1: .ends %2 does not match open .subckt %3.").arg(lineNo).arg(parts.at(1), openName));
                }
            }
        }
    }

    if (parsed.isEmpty()) {
        if (errors) errors->append("No .subckt definition found.");
    } else if (parsed.size() > 1) {
        if (warnings) warnings->append(QString("Library contains %1 .subckt definitions. The selected one will drive symbol metadata, while the entire library file will be preserved on disk.").arg(parsed.size()));
    }
    if (!subcktStack.isEmpty() && errors) {
        errors->append(QString("Missing .ends for subcircuit %1.").arg(subcktStack.last()));
    }

    return parsed;
}

void SpiceSubcircuitImportDialog::rebuildPinTable(const QStringList& pins) {
    const QList<Result::PinMapping> previousMappings = m_result.pinMappings;

    m_pinTable->setRowCount(pins.size());
    for (int row = 0; row < pins.size(); ++row) {
        const QString subcktPin = pins.at(row);
        QString symbolLabel = subcktPin;
        int symbolPinNumber = row + 1;

        for (const Result::PinMapping& prev : previousMappings) {
            if (prev.subcktPin.compare(subcktPin, Qt::CaseInsensitive) == 0) {
                if (!prev.symbolPinName.trimmed().isEmpty()) symbolLabel = prev.symbolPinName.trimmed();
                if (prev.symbolPinNumber > 0) symbolPinNumber = prev.symbolPinNumber;
                break;
            }
        }

        auto* subcktPinItem = new QTableWidgetItem(subcktPin);
        subcktPinItem->setFlags(subcktPinItem->flags() & ~Qt::ItemIsEditable);
        auto* symbolPinNumberItem = new QTableWidgetItem(QString::number(symbolPinNumber));
        auto* symbolLabelItem = new QTableWidgetItem(symbolLabel);

        m_pinTable->setItem(row, 0, subcktPinItem);
        m_pinTable->setItem(row, 1, symbolPinNumberItem);
        m_pinTable->setItem(row, 2, symbolLabelItem);
    }
}

void SpiceSubcircuitImportDialog::updateFromText() {
    const QString text = m_textEdit->toPlainText();
    const QString previousSubcktName = m_result.subcktName;

    QStringList errors;
    QStringList warnings;
    m_parsedSubckts = parseSubckts(text, &errors, &warnings);

    const QString oldSelection = m_subcktPicker->currentData().toString();
    QSignalBlocker blocker(m_subcktPicker);
    m_subcktPicker->clear();
    for (const ParsedSubckt& parsed : m_parsedSubckts) {
        m_subcktPicker->addItem(QString("%1  (%2 pin%3)").arg(parsed.name).arg(parsed.pins.size()).arg(parsed.pins.size() == 1 ? "" : "s"),
                                parsed.name);
    }
    m_subcktPicker->setEnabled(m_parsedSubckts.size() > 1);
    m_subcktPickerLabel->setVisible(m_parsedSubckts.size() > 1);
    m_subcktPicker->setVisible(m_parsedSubckts.size() > 1);

    int selectedIndex = 0;
    if (!oldSelection.isEmpty()) {
        const int idx = m_subcktPicker->findData(oldSelection);
        if (idx >= 0) selectedIndex = idx;
    } else if (!previousSubcktName.isEmpty()) {
        const int idx = m_subcktPicker->findData(previousSubcktName);
        if (idx >= 0) selectedIndex = idx;
    }
    if (m_subcktPicker->count() > 0) m_subcktPicker->setCurrentIndex(selectedIndex);

    QString subcktName;
    QStringList pins;
    if (selectedIndex >= 0 && selectedIndex < m_parsedSubckts.size()) {
        subcktName = m_parsedSubckts.at(selectedIndex).name;
        pins = m_parsedSubckts.at(selectedIndex).pins;
    }

    m_nameEdit->setText(subcktName);
    const QString currentFileName = m_fileNameEdit->text().trimmed();
    if (!m_preferredLibraryFileName.trimmed().isEmpty()) {
        if (currentFileName.isEmpty() || currentFileName == suggestedFileName(previousSubcktName) || currentFileName == previousSubcktName + ".lib") {
            m_fileNameEdit->setText(m_preferredLibraryFileName);
        }
    } else if (!subcktName.isEmpty() && (currentFileName.isEmpty() || currentFileName == suggestedFileName(previousSubcktName))) {
        m_fileNameEdit->setText(suggestedFileName(subcktName));
    }
    refreshPathPreview();
    rebuildPinTable(pins);

    QStringList messages;
    if (!errors.isEmpty()) {
        messages << "Errors:";
        messages << errors;
    }
    if (!warnings.isEmpty()) {
        if (!messages.isEmpty()) messages << QString();
        messages << "Warnings:";
        messages << warnings;
    }

    if (messages.isEmpty()) {
        m_statusLabel->setStyleSheet("color: #15803d;");
        const QString summary = (m_parsedSubckts.size() > 1)
            ? QString("Ready to save library with %1 subcircuits. Symbol metadata is mapped to %2 (%3 pin(s)).")
                  .arg(m_parsedSubckts.size())
                  .arg(subcktName.isEmpty() ? QString("the selected subcircuit") : subcktName)
                  .arg(pins.size())
            : QString("Ready to save %1 with %2 pin(s).").arg(subcktName.isEmpty() ? QString("subcircuit") : subcktName).arg(pins.size());
        m_statusLabel->setText(summary);
    } else if (!errors.isEmpty()) {
        m_statusLabel->setStyleSheet("color: #b91c1c;");
        m_statusLabel->setText(messages.join('\n'));
    } else {
        m_statusLabel->setStyleSheet("color: #b45309;");
        m_statusLabel->setText(messages.join('\n'));
    }

    m_result.subcktName = subcktName;
    m_result.pins = pins;
    m_result.netlistText = text.trimmed();
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(errors.isEmpty() && !subcktName.isEmpty());
}

void SpiceSubcircuitImportDialog::onAccepted() {
    const QString subcktName = m_nameEdit->text().trimmed();
    QString fileName = m_fileNameEdit->text().trimmed();
    if (subcktName.isEmpty()) {
        QMessageBox::warning(this, "Import SPICE Subcircuit", "A valid .subckt definition is required.");
        return;
    }
    if (fileName.isEmpty()) fileName = suggestedFileName(subcktName);
    if (!fileName.contains('.')) fileName += ".lib";

    const QString baseDir = baseDirectory();
    QDir dir(baseDir);
    if (!dir.mkpath("sub")) {
        QMessageBox::warning(this, "Import SPICE Subcircuit", "Failed to create the global sub library folder.");
        return;
    }

    const QString absolutePath = QDir(baseDir).filePath(QStringLiteral("sub/%1").arg(fileName));
    QFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, "Import SPICE Subcircuit", QString("Failed to write %1.").arg(absolutePath));
        return;
    }

    QString netlistText = m_textEdit->toPlainText().trimmed();
    if (!netlistText.endsWith('\n')) netlistText += '\n';
    file.write(netlistText.toUtf8());
    file.close();

    const QString relativeIncludePath = QDir::fromNativeSeparators(QDir(baseDir).relativeFilePath(absolutePath));
    ModelLibraryManager::instance().reload();

    QList<Result::PinMapping> pinMappings;
    QSet<int> usedSymbolPinNumbers;
    for (int row = 0; row < m_pinTable->rowCount(); ++row) {
        QTableWidgetItem* subcktPinItem = m_pinTable->item(row, 0);
        QTableWidgetItem* symbolPinNumberItem = m_pinTable->item(row, 1);
        QTableWidgetItem* symbolLabelItem = m_pinTable->item(row, 2);
        if (!subcktPinItem || !symbolPinNumberItem || !symbolLabelItem) continue;

        Result::PinMapping mapping;
        mapping.subcktPin = subcktPinItem->text().trimmed();
        mapping.symbolPinName = symbolLabelItem->text().trimmed();
        mapping.symbolPinNumber = symbolPinNumberItem->text().toInt();

        if (mapping.subcktPin.isEmpty()) continue;
        if (mapping.symbolPinName.isEmpty()) mapping.symbolPinName = mapping.subcktPin;
        if (mapping.symbolPinNumber <= 0) {
            QMessageBox::warning(this, "Import SPICE Subcircuit", QString("Symbol pin number for %1 must be greater than zero.").arg(mapping.subcktPin));
            return;
        }
        if (usedSymbolPinNumbers.contains(mapping.symbolPinNumber)) {
            QMessageBox::warning(this, "Import SPICE Subcircuit", QString("Duplicate symbol pin number %1 in mapping table.").arg(mapping.symbolPinNumber));
            return;
        }
        usedSymbolPinNumbers.insert(mapping.symbolPinNumber);
        pinMappings.append(mapping);
    }

    m_result.subcktName = subcktName;
    m_result.fileName = fileName;
    m_result.absolutePath = absolutePath;
    m_result.relativeIncludePath = relativeIncludePath;
    m_result.netlistText = netlistText.trimmed();
    m_result.pinMappings = pinMappings;
    m_result.insertIncludeDirective = m_insertIncludeCheck->isChecked();
    m_result.openSymbolEditor = m_openSymbolEditorCheck->isChecked();
    m_result.autoPlaceAfterSave = m_openSymbolEditorCheck->isChecked() && m_autoPlaceAfterSaveCheck->isChecked();

    accept();
}

void SpiceSubcircuitImportDialog::setPreloadedNetlist(const QString& netlist) {
    if (m_textEdit) {
        m_textEdit->setPlainText(netlist);
    }
}

void SpiceSubcircuitImportDialog::setSuggestedLibraryFileName(const QString& fileName) {
    m_preferredLibraryFileName = QFileInfo(fileName).fileName().trimmed();
    if (!m_preferredLibraryFileName.isEmpty() && m_fileNameEdit && m_fileNameEdit->text().trimmed().isEmpty()) {
        m_fileNameEdit->setText(m_preferredLibraryFileName);
    }
    refreshPathPreview();
}

void SpiceSubcircuitImportDialog::onSelectedSubcktChanged() {
    if (!m_textEdit) return;
    updateFromText();
}

void SpiceSubcircuitImportDialog::onAiGenerateClicked() {
    bool ok;
    QString prompt = QInputDialog::getText(this, "Viora AI Subcircuit Generator",
                                          "Enter the component you want to generate (e.g. 'LM741 op amp', 'IRFZ44N MOSFET'):",
                                          QLineEdit::Normal, "", &ok);
    if (!ok || prompt.trimmed().isEmpty()) return;

    m_aiGenerateBtn->setEnabled(false);
    m_progressBar->show();
    m_statusLabel->setText("Viora AI is synthesizing your component model...");
    m_aiResponseBuffer.clear();

    if (m_aiProcess) {
        m_aiProcess->kill();
        m_aiProcess->deleteLater();
    }

    m_aiProcess = new QProcess(this);
    m_aiProcess->setProcessEnvironment(FluxScriptManager::getConfiguredEnvironment());

    connect(m_aiProcess, &QProcess::readyReadStandardOutput, this, &SpiceSubcircuitImportDialog::onAiProcessReadyRead);
    connect(m_aiProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &SpiceSubcircuitImportDialog::onAiProcessFinished);

    QString scriptPath = QDir(FluxScriptManager::getScriptsDir()).absoluteFilePath("gemini_query.py");
    QString pythonExec = FluxScriptManager::getPythonExecutable();

    QStringList args;
    args << scriptPath << prompt.trimmed() << "--mode" << "subcircuit";

    m_aiProcess->start(pythonExec, args);
}

void SpiceSubcircuitImportDialog::onAiProcessReadyRead() {
    m_aiResponseBuffer += QString::fromUtf8(m_aiProcess->readAllStandardOutput());
}

void SpiceSubcircuitImportDialog::onAiProcessFinished(int exitCode) {
    m_progressBar->hide();
    m_aiGenerateBtn->setEnabled(true);

    if (exitCode != 0) {
        m_statusLabel->setText("Error: AI generation failed.");
        QMessageBox::critical(this, "AI Error", "The Viora AI process failed. Please check your internet connection and Gemini API key.");
        return;
    }

    // Extraction logic for JSON
    QString jsonStr = m_aiResponseBuffer.trimmed();
    int start = jsonStr.indexOf('{');
    int end = jsonStr.lastIndexOf('}');
    if (start != -1 && end != -1 && end > start) {
        jsonStr = jsonStr.mid(start, end - start + 1);
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        m_statusLabel->setText("Error: Failed to parse AI response.");
        QMessageBox::critical(this, "Parse Error", "The AI returned an invalid response format.");
        return;
    }

    QJsonObject root = doc.object();
    QString subcktCode = root["subcircuit"].toString();
    QJsonArray mapping = root["mapping"].toArray();

    if (subcktCode.isEmpty()) {
        m_statusLabel->setText("Error: AI returned empty subcircuit.");
        return;
    }

    // 1. Set the text
    m_textEdit->setPlainText(subcktCode);
    
    // 2. Trigger parsing (this populates m_pinTable rows)
    updateFromText();

    // 3. Override pin labels from AI mapping
    for (int i = 0; i < mapping.size(); ++i) {
        QJsonObject mapObj = mapping[i].toObject();
        int pinNum = mapObj["num"].toInt();
        QString pinLabel = mapObj["label"].toString();

        // Find the row in m_pinTable where 'Subckt Pin' matches
        // Actually, AI mapping num usually matches the order in .subckt line
        // But let's be safe and match by row if possible, or just use the index if num aligns
        if (i < m_pinTable->rowCount()) {
            QTableWidgetItem* labelItem = m_pinTable->item(i, 2);
            if (labelItem) {
                labelItem->setText(pinLabel);
            }
        }
    }

    m_statusLabel->setText("Successfully generated '" + root["name"].toString() + "' model and pin mapping.");
}
