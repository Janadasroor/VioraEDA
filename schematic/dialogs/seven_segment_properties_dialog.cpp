#include "seven_segment_properties_dialog.h"

#include "../items/seven_segment_display_item.h"
#include "theme_manager.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SevenSegmentPropertiesDialog::SevenSegmentPropertiesDialog(SevenSegmentDisplayItem* item, QWidget* parent)
    : QDialog(parent), m_item(item) {
    setWindowTitle("Segment Display Properties");
    setModal(true);
    resize(420, 340);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_variant = new QComboBox();
    m_variant->addItem("7-Segment");
    m_variant->addItem("Dual 7-Segment");
    m_variant->addItem("14-Segment");
    m_variant->addItem("16-Segment");
    form->addRow("Variant:", m_variant);

    m_commonType = new QComboBox();
    m_commonType->addItem("Common Cathode");
    m_commonType->addItem("Common Anode");
    form->addRow("Common Type:", m_commonType);

    m_threshold = new QDoubleSpinBox();
    m_threshold->setRange(0.0, 20.0);
    m_threshold->setDecimals(2);
    m_threshold->setSingleStep(0.1);
    m_threshold->setSuffix(" V");
    form->addRow("Segment Threshold:", m_threshold);

    m_segmentOnColorButton = new QPushButton();
    m_segmentOnColorButton->setMinimumWidth(140);
    form->addRow("On Color:", m_segmentOnColorButton);
    connect(m_segmentOnColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_segmentOnColor, this, "Select Segment On Color");
        if (!chosen.isValid()) return;
        m_segmentOnColor = chosen;
        updateColorButton(m_segmentOnColorButton, m_segmentOnColor, "ON");
    });

    m_segmentOffColorButton = new QPushButton();
    m_segmentOffColorButton->setMinimumWidth(140);
    form->addRow("Off Color:", m_segmentOffColorButton);
    connect(m_segmentOffColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_segmentOffColor, this, "Select Segment Off Color");
        if (!chosen.isValid()) return;
        m_segmentOffColor = chosen;
        updateColorButton(m_segmentOffColorButton, m_segmentOffColor, "OFF");
    });

    m_bodyColorButton = new QPushButton();
    m_bodyColorButton->setMinimumWidth(140);
    form->addRow("Body Color:", m_bodyColorButton);
    connect(m_bodyColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_bodyColor, this, "Select Body Color");
        if (!chosen.isValid()) return;
        m_bodyColor = chosen;
        updateColorButton(m_bodyColorButton, m_bodyColor, "BODY");
    });

    m_bezelColorButton = new QPushButton();
    m_bezelColorButton->setMinimumWidth(140);
    form->addRow("Bezel Color:", m_bezelColorButton);
    connect(m_bezelColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_bezelColor, this, "Select Bezel Color");
        if (!chosen.isValid()) return;
        m_bezelColor = chosen;
        updateColorButton(m_bezelColorButton, m_bezelColor, "BEZEL");
    });

    m_glowStrength = new QDoubleSpinBox();
    m_glowStrength->setRange(0.0, 2.0);
    m_glowStrength->setDecimals(2);
    m_glowStrength->setSingleStep(0.1);
    form->addRow("Glow Strength:", m_glowStrength);

    m_fitScale = new QSpinBox();
    m_fitScale->setRange(70, 125);
    m_fitScale->setSuffix(" %");
    form->addRow("Fit Scale:", m_fitScale);

    if (m_item) {
        switch (m_item->variant()) {
        case SevenSegmentDisplayItem::Variant::Dual7: m_variant->setCurrentIndex(1); break;
        case SevenSegmentDisplayItem::Variant::Seg14: m_variant->setCurrentIndex(2); break;
        case SevenSegmentDisplayItem::Variant::Seg16: m_variant->setCurrentIndex(3); break;
        case SevenSegmentDisplayItem::Variant::Single7:
        default: m_variant->setCurrentIndex(0); break;
        }
        m_commonType->setCurrentIndex(
            m_item->commonType() == SevenSegmentDisplayItem::CommonType::CommonAnode ? 1 : 0);
        m_threshold->setValue(m_item->thresholdVoltage());
        m_segmentOnColor = m_item->segmentOnColor();
        m_segmentOffColor = m_item->segmentOffColor();
        m_bodyColor = m_item->bodyColor();
        m_bezelColor = m_item->bezelColor();
        m_glowStrength->setValue(m_item->glowStrength());
        m_fitScale->setValue(qRound(m_item->fitScale() * 100.0));
    }
    if (!m_segmentOnColor.isValid()) m_segmentOnColor = QColor(255, 72, 60);
    if (!m_segmentOffColor.isValid()) m_segmentOffColor = QColor(70, 28, 28);
    if (!m_bodyColor.isValid()) m_bodyColor = QColor("#2a2f3a");
    if (!m_bezelColor.isValid()) m_bezelColor = QColor("#101319");
    updateColorButton(m_segmentOnColorButton, m_segmentOnColor, "ON");
    updateColorButton(m_segmentOffColorButton, m_segmentOffColor, "OFF");
    updateColorButton(m_bodyColorButton, m_bodyColor, "BODY");
    updateColorButton(m_bezelColorButton, m_bezelColor, "BEZEL");

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyChanges();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (ThemeManager::theme()) {
        setStyleSheet(ThemeManager::theme()->widgetStylesheet());
    }
}

void SevenSegmentPropertiesDialog::applyChanges() {
    if (!m_item) return;

    switch (m_variant->currentIndex()) {
    case 1: m_item->setVariant(SevenSegmentDisplayItem::Variant::Dual7); break;
    case 2: m_item->setVariant(SevenSegmentDisplayItem::Variant::Seg14); break;
    case 3: m_item->setVariant(SevenSegmentDisplayItem::Variant::Seg16); break;
    case 0:
    default: m_item->setVariant(SevenSegmentDisplayItem::Variant::Single7); break;
    }

    m_item->setCommonType(
        m_commonType->currentIndex() == 1
            ? SevenSegmentDisplayItem::CommonType::CommonAnode
            : SevenSegmentDisplayItem::CommonType::CommonCathode);
    m_item->setThresholdVoltage(m_threshold->value());
    m_item->setSegmentOnColor(m_segmentOnColor);
    m_item->setSegmentOffColor(m_segmentOffColor);
    m_item->setBodyColor(m_bodyColor);
    m_item->setBezelColor(m_bezelColor);
    m_item->setGlowStrength(m_glowStrength->value());
    m_item->setFitScale(m_fitScale->value() / 100.0);
    m_item->update();
}

void SevenSegmentPropertiesDialog::updateColorButton(QPushButton* button, const QColor& color, const QString& fallbackText) {
    if (!button) return;
    const QColor c = color.isValid() ? color : QColor(255, 72, 60);
    button->setText(QString("%1  %2").arg(fallbackText, c.name().toUpper()));
    button->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2; border:1px solid #555; padding:6px 10px; border-radius:4px; }")
        .arg(c.name(), c.lightness() < 140 ? "#ffffff" : "#111111"));
}
