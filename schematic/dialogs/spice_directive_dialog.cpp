#include "spice_directive_dialog.h"
#include "../editor/schematic_commands.h"
#include "../ui/spice_highlighter.h"
#include "../items/inductor_item.h"
#include "../items/power_item.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QSplitter>
#include <QRegularExpression>
#include <QSet>
#include <QTextCursor>

namespace {

QSet<QString> schematicReferences(QGraphicsScene* scene) {
    QSet<QString> refs;
    if (!scene) return refs;

    for (QGraphicsItem* item : scene->items()) {
        auto* schematicItem = dynamic_cast<SchematicItem*>(item);
        if (!schematicItem) continue;
        if (schematicItem->itemType() == SchematicItem::SpiceDirectiveType) continue;

        const QString ref = schematicItem->reference().trimmed().toUpper();
        if (!ref.isEmpty()) refs.insert(ref);
    }

    return refs;
}

QSet<QString> schematicDirectiveModelNames(QGraphicsScene* scene, const SchematicSpiceDirectiveItem* self) {
    QSet<QString> names;
    if (!scene) return names;

    for (QGraphicsItem* item : scene->items()) {
        auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(item);
        if (!directive || directive == self) continue;

        const QStringList lines = directive->text().split('\n');
        for (const QString& rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (!line.startsWith(".model", Qt::CaseInsensitive)) continue;
            const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) names.insert(parts.at(1).toUpper());
        }
    }

    return names;
}

QSet<QString> powerRailNetNames(QGraphicsScene* scene) {
    QSet<QString> rails;
    if (!scene) return rails;

    for (QGraphicsItem* item : scene->items()) {
        auto* schematicItem = dynamic_cast<SchematicItem*>(item);
        if (!schematicItem) continue;
        if (schematicItem->itemType() != SchematicItem::PowerType) continue;

        const QString value = schematicItem->value().trimmed();
        if (!value.isEmpty()) rails.insert(value.toUpper());
        const QString ref = schematicItem->reference().trimmed();
        if (!ref.isEmpty()) rails.insert(ref.toUpper());
    }

    return rails;
}

QStringList inductorReferences(QGraphicsScene* scene, const SchematicSpiceDirectiveItem* self = nullptr) {
    QStringList refs;
    if (!scene) return refs;

    for (QGraphicsItem* item : scene->items()) {
        if (item == self) continue;
        auto* inductor = dynamic_cast<InductorItem*>(item);
        if (!inductor) continue;
        const QString ref = inductor->reference().trimmed().toUpper();
        if (!ref.isEmpty()) refs.append(ref);
    }

    refs.removeDuplicates();
    std::sort(refs.begin(), refs.end());
    return refs;
}

bool isCommentLine(const QString& line) {
    return line.startsWith('*') || line.startsWith(';') || line.startsWith('#');
}

bool isDirectiveLine(const QString& line) {
    return line.startsWith('.');
}

QString firstToken(const QString& line) {
    return line.section(QRegularExpression("\\s+"), 0, 0).trimmed();
}

