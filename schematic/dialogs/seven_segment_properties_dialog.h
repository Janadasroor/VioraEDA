#ifndef SEVEN_SEGMENT_PROPERTIES_DIALOG_H
#define SEVEN_SEGMENT_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QPointer>
#include <QColor>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class SevenSegmentDisplayItem;

class SevenSegmentPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit SevenSegmentPropertiesDialog(SevenSegmentDisplayItem* item, QWidget* parent = nullptr);

private:
    void applyChanges();
    void updateColorButton(QPushButton* button, const QColor& color, const QString& fallbackText);

    QPointer<SevenSegmentDisplayItem> m_item;
    QComboBox* m_variant = nullptr;
    QComboBox* m_commonType = nullptr;
    QDoubleSpinBox* m_threshold = nullptr;
    QDoubleSpinBox* m_glowStrength = nullptr;
    QSpinBox* m_fitScale = nullptr;
    QPushButton* m_segmentOnColorButton = nullptr;
    QPushButton* m_segmentOffColorButton = nullptr;
    QPushButton* m_bodyColorButton = nullptr;
    QPushButton* m_bezelColorButton = nullptr;
    QColor m_segmentOnColor;
    QColor m_segmentOffColor;
    QColor m_bodyColor;
    QColor m_bezelColor;
};

#endif // SEVEN_SEGMENT_PROPERTIES_DIALOG_H
