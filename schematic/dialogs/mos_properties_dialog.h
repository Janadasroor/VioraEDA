#ifndef MOS_PROPERTIES_DIALOG_H
#define MOS_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <QPointer>

class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QScrollArea;
class QVBoxLayout;
class SchematicItem;

struct MosParamDef {
    QString name;
    QString defaultVal;
    QString unit;
    QString desc;
};

struct MosParamCategory {
    QString name;
    QVector<MosParamDef> params;
};

struct MosModelDef {
    QString model;
    QString description;
    int level = 1;
    QString spiceType;
    QString spiceLevelParam;
    QVector<MosParamCategory> categories;
};

class MosPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit MosPropertiesDialog(SchematicItem* item, QWidget* parent = nullptr);

    QString modelName() const;
    QString modelLevel() const;
    QString footprint() const;
    QMap<QString, QString> paramExpressions() const;
    QString newSymbolName() const;

private Q_SLOTS:
    void updateCommandPreview();
    void applyChanges();
    void pickFootprint();
    void autoMatchModel();
    void onLevelChanged(int index);

private:
    void setupUI();
    void loadValues();
    void fillFromModel(const QString& modelName);
    void rebuildParamForm(const MosModelDef& def);
    void loadModelDef(const QString& levelName);
    bool isPmosSelected() const;
    bool isPmos() const;

    QPointer<SchematicItem> m_item;

    QLineEdit* m_modelNameEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QComboBox* m_levelCombo = nullptr;
    QPushButton* m_pickModelButton = nullptr;
    QLineEdit* m_footprintEdit = nullptr;
    QLineEdit* m_commandPreview = nullptr;
    QTextEdit* m_rawParamsEdit = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QVBoxLayout* m_paramLayout = nullptr;

    MosModelDef m_currentDef;
    QMap<QString, QLineEdit*> m_paramEdits;
    QVector<QWidget*> m_categoryWidgets;

    // Known model levels
    struct LevelInfo {
        QString name;
        QString jsonFile;
    };
    static const QVector<LevelInfo>& knownLevels();
};

#endif // MOS_PROPERTIES_DIALOG_H