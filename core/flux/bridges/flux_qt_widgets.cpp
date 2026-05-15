#include "flux_qt_bridge.h"
#include <QPushButton>
#include <QMessageBox>
#include <QSlider>
#include <QLCDNumber>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QBoxLayout>
#include <QDialog>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <cstring>

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

    void flux_qt_table_set_value(double tableHandle, double row, double col, double value) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        if (table) {
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setData(Qt::DisplayRole, value);
            table->setItem(static_cast<int>(row), static_cast<int>(col), item);
        }
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

    // Timer
    double flux_qt_create_timer(double intervalMs, const char* callbackName) {
        QTimer* timer = new QTimer();
        timer->setInterval(static_cast<int>(intervalMs));
        timer->setSingleShot(false);
        double handle = FluxQtBridge::instance().registerObject(timer);
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(timeout()), callbackName);
        return handle;
    }

    void flux_qt_timer_start(double handle) {
        QTimer* timer = qobject_cast<QTimer*>(
            FluxQtBridge::instance().resolveHandle(handle));
        if (timer) timer->start();
    }

    void flux_qt_timer_stop(double handle) {
        QTimer* timer = qobject_cast<QTimer*>(
            FluxQtBridge::instance().resolveHandle(handle));
        if (timer) timer->stop();
    }

    // LCD helper — display is a slot, not a Q_PROPERTY
    void flux_qt_lcd_display(double handle, double value) {
        QLCDNumber* lcd = qobject_cast<QLCDNumber*>(
            FluxQtBridge::instance().resolveHandle(handle));
        if (lcd) lcd->display(value);
    }

    // Window helpers
    void flux_qt_set_window_size(double handle, double w, double h) {
        QWidget* wgt = qobject_cast<QWidget*>(
            FluxQtBridge::instance().resolveHandle(handle));
        if (wgt) wgt->resize(static_cast<int>(w), static_cast<int>(h));
    }

    // Layout system
    double flux_qt_create_layout(const char* type) {
        QBoxLayout::Direction dir = QBoxLayout::TopToBottom;
        if (strcmp(type, "hbox") == 0) dir = QBoxLayout::LeftToRight;
        else if (strcmp(type, "vbox") == 0) dir = QBoxLayout::TopToBottom;
        else if (strcmp(type, "grid") == 0) {
            QGridLayout* grid = new QGridLayout();
            grid->setContentsMargins(4, 4, 4, 4);
            // QLayout is a QObject in Qt6 — register it directly
            return FluxQtBridge::instance().registerObject(grid);
        }
        QBoxLayout* box = new QBoxLayout(dir);
        box->setContentsMargins(4, 4, 4, 4);
        return FluxQtBridge::instance().registerObject(box);
    }

    void flux_qt_set_layout(double containerHandle, double layoutHandle) {
        QWidget* container = qobject_cast<QWidget*>(
            FluxQtBridge::instance().resolveHandle(containerHandle));
        QLayout* layout = qobject_cast<QLayout*>(
            FluxQtBridge::instance().resolveHandle(layoutHandle));
        if (container && layout)
            container->setLayout(layout);
    }

    void flux_qt_layout_add_widget(double layoutHandle, double widgetHandle) {
        QLayout* layout = qobject_cast<QLayout*>(
            FluxQtBridge::instance().resolveHandle(layoutHandle));
        QWidget* widget = qobject_cast<QWidget*>(
            FluxQtBridge::instance().resolveHandle(widgetHandle));
        if (layout && widget) {
            if (auto* box = qobject_cast<QBoxLayout*>(layout))
                box->addWidget(widget);
            else if (auto* grid = qobject_cast<QGridLayout*>(layout))
                grid->addWidget(widget);
        }
    }

    void flux_qt_grid_add_widget(double layoutHandle, double widgetHandle,
                                  double row, double col,
                                  double rowSpan, double colSpan) {
        QGridLayout* grid = qobject_cast<QGridLayout*>(
            FluxQtBridge::instance().resolveHandle(layoutHandle));
        QWidget* widget = qobject_cast<QWidget*>(
            FluxQtBridge::instance().resolveHandle(widgetHandle));
        if (grid && widget)
            grid->addWidget(widget, static_cast<int>(row), static_cast<int>(col),
                           static_cast<int>(rowSpan), static_cast<int>(colSpan));
    }

    // Container / Window helpers
    double flux_qt_create_window(const char* title) {
        QDialog* dialog = new QDialog();
        dialog->setWindowTitle(QString::fromUtf8(title));
        // No default layout — user calls flux_qt_set_layout explicitly
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
