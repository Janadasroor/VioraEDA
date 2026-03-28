#include "spice_directive_dialog.h"
#include "../editor/schematic_commands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

SpiceDirectiveDialog::SpiceDirectiveDialog(SchematicSpiceDirectiveItem* item, QUndoStack* undoStack, QGraphicsScene* scene, QWidget* parent)
    : QDialog(parent), m_item(item), m_undoStack(undoStack), m_scene(scene)
{
    setupUi();
    loadFromItem();

    connect(m_okButton, &QPushButton::clicked, this, &SpiceDirectiveDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
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
        "X1 0 inv out vcc vee opamp\n"
        ".subckt opamp 1 2 3 4 5 ... .ends opamp", this);
    mainLayout->addWidget(infoLabel);

    m_commandEdit = new QPlainTextEdit(this);
    m_commandEdit->setPlaceholderText("Vcc vcc 0 DC 15\nVee vee 0 DC -15\nVin in 0 SIN(0 1 1k)\n\nR1 in inv 10k\nR2 out inv 100k\n\nX1 0 inv out vcc vee opamp\n.subckt opamp 1 2 3 4 5\nE1 3 0 1 2 100k\nR1 3 0 1k\n.ends opamp\n\n.tran 10u 10m");
    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    m_commandEdit->setFont(font);
    mainLayout->addWidget(m_commandEdit);

    QHBoxLayout* btnLayout = new QHBoxLayout();
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