QStringList collapseSpiceContinuationLines(const QString& text) {
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

QString rewritePreviewLtspiceBehavioralIf(const QString& line, QStringList* fixes = nullptr) {
    QString out = line;
    static const QRegularExpression ifZeroRe(
        "\\bif\\s*\\(\\s*([^,]+?)\\s*([<>]=?)\\s*([^,]+?)\\s*,\\s*([^,]+?)\\s*,\\s*(0|0\\.0*)\\s*\\)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ifElseRe(
        "\\bif\\s*\\(\\s*([^,]+?)\\s*([<>]=?)\\s*([^,]+?)\\s*,\\s*([^,]+?)\\s*,\\s*([^,]+?)\\s*\\)",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = ifZeroRe.match(out);
    if (!match.hasMatch()) {
        match = ifElseRe.match(out);
        if (!match.hasMatch()) return out;
    }

    const QString lhs = match.captured(1).trimmed();
    const QString op = match.captured(2).trimmed();
    const QString rhs = match.captured(3).trimmed();
    const QString trueExpr = match.captured(4).trimmed();
    const QString falseExpr = match.captured(5).trimmed();

    QString stepExpr;
    if (op == ">" || op == ">=") stepExpr = QString("u((%1)-(%2))").arg(lhs, rhs);
    else if (op == "<" || op == "<=") stepExpr = QString("u((%1)-(%2))").arg(rhs, lhs);
    else return out;

    const bool falseIsZero = falseExpr == "0" || falseExpr == "0.0";
    const QString replacement = falseIsZero
        ? QString("((%1)*(%2))").arg(trueExpr, stepExpr)
        : QString("((%1)*(%2) + (%3)*(1-(%2)))").arg(trueExpr, stepExpr, falseExpr);
    out.replace(match.capturedStart(0), match.capturedLength(0), replacement);
    if (fixes) fixes->append(QString("if(...) -> %1").arg(replacement));
    return out;
}

QString rewritePreviewDirectiveLine(const QString& line, QStringList* fixes = nullptr) {
    QString out = line;
    if (out.contains("if(", Qt::CaseInsensitive) && out.contains(" V={", Qt::CaseInsensitive)) {
        out = rewritePreviewLtspiceBehavioralIf(out, fixes);
    }
    if (out.contains(" V={", Qt::CaseInsensitive)) {
        const QString original = out;
        out.replace("&&", " and ");
        out.replace("||", " or ");
        static const QRegularExpression singleAndRe("(?<![&])&(?![&])");
        static const QRegularExpression singleOrRe("(?<![|])\\|(?![|])");
        out.replace(singleAndRe, " and ");
        out.replace(singleOrRe, " or ");
        if (fixes && out != original) fixes->append("boolean operators -> and/or");
    }
    return out;
}

struct MutualCouplingCard {
    QString name;
    QStringList inductors;
    QString coefficient;
};

bool parseMutualCouplingLine(const QString& line, MutualCouplingCard* out = nullptr) {
    const QStringList parts = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 4) return false;
    if (!parts.first().startsWith('K', Qt::CaseInsensitive)) return false;

    MutualCouplingCard parsed;
    parsed.name = parts.first().trimmed();
    parsed.coefficient = parts.last().trimmed();
    for (int i = 1; i < parts.size() - 1; ++i) {
        parsed.inductors.append(parts.at(i).trimmed());
    }
    if (parsed.inductors.size() < 2) return false;

    if (out) *out = parsed;
    return true;
}

QString nextMutualCouplingName(const QString& currentText) {
    QSet<int> usedNumbers;
    const QStringList lines = collapseSpiceContinuationLines(currentText);
    static const QRegularExpression numberedNameRe("^K(\\d+)$", QRegularExpression::CaseInsensitiveOption);

    for (const QString& rawLine : lines) {
        MutualCouplingCard card;
        if (!parseMutualCouplingLine(rawLine, &card)) continue;
        const QRegularExpressionMatch match = numberedNameRe.match(card.name.trimmed());
        if (match.hasMatch()) usedNumbers.insert(match.captured(1).toInt());
    }

    int next = 1;
    while (usedNumbers.contains(next)) ++next;
    return QString("K%1").arg(next);
}

QString replaceOrAppendMutualCouplingLine(const QString& text, const QString& newLine) {
    QStringList lines = text.split('\n');
    for (QString& line : lines) {
        if (parseMutualCouplingLine(line)) {
            line = newLine;
            return lines.join('\n');
        }
    }

    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
        lines.removeLast();
    }
    lines.append(newLine);
    return lines.join('\n');
}

}

SpiceDirectiveDialog::SpiceDirectiveDialog(SchematicSpiceDirectiveItem* item, QUndoStack* undoStack, QGraphicsScene* scene, QWidget* parent)
    : QDialog(parent), m_item(item), m_undoStack(undoStack), m_scene(scene), m_commandEdit(nullptr), m_validationLabel(nullptr), m_previewEdit(nullptr), m_applyFixesButton(nullptr), m_mutualCouplingButton(nullptr), m_okButton(nullptr), m_cancelButton(nullptr), m_highlighter(nullptr)
{
    setupUi();
    loadFromItem();

    connect(m_okButton, &QPushButton::clicked, this, &SpiceDirectiveDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyFixesButton, &QPushButton::clicked, this, &SpiceDirectiveDialog::applyCompatibilityFixes);
    connect(m_mutualCouplingButton, &QPushButton::clicked, this, &SpiceDirectiveDialog::openMutualCouplingBuilder);
    connect(m_commandEdit, &QPlainTextEdit::textChanged, this, &SpiceDirectiveDialog::validateDirectiveText);
    connect(m_commandEdit, &QPlainTextEdit::textChanged, this, &SpiceDirectiveDialog::updatePreview);

    validateDirectiveText();
    updatePreview();
}

