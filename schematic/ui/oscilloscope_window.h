#ifndef OSCILLOSCOPE_WINDOW_H
#define OSCILLOSCOPE_WINDOW_H

#include <QMainWindow>
#include <QUuid>
#include <QMap>
#include "mini_scope_widget.h"
#include "../items/oscilloscope_item.h"
#include "../../simulator/core/sim_results.h"

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class NetManager;

/**
 * @brief A standalone, hardware-realistic window for an Analog Oscilloscope instrument.
 */
class OscilloscopeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit OscilloscopeWindow(const QUuid& itemId, const QString& itemName, QWidget* parent = nullptr);
    ~OscilloscopeWindow();

    QUuid itemId() const { return m_itemId; }

    /**
     * @brief Updates the window with new simulation results.
     * Filters for nets connected specifically to this instrument.
     */
    void updateResults(const SimResults& results, NetManager* netManager);

    /**
     * @brief Clear existing traces.
     */
    void clear();

Q_SIGNALS:
    void windowClosing(const QUuid& id);
    void configChanged(const QUuid& id, const OscilloscopeItem::Config& cfg);
    void propertiesRequested(const QUuid& id);

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void onChannelToggled(int ch, bool checked);
    void onTimebaseChanged(double value);
    void onVoltsDivChanged(int ch, double value);
    void onOffsetChanged(int ch, double value);
    void onTriggerSourceChanged(int index);
    void onTriggerLevelChanged(double value);
    void onFreezeClicked();
    void onClearMemoriesClicked();

private:
    void setupUI();
    void updateInstrumentConfig();
    void applyConfigToUI();

    QUuid m_itemId;
    QString m_itemName;
    OscilloscopeItem::Config m_config;

    // UI Components
    MiniScopeWidget* m_scopeDisplay;
    
    // Channel Controls
    struct ChannelUI {
        QCheckBox* enabled;
        QDoubleSpinBox* voltsDiv;
        QDoubleSpinBox* offset;
        QLabel* label;
    } m_channelUI[4];

    QDoubleSpinBox* m_timebaseSpin;
    QComboBox* m_triggerSourceCombo;
    QDoubleSpinBox* m_triggerLevelSpin;
    QPushButton* m_freezeBtn;
    QPushButton* m_clearMemBtn;

    NetManager* m_lastNetManager = nullptr;
};

#endif // OSCILLOSCOPE_WINDOW_H
