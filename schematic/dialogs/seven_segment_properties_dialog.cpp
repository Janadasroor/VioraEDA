#include "seven_segment_properties_dialog.h"

#include "../items/seven_segment_display_item.h"
#include "theme_manager.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>

SevenSegmentPropertiesDialog::SevenSegmentPropertiesDialog(SevenSegmentDisplayItem* item, QWidget* parent)
    : QDialog(parent), m_item(item) {
    setWindowTitle("Segment Display Properties");
    setModal(true);
    resize(380, 210);

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
    }

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
    m_item->update();
}
