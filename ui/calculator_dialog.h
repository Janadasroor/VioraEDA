#ifndef CALCULATOR_DIALOG_H
#define CALCULATOR_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSpinBox>

class CalculatorDialog : public QDialog {
    Q_OBJECT

public:
    explicit CalculatorDialog(QWidget* parent = nullptr);
    ~CalculatorDialog();

private:
    void setupUI();
    void applyStyle();
    
    // UI Helpers
    QLabel* makeHeader(const QString& text);
    QLabel* makeResultBox(const QString& text = "—");
    
    // Tabs
    QWidget* createTraceWidthTab();
    QWidget* createImpedanceTab();
    QWidget* createVoltageTab();
    QWidget* createViaTab();
    QWidget* createUnitConverterTab();
    QWidget* createSMDTab();
    QWidget* createOhmsLawTab();
    QWidget* createTimeConstTab();
    QWidget* createFilterTab();
    QWidget* createPrefixTab();
    QWidget* createPowerTab();
    
    // Calculations
    void calcTraceWidth();
    void calcImpedance();
    void calcVia();
    void calcUnits();
    
    // Widgets References (for real-time updates)
    // Trace Tab
    QDoubleSpinBox *m_traceI, *m_traceOz, *m_traceDT, *m_traceTa;
    QComboBox *m_traceLayer;
    QLabel *m_traceRes;
    
    // Impedance Tab
    QComboBox *m_impMode;
    QDoubleSpinBox *m_impW, *m_impH, *m_impT, *m_impEr, *m_impG;
    QLabel *m_impRes;
    
    // Units Tab
    QDoubleSpinBox *m_unitVal;
    QComboBox *m_unitFrom, *m_unitTo;
    QLabel *m_unitRes;

    // Voltage Tab
    QDoubleSpinBox *m_divVin, *m_divR1, *m_divR2;
    QLabel *m_divRes;
    QDoubleSpinBox *m_ledVin, *m_ledVf, *m_ledIf;
    QLabel *m_ledRes;
    QDoubleSpinBox *m_lmVin, *m_lmR1, *m_lmR2;
    QLabel *m_lmRes;

    // Via Tab
    QDoubleSpinBox *m_viaDrill, *m_viaPlating, *m_viaLength, *m_viaDT;
    QSpinBox *m_viaCount;
    QLabel *m_viaRes;

    // SMD Tab
    QLineEdit *m_smdInput;
    QLabel *m_smdRes;

    // Ohm's Law Tab
    QLineEdit *m_ohmV, *m_ohmI, *m_ohmR, *m_ohmP;
    QLabel *m_ohmRes;

    // Time Constant Tab
    QDoubleSpinBox *m_tcR, *m_tcC, *m_tcL;
    QComboBox *m_tcMode;
    QLabel *m_tcRes;

    // Filter Tab
    QComboBox *m_fltType, *m_fltOrder;
    QDoubleSpinBox *m_fltFc, *m_fltZ0, *m_fltR;
    QLabel *m_fltRes;

    // Prefix Tab
    QDoubleSpinBox *m_prefVal;
    QLabel *m_prefRes;

    // Power Tab
    QDoubleSpinBox *m_pwrV, *m_pwrI, *m_pwrR;
    QDoubleSpinBox *m_pwrTamb, *m_pwrRthJA, *m_pwrTmax;
    QLabel *m_pwrRes;
};

#endif // CALCULATOR_DIALOG_H
