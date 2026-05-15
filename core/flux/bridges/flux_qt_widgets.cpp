#include "flux_qt_bridge.h"
#include <QPushButton>
#include <QMessageBox>
#include <QSlider>
#include <QLCDNumber>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>

extern "C" {
    // Basic UI construction
    double flux_qt_create_button(const char* text) {
        QPushButton* btn = new QPushButton(QString::fromUtf8(text));
        btn->setAttribute(Qt::WA_DeleteOnClose);
        btn->show();
        return FluxQtBridge::instance().registerObject(btn);
    }

    void flux_qt_msg_box(const char* title, const char* text) {
        QMessageBox::information(nullptr, QString::fromUtf8(title), QString::fromUtf8(text));
    }

    // New Widgets: Sliders & LCDs
    double flux_qt_create_slider(double orientation) {
        Qt::Orientation orient = (orientation == 1.0) ? Qt::Vertical : Qt::Horizontal;
        QSlider* slider = new QSlider(orient);
        slider->setAttribute(Qt::WA_DeleteOnClose);
        slider->setMinimum(0);
        slider->setMaximum(100);
        slider->show();
        return FluxQtBridge::instance().registerObject(slider);
    }

    double flux_qt_create_lcd() {
        QLCDNumber* lcd = new QLCDNumber();
        lcd->setAttribute(Qt::WA_DeleteOnClose);
        lcd->setDigitCount(8);
        lcd->show();
        return FluxQtBridge::instance().registerObject(lcd);
    }

    double flux_qt_create_label(const char* text) {
        QLabel* label = new QLabel(QString::fromUtf8(text));
        label->setAttribute(Qt::WA_DeleteOnClose);
        label->show();
        return FluxQtBridge::instance().registerObject(label);
    }

    // New Widgets: ComboBox
    double flux_qt_create_combobox() {
        QComboBox* combo = new QComboBox();
        combo->setAttribute(Qt::WA_DeleteOnClose);
        combo->show();
        return FluxQtBridge::instance().registerObject(combo);
    }

    void flux_qt_combo_add_item(double comboHandle, const char* text) {
        QComboBox* combo = qobject_cast<QComboBox*>(
            FluxQtBridge::instance().resolveHandle(comboHandle));
        if (combo) combo->addItem(QString::fromUtf8(text));
    }

    void flux_qt_combo_clear(double comboHandle) {
        QComboBox* combo = qobject_cast<QComboBox*>(
            FluxQtBridge::instance().resolveHandle(comboHandle));
        if (combo) combo->clear();
    }

    void flux_qt_combo_set_current_index(double comboHandle, double index) {
        QComboBox* combo = qobject_cast<QComboBox*>(
            FluxQtBridge::instance().resolveHandle(comboHandle));
        if (combo) combo->setCurrentIndex(static_cast<int>(index));
    }

    // New Widgets: CheckBox
    double flux_qt_create_checkbox(const char* text) {
        QCheckBox* cb = new QCheckBox(QString::fromUtf8(text));
        cb->setAttribute(Qt::WA_DeleteOnClose);
        cb->show();
        return FluxQtBridge::instance().registerObject(cb);
    }

    // New Widgets: SpinBox
    double flux_qt_create_spinbox() {
        QSpinBox* spin = new QSpinBox();
        spin->setAttribute(Qt::WA_DeleteOnClose);
        spin->setMinimum(0);
        spin->setMaximum(1000000);
        spin->show();
        return FluxQtBridge::instance().registerObject(spin);
    }

    // New Widgets: ProgressBar
    double flux_qt_create_progressbar() {
        QProgressBar* bar = new QProgressBar();
        bar->setAttribute(Qt::WA_DeleteOnClose);
        bar->setMinimum(0);
        bar->setMaximum(100);
        bar->setValue(0);
        bar->show();
        return FluxQtBridge::instance().registerObject(bar);
    }

    // New Widgets: TableView
    double flux_qt_create_tableview(double rows, double cols) {
        QTableWidget* table = new QTableWidget(
            static_cast<int>(rows), static_cast<int>(cols));
        table->setAttribute(Qt::WA_DeleteOnClose);
        table->show();
        return FluxQtBridge::instance().registerObject(table);
    }

    void flux_qt_table_set_item(double tableHandle, double row, double col, const char* text) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        if (table) {
            table->setItem(static_cast<int>(row), static_cast<int>(col),
                new QTableWidgetItem(QString::fromUtf8(text)));
        }
    }

    void flux_qt_table_set_header(double tableHandle, double col, const char* text) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        if (table) {
            table->setHorizontalHeaderItem(static_cast<int>(col),
                new QTableWidgetItem(QString::fromUtf8(text)));
        }
    }

    double flux_qt_table_row_count(double tableHandle) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        return table ? static_cast<double>(table->rowCount()) : 0.0;
    }

    double flux_qt_table_col_count(double tableHandle) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        return table ? static_cast<double>(table->columnCount()) : 0.0;
    }

    // Container / Window helpers
    double flux_qt_create_window(const char* title) {
        QDialog* dialog = new QDialog();
        dialog->setWindowTitle(QString::fromUtf8(title));
        dialog->setLayout(new QVBoxLayout());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return FluxQtBridge::instance().registerObject(dialog);
    }

    void flux_qt_add_widget(double containerHandle, double widgetHandle) {
        QWidget* container = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(containerHandle));
        QWidget* widget = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widgetHandle));
        if (container && widget && container->layout()) {
            container->layout()->addWidget(widget);
        }
    }

    // Event binding (old: numeric handle, deprecated)
    void flux_qt_on_click(double btnHandle, double funcHandle) {
        FluxQtBridge::instance().connectSignal(btnHandle, SIGNAL(clicked()), funcHandle);
    }

    void flux_qt_on_value_changed(double handle, double funcHandle) {
        FluxQtBridge::instance().connectSignal(handle, SIGNAL(valueChanged(int)), funcHandle);
    }

    void flux_qt_on_current_index_changed(double handle, double funcHandle) {
        FluxQtBridge::instance().connectSignal(handle, SIGNAL(currentIndexChanged(int)), funcHandle);
    }

    void flux_qt_on_toggled(double handle, double funcHandle) {
        FluxQtBridge::instance().connectSignal(handle, SIGNAL(toggled(bool)), funcHandle);
    }

    // Event binding (string function name — preferred in extensions)
    void flux_qt_on_click_by_name(double btnHandle, const char* funcName) {
        FluxQtBridge::instance().connectSignalByName(btnHandle, SIGNAL(clicked()), funcName);
    }

    void flux_qt_on_value_changed_by_name(double handle, const char* funcName) {
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(valueChanged(int)), funcName);
    }

    void flux_qt_on_current_index_changed_by_name(double handle, const char* funcName) {
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(currentIndexChanged(int)), funcName);
    }

    void flux_qt_on_toggled_by_name(double handle, const char* funcName) {
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(toggled(bool)), funcName);
    }
}
