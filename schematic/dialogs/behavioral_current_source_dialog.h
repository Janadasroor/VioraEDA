#ifndef BEHAVIORAL_CURRENT_SOURCE_DIALOG_H
#define BEHAVIORAL_CURRENT_SOURCE_DIALOG_H

#include <QDialog>
#include <QPointer>

class QPlainTextEdit;
class QLabel;
class QDoubleSpinBox;
class QPushButton;
class QCompleter;
class BehavioralCurrentSourceItem;
class QGraphicsScene;

class BehavioralCurrentSourceDialog : public QDialog {
    Q_OBJECT
public:
    BehavioralCurrentSourceDialog(BehavioralCurrentSourceItem* item, QGraphicsScene* scene, QWidget* parent = nullptr);

private:
    void applyChanges();
    void updateUi();
    bool eventFilter(QObject* obj, QEvent* event) override;

    QPointer<BehavioralCurrentSourceItem> m_item;
    QGraphicsScene* m_scene;

    QPlainTextEdit* m_expr = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_preview = nullptr;
    QDoubleSpinBox* m_previewValue = nullptr;
    QDoubleSpinBox* m_previewTime = nullptr;
    QPushButton* m_insertNodeBtn = nullptr;
    QCompleter* m_completer = nullptr;
};

#endif // BEHAVIORAL_CURRENT_SOURCE_DIALOG_H
