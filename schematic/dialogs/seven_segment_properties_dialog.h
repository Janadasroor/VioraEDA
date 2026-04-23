#ifndef SEVEN_SEGMENT_PROPERTIES_DIALOG_H
#define SEVEN_SEGMENT_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QPointer>

class QComboBox;
class QDoubleSpinBox;
class SevenSegmentDisplayItem;

class SevenSegmentPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit SevenSegmentPropertiesDialog(SevenSegmentDisplayItem* item, QWidget* parent = nullptr);

private:
    void applyChanges();

    QPointer<SevenSegmentDisplayItem> m_item;
    QComboBox* m_variant = nullptr;
    QComboBox* m_commonType = nullptr;
    QDoubleSpinBox* m_threshold = nullptr;
};

#endif // SEVEN_SEGMENT_PROPERTIES_DIALOG_H