void SpiceDirectiveDialog::setupUi() {
    setWindowTitle("Edit SPICE Directive");
    resize(500, 300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* infoLabel = new QLabel("Enter SPICE commands or LTspice-style netlist blocks:\n"
        ".tran .ac .op .dc .noise .four .tf .disto .sens\n"
        ".model .param .subckt .ends .include .lib .endl\n"
        ".func .global .ic .nodeset .options .temp .step .meas .print\n\n"
        "You can also paste component and subcircuit lines such as:\n"
        "Vcc vcc 0 DC 15\n"
        "K1 L1 L2 0.99\n"
        "X1 0 inv out vcc vee opamp\n"
        ".subckt opamp 1 2 3 4 5 ... .ends opamp", this);
    mainLayout->addWidget(infoLabel);

    QSplitter* splitter = new QSplitter(Qt::Vertical, this);

    m_commandEdit = new QPlainTextEdit(splitter);
    m_commandEdit->setPlaceholderText("Vcc vcc 0 DC 15\nVee vee 0 DC -15\nVin in 0 SIN(0 1 1k)\n\nR1 in inv 10k\nR2 out inv 100k\n\nX1 0 inv out vcc vee opamp\n.subckt opamp 1 2 3 4 5\nE1 3 0 1 2 100k\nR1 3 0 1k\n.ends opamp\n\n.tran 10u 10m");
    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    m_commandEdit->setFont(font);
    m_highlighter = new SpiceHighlighter(m_commandEdit->document());

    m_previewEdit = new QTextBrowser(splitter);
    m_previewEdit->setOpenLinks(false);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setFont(font);
    m_previewEdit->setPlaceholderText("Directive preview will appear here.");

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLayout->addWidget(splitter);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setTextFormat(Qt::PlainText);
    mainLayout->addWidget(m_validationLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_applyFixesButton = new QPushButton("Apply Compatibility Fixes", this);
    btnLayout->addWidget(m_applyFixesButton);
    m_mutualCouplingButton = new QPushButton("Mutual Coupling...", this);
    btnLayout->addWidget(m_mutualCouplingButton);
    btnLayout->addStretch();

    m_cancelButton = new QPushButton("Cancel", this);
    m_okButton = new QPushButton("OK", this);
    m_okButton->setDefault(true);

    btnLayout->addWidget(m_cancelButton);
    btnLayout->addWidget(m_okButton);

    mainLayout->addLayout(btnLayout);
    
    // Apply styling if a theme is available would be nice, but QDialog usually inherits
}

void SpiceDirectiveDialog::loadFromItem() {
    if (m_item) {
        m_commandEdit->setPlainText(m_item->text());
    }
}

void SpiceDirectiveDialog::onAccepted() {
    saveToItem();
    accept();
}

void SpiceDirectiveDialog::applyCompatibilityFixes() {
    if (!m_commandEdit) return;

    const QString originalText = m_commandEdit->toPlainText();
    const QStringList lines = collapseSpiceContinuationLines(originalText);
    QStringList rewrittenLines;
    bool changed = false;

    for (const QString& rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) {
            rewrittenLines << QString();
            continue;
        }
        if (isCommentLine(trimmed)) {
            rewrittenLines << rawLine;
            continue;
        }

        const QString rewritten = rewritePreviewDirectiveLine(trimmed);
        rewrittenLines << rewritten;
        changed = changed || (rewritten != trimmed);
    }

    if (changed) {
        m_commandEdit->setPlainText(rewrittenLines.join('\n'));
    }
}

void SpiceDirectiveDialog::openMutualCouplingBuilder() {
    const QStringList inductors = inductorReferences(m_scene, m_item);
    if (inductors.size() < 2) {
        QMessageBox::warning(this, "Mutual Coupling",
                             "At least two schematic inductors with reference designators are required.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Mutual Inductor Coupling");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    QLabel* info = new QLabel(
        "Generate an LTspice/ngspice mutual coupling line.\n"
        "Example: K1 L1 L2 L3 0.99", &dlg);
    info->setWordWrap(true);
    layout->addWidget(info);

    QFormLayout* form = new QFormLayout();
    QLineEdit* nameEdit = new QLineEdit(nextMutualCouplingName(m_commandEdit ? m_commandEdit->toPlainText() : QString()), &dlg);
    QListWidget* inductorList = new QListWidget(&dlg);
    inductorList->setSelectionMode(QAbstractItemView::MultiSelection);
    inductorList->setMinimumHeight(120);
    for (const QString& ref : inductors) {
        auto* item = new QListWidgetItem(ref, inductorList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    if (inductorList->count() > 0) inductorList->item(0)->setCheckState(Qt::Checked);
    if (inductorList->count() > 1) inductorList->item(1)->setCheckState(Qt::Checked);

    QDoubleSpinBox* coeffSpin = new QDoubleSpinBox(&dlg);
    coeffSpin->setDecimals(4);
    coeffSpin->setRange(-1.0, 1.0);
    coeffSpin->setSingleStep(0.01);
    coeffSpin->setValue(0.99);

    MutualCouplingCard parsedCard;
    const QStringList existingLines = collapseSpiceContinuationLines(m_commandEdit ? m_commandEdit->toPlainText() : QString());
    for (const QString& rawLine : existingLines) {
        if (!parseMutualCouplingLine(rawLine, &parsedCard)) continue;
        nameEdit->setText(parsedCard.name);
        const QSet<QString> selectedRefs(parsedCard.inductors.begin(), parsedCard.inductors.end());
        for (int i = 0; i < inductorList->count(); ++i) {
            auto* item = inductorList->item(i);
            item->setCheckState(selectedRefs.contains(item->text().trimmed().toUpper()) ? Qt::Checked : Qt::Unchecked);
        }
        bool coeffOk = false;
        const double coeff = parsedCard.coefficient.toDouble(&coeffOk);
        if (coeffOk && coeff >= -1.0 && coeff <= 1.0) coeffSpin->setValue(coeff);
        break;
    }

    form->addRow("Name", nameEdit);
    form->addRow("Inductors", inductorList);
    form->addRow("Coupling", coeffSpin);
    layout->addLayout(form);

    QLabel* hint = new QLabel("The generated line will replace the first existing K-card in this block, or be appended if none exists.", &dlg);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted || !m_commandEdit) return;

    const QString name = nameEdit->text().trimmed().toUpper();
    QStringList selectedInductors;
    for (int i = 0; i < inductorList->count(); ++i) {
        auto* item = inductorList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            selectedInductors.append(item->text().trimmed().toUpper());
        }
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Mutual Coupling", "A coupling name is required.");
        return;
    }
    selectedInductors.removeDuplicates();
    if (selectedInductors.size() < 2) {
        QMessageBox::warning(this, "Mutual Coupling", "Select at least two different inductors.");
        return;
    }

    const QString coefficient =
        QString::number(coeffSpin->value(), 'f', coeffSpin->decimals()).remove(QRegularExpression("0+$")).remove(QRegularExpression("\\.$"));
    const QString couplingLine = QString("%1 %2 %3")
        .arg(name, selectedInductors.join(' '), coefficient);
    m_commandEdit->setPlainText(replaceOrAppendMutualCouplingLine(m_commandEdit->toPlainText(), couplingLine));
}

void SpiceDirectiveDialog::saveToItem() {
    if (!m_item) return;

    QString newText = m_commandEdit->toPlainText().trimmed();
    QString oldText = m_item->text();

    if (newText != oldText) {
        if (m_undoStack && m_scene) {
            m_undoStack->push(new ChangePropertyCommand(m_scene, m_item, "Text", oldText, newText));
        } else {
            m_item->setText(newText);
            m_item->update();
        }
    }
}

void SpiceDirectiveDialog::validateDirectiveText() {
    if (!m_commandEdit || !m_validationLabel || !m_okButton) return;

    const QString text = m_commandEdit->toPlainText();
    const QStringList lines = collapseSpiceContinuationLines(text);
    const QSet<QString> sceneRefs = schematicReferences(m_scene);
    const QStringList inductorRefList = inductorReferences(m_scene, m_item);
    QSet<QString> inductorRefs;
    for (const QString& ref : inductorRefList) inductorRefs.insert(ref);
    const QSet<QString> existingModelNames = schematicDirectiveModelNames(m_scene, m_item);
    const QSet<QString> powerRails = powerRailNetNames(m_scene);

    QStringList errors;
    QStringList warnings;
    QStringList analysisCards;
    QMap<QString, int> elementLineByRef;
    QMap<QString, int> modelLineByName;
    QStringList subcktStack;

    for (int i = 0; i < lines.size(); ++i) {
        const QString rawLine = lines.at(i);
        const QString line = rawLine.trimmed();
        const int lineNo = i + 1;

        if (line.isEmpty() || isCommentLine(line)) continue;

        if (isDirectiveLine(line)) {
            const QString card = firstToken(line).toLower();

            if (card == ".subckt") {
                const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() < 2) {
                    errors << QString("Line %1: .subckt is missing a subcircuit name.").arg(lineNo);
                } else {
                    subcktStack.append(parts.at(1));
                }
            } else if (card == ".ends") {
                const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                const QString endName = parts.size() >= 2 ? parts.at(1) : QString();
                if (subcktStack.isEmpty()) {
                    errors << QString("Line %1: .ends has no matching .subckt.").arg(lineNo);
                } else {
                    const QString openName = subcktStack.takeLast();
                    if (!endName.isEmpty() && endName.compare(openName, Qt::CaseInsensitive) != 0) {
                        errors << QString("Line %1: .ends %2 does not match open .subckt %3.")
                                      .arg(lineNo)
                                      .arg(endName, openName);
                    }
                }
            } else if (card == ".model") {
                const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() < 2) {
                    errors << QString("Line %1: .model is missing a model name.").arg(lineNo);
                } else {
                    const QString modelName = parts.at(1).toUpper();
                    if (modelLineByName.contains(modelName)) {
                        warnings << QString("Line %1: duplicate .model %2 (first seen on line %3).")
                                        .arg(lineNo)
                                        .arg(parts.at(1))
                                        .arg(modelLineByName.value(modelName));
                    } else {
                        modelLineByName.insert(modelName, lineNo);
                    }
                    if (existingModelNames.contains(modelName)) {
                        warnings << QString("Line %1: .model %2 duplicates a model declared in another directive block.")
                                        .arg(lineNo)
                                        .arg(parts.at(1));
                    }
                }

                if (line.contains(" D(", Qt::CaseInsensitive) &&
                    (line.contains("Ron=", Qt::CaseInsensitive) || line.contains("Roff=", Qt::CaseInsensitive) || line.contains("Vfwd=", Qt::CaseInsensitive))) {
                    warnings << QString("Line %1: LTspice-style diode model parameters Ron/Roff/Vfwd may fail in ngspice.").arg(lineNo);
                }
            }

            static const QSet<QString> kAnalysisCards = {
                ".tran", ".ac", ".op", ".dc", ".noise", ".four", ".tf", ".disto", ".sens"
            };
            if (kAnalysisCards.contains(card)) {
                analysisCards.append(QString("%1 (line %2)").arg(card, QString::number(lineNo)));
            }

            if (card == ".meas" && line.contains("I(", Qt::CaseInsensitive)) {
                warnings << QString("Line %1: I(R...) style .meas current expressions may not be portable to ngspice.").arg(lineNo);
            }
            if (card == ".meas" && line.contains(" PARAM ", Qt::CaseInsensitive)) {
                warnings << QString("Line %1: .meas PARAM was kept as-is; verify LTspice/ngspice compatibility.").arg(lineNo);
            }
            if (card == ".meas" && line.contains(" FIND ", Qt::CaseInsensitive) && line.contains(" AT=", Qt::CaseInsensitive)) {
                warnings << QString("Line %1: .meas FIND ... AT= may differ between LTspice and ngspice.").arg(lineNo);
            }
            if ((card == ".meas" || card == ".func" || card == ".param") && line.contains("table(", Qt::CaseInsensitive)) {
                warnings << QString("Line %1: table(...) may behave differently between LTspice and ngspice.").arg(lineNo);
            }

            continue;
        }

        if (line.contains("if(", Qt::CaseInsensitive)) {
            warnings << QString("Line %1: LTspice-style if(...) may fail in ngspice; prefer u(...) or let VioSpice rewrite simple cases during netlist generation.").arg(lineNo);
        }
        if ((line.contains("&&") || line.contains("||") || line.contains('&') || line.contains('|')) && line.contains("V={", Qt::CaseInsensitive)) {
            warnings << QString("Line %1: LTspice-style boolean operators may need compatibility rewriting for ngspice.").arg(lineNo);
        }
        if (line.contains("table(", Qt::CaseInsensitive)) {
            warnings << QString("Line %1: table(...) expression detected; verify ngspice compatibility.").arg(lineNo);
        }

        const QString token = firstToken(line);
        if (token.isEmpty()) continue;

        const QString normalizedRef = token.toUpper();
        if (elementLineByRef.contains(normalizedRef)) {
            warnings << QString("Line %1: duplicate element reference %2 (first seen on line %3).").arg(lineNo).arg(token).arg(elementLineByRef.value(normalizedRef));
        } else {
            elementLineByRef.insert(normalizedRef, lineNo);
        }

        if (sceneRefs.contains(normalizedRef)) {
            warnings << QString("Line %1: element reference %2 conflicts with an existing schematic reference.").arg(lineNo).arg(token);
        }

        const QChar prefix = normalizedRef.isEmpty() ? QChar() : normalizedRef.at(0);
        if (prefix == 'K') {
            MutualCouplingCard card;
            if (!parseMutualCouplingLine(line, &card)) {
                errors << QString("Line %1: invalid mutual coupling card. Expected 'Kname L1 L2 [L3 ...] 0.99'.").arg(lineNo);
                continue;
            }

            QSet<QString> seenInductors;
            for (const QString& refText : card.inductors) {
                const QString ref = refText.trimmed().toUpper();
                if (!ref.startsWith('L')) {
                    errors << QString("Line %1: mutual coupling must reference inductors (L...).").arg(lineNo);
                    continue;
                }
                if (seenInductors.contains(ref)) {
                    errors << QString("Line %1: mutual coupling must reference different inductors.").arg(lineNo);
                    continue;
                }
                seenInductors.insert(ref);
                if (!inductorRefs.isEmpty() && !inductorRefs.contains(ref)) {
                    warnings << QString("Line %1: inductor %2 was not found on the current schematic.").arg(lineNo).arg(refText);
                }
            }
            if (seenInductors.size() < 2) {
                errors << QString("Line %1: mutual coupling must reference at least two different inductors.").arg(lineNo);
            }

            bool coeffOk = false;
            const double coeff = card.coefficient.toDouble(&coeffOk);
            if (!coeffOk) {
                errors << QString("Line %1: coupling coefficient '%2' is not numeric.").arg(lineNo).arg(card.coefficient);
            } else if (coeff < -1.0 || coeff > 1.0) {
                errors << QString("Line %1: coupling coefficient must be between -1 and 1.").arg(lineNo);
            }
            continue;
        }

        if ((prefix == 'V' || prefix == 'I') && !powerRails.isEmpty()) {
            const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                const QString netName = parts.at(1).trimmed().toUpper();
                if (powerRails.contains(netName)) {
                    warnings << QString("Line %1: source %2 drives rail %3, which already exists as a schematic power rail.")
                                    .arg(lineNo)
                                    .arg(token)
                                    .arg(parts.at(1));
                }
            }
        }
    }

    if (!subcktStack.isEmpty()) {
        errors << QString("Missing .ends for subcircuit %1.").arg(subcktStack.last());
    }

    if (analysisCards.size() > 1) {
        warnings << QString("Multiple analysis cards in one directive block: %1.").arg(analysisCards.join(", "));
    }

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
        m_validationLabel->setStyleSheet("color: #15803d;");
        m_validationLabel->setText("Validation: no obvious directive block problems found.");
    } else if (!errors.isEmpty()) {
        m_validationLabel->setStyleSheet("color: #b91c1c;");
        m_validationLabel->setText(messages.join('\n'));
    } else {
        m_validationLabel->setStyleSheet("color: #b45309;");
        m_validationLabel->setText(messages.join('\n'));
    }

    m_okButton->setEnabled(errors.isEmpty());
}

