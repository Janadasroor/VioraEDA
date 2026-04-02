#ifndef CURRENTSOURCE_LTSPICE_DIALOG_H
#define CURRENTSOURCE_LTSPICE_DIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QUndoStack>
#include <QGraphicsScene>
#include "../items/current_source_item.h"

class CurrentSourceLTSpiceDialog : public QDialog {
    Q_OBJECT

public:
    CurrentSourceLTSpiceDialog(CurrentSourceItem* item, QUndoStack* undoStack = nullptr, QGraphicsScene* scene = nullptr, const QString& projectDir = QString(), QWidget* parent = nullptr);

private Q_SLOTS:
    void onFunctionChanged();
    void onAccepted();
    void onPwlBrowse();
    void onCustomDraw();

private:
    void setupUi();
    void loadFromItem();
    void saveToItem();

    CurrentSourceItem* m_item;
    QUndoStack* m_undoStack;
    QGraphicsScene* m_scene;
    QString m_projectDir;

    QRadioButton* m_noneRadio;
    QRadioButton* m_pulseRadio;
    QRadioButton* m_sineRadio;
    QRadioButton* m_expRadio;
    QRadioButton* m_sffmRadio;
    QRadioButton* m_pwlRadio;
    QRadioButton* m_customRadio;
    QRadioButton* m_pwlFileRadio;
    QRadioButton* m_behavioralRadio;

    QStackedWidget* m_paramStack;

    QLineEdit* m_pulseI1;
    QLineEdit* m_pulseI2;
    QLineEdit* m_pulseTd;
    QLineEdit* m_pulseTr;
    QLineEdit* m_pulseTf;
    QLineEdit* m_pulseTon;
    QLineEdit* m_pulseTperiod;
    QLineEdit* m_pulseNcycles;

    QLineEdit* m_sineIoffset;
    QLineEdit* m_sineIamp;
    QLineEdit* m_sineFreq;
    QLineEdit* m_sineTd;
    QLineEdit* m_sineTheta;
    QLineEdit* m_sinePhi;
    QLineEdit* m_sineNcycles;

    QLineEdit* m_expI1;
    QLineEdit* m_expI2;
    QLineEdit* m_expTd1;
    QLineEdit* m_expTau1;
    QLineEdit* m_expTd2;
    QLineEdit* m_expTau2;

    QLineEdit* m_sffmIoff;
    QLineEdit* m_sffmIamp;
    QLineEdit* m_sffmFcar;
    QLineEdit* m_sffmMdi;
    QLineEdit* m_sffmFsig;

    QLineEdit* m_pwlPoints;
    QPushButton* m_pwlDrawBtn;
    QCheckBox* m_pwlRepeat;

    QLineEdit* m_pwlFile;

    QPlainTextEdit* m_behavioralExpr;
    QLabel* m_behavioralStatus;
    QLabel* m_behavioralPreview;
    QDoubleSpinBox* m_previewValue;
    QDoubleSpinBox* m_previewTime;
    QCompleter* m_behavioralCompleter;

    bool eventFilter(QObject* obj, QEvent* event) override;

    QLineEdit* m_dcValue;
    QCheckBox* m_dcVisible;

    QLineEdit* m_acAmplitude;
    QLineEdit* m_acPhase;
    QCheckBox* m_acVisible;

    QLineEdit* m_seriesRes;
    QLineEdit* m_parallelCap;
    QCheckBox* m_parasiticVisible;

    QCheckBox* m_functionVisible;
};

#endif
