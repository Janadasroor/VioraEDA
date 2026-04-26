#include "tuning_slider_properties_dialog.h"
#include "../items/tuning_slider_symbol_item.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>

TuningSliderPropertiesDialog::TuningSliderPropertiesDialog(TuningSliderSymbolItem* item, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Slider Properties");
    resize(500, 350);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    m_refEdit = new QLineEdit(item->reference());
    
    m_minSpin = new QDoubleSpinBox();
    m_minSpin->setRange(-1e15, 1e15);
    m_minSpin->setValue(item->minValue());

    m_maxSpin = new QDoubleSpinBox();
    m_maxSpin->setRange(-1e15, 1e15);
    m_maxSpin->setValue(item->maxValue());

    m_currentSpin = new QDoubleSpinBox();
    m_currentSpin->setRange(-1e15, 1e15);
    m_currentSpin->setValue(item->currentValue());

    form->addRow("Parameter Name:", m_refEdit);
    form->addRow("Min Value:", m_minSpin);
    form->addRow("Max Value:", m_maxSpin);
    form->addRow("Current Value:", m_currentSpin);

    // Flux Integration
    m_fluxVarEdit = new QLineEdit(item->fluxVariableName());
    m_fluxVarEdit->setPlaceholderText("e.g., target_freq");
    form->addRow("Flux Variable:", m_fluxVarEdit);

    QHBoxLayout* scriptLayout = new QHBoxLayout();
    m_scriptPathEdit = new QLineEdit(item->reactiveScript());
    QPushButton* browseBtn = new QPushButton("...");
    browseBtn->setFixedWidth(30);
    scriptLayout->addWidget(m_scriptPathEdit);
    scriptLayout->addWidget(browseBtn);
    form->addRow("Reactive Script:", scriptLayout);

    mainLayout->addLayout(form);

    auto* note = new QLabel("Reactive scripts allow one slider to control multiple components.");
    note->setStyleSheet("color: #888; font-style: italic;");
    mainLayout->addWidget(note);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(bbox);

    connect(browseBtn, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Reactive Script", "", "FluxScript (*.flux)");
        if (!path.isEmpty()) m_scriptPathEdit->setText(path);
    });
}