void SpiceDirectiveDialog::updatePreview() {
    if (!m_commandEdit || !m_previewEdit) return;

    const QString text = m_commandEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_previewEdit->setPlainText("Directive preview will appear here.");
        return;
    }

    QStringList preview;
    preview << "* SPICE block preview";
    QStringList compatibilityFixes;
    QStringList compatibilityDiff;

    const QStringList lines = collapseSpiceContinuationLines(text);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            preview << QString();
            continue;
        }
        if (isCommentLine(line)) {
            preview << line;
            continue;
        }
        QStringList lineFixes;
        const QString rewritten = rewritePreviewDirectiveLine(line, &lineFixes);
        if (rewritten != line) {
            preview << "* compatibility fix: " + line;
            preview << rewritten;
            compatibilityDiff << QString("- %1").arg(line);
            compatibilityDiff << QString("+ %1").arg(rewritten);
            for (const QString& fix : lineFixes) {
                compatibilityFixes << QString("- %1: %2").arg(line, fix);
            }
        } else {
            preview << line;
        }
    }

    if (!compatibilityFixes.isEmpty()) {
        preview << QString();
        preview << "* Compatibility fixes applied in preview";
        preview << compatibilityFixes;
        preview << QString();
        preview << "* Compatibility diff preview";
        preview << compatibilityDiff;
    }

    m_previewEdit->setPlainText(preview.join('\n'));
    QTextCursor cursor = m_previewEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_previewEdit->setTextCursor(cursor);
}
