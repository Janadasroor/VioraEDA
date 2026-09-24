/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "flux_qt_bridge.h"
#include "flux_workspace_bridge.h"
#include "../../schematic/ui/mini_scope_widget.h"
#include "../../schematic/ui/oscilloscope_window.h"
#include "../../ui/waveform_viewer.h"
#include "../extensions/extension_sandbox.h"
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
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QHash>
#include <QTabWidget>
#include <QGroupBox>
#include <QFrame>
#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <iostream>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <cstring>

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

static const char* dbl_to_str(double d) {
    uint64_t raw;
    std::memcpy(&raw, &d, sizeof(raw));
    return reinterpret_cast<const char*>(static_cast<uintptr_t>(raw));
}

extern "C" {
    // Basic UI construction
    double flux_qt_create_button(double text_dbl) {
        QPushButton* btn = new QPushButton(QString::fromUtf8(dbl_to_str(text_dbl)));
        btn->setAttribute(Qt::WA_DeleteOnClose);
        btn->show();
        return FluxQtBridge::instance().registerObject(btn);
    }

    void flux_qt_msg_box(double title_dbl, double text_dbl) {
        QString title = QString::fromUtf8(dbl_to_str(title_dbl));
        QString text = QString::fromUtf8(dbl_to_str(text_dbl));
        if (QGuiApplication::platformName() == "offscreen" || !qobject_cast<QApplication*>(QCoreApplication::instance())) {
            std::cout << "[" << title.toStdString() << "] " << text.toStdString() << std::endl;
            return;
        }
        QMessageBox::information(nullptr, title, text);
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

    double flux_qt_create_label(double text_dbl) {
        QLabel* label = new QLabel(QString::fromUtf8(dbl_to_str(text_dbl)));
        label->setAttribute(Qt::WA_DeleteOnClose);
        label->show();
        return FluxQtBridge::instance().registerObject(label);
    }

    // New Widgets: LineEdit
    double flux_qt_create_lineedit(double text_dbl) {
        QLineEdit* edit = new QLineEdit(QString::fromUtf8(dbl_to_str(text_dbl)));
        edit->setAttribute(Qt::WA_DeleteOnClose);
        edit->show();
        return FluxQtBridge::instance().registerObject(edit);
    }

    // New Widgets: ComboBox
    double flux_qt_create_combobox() {
        QComboBox* combo = new QComboBox();
        combo->setAttribute(Qt::WA_DeleteOnClose);
        combo->show();
        return FluxQtBridge::instance().registerObject(combo);
    }

    void flux_qt_combo_add_item(double comboHandle, double text_dbl) {
        QComboBox* combo = qobject_cast<QComboBox*>(
            FluxQtBridge::instance().resolveHandle(comboHandle));
        if (combo) combo->addItem(QString::fromUtf8(dbl_to_str(text_dbl)));
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
    double flux_qt_create_checkbox(double text_dbl) {
        QCheckBox* cb = new QCheckBox(QString::fromUtf8(dbl_to_str(text_dbl)));
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

    void flux_qt_table_set_item(double tableHandle, double row, double col, double text_dbl) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        if (table) {
            table->setItem(static_cast<int>(row), static_cast<int>(col),
                new QTableWidgetItem(QString::fromUtf8(dbl_to_str(text_dbl))));
        }
    }

    void flux_qt_table_set_header(double tableHandle, double col, double text_dbl) {
        QTableWidget* table = qobject_cast<QTableWidget*>(
            FluxQtBridge::instance().resolveHandle(tableHandle));
        if (table) {
            table->setHorizontalHeaderItem(static_cast<int>(col),
                new QTableWidgetItem(QString::fromUtf8(dbl_to_str(text_dbl))));
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
    double flux_qt_create_timer(double intervalMs, double callback_dbl) {
        QTimer* timer = new QTimer();
        timer->setInterval(static_cast<int>(intervalMs));
        timer->setSingleShot(false);
        double handle = FluxQtBridge::instance().registerObject(timer);
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(timeout()), dbl_to_str(callback_dbl));
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
    double flux_qt_create_layout(double type_dbl) {
        const char* type = dbl_to_str(type_dbl);
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
    double flux_qt_create_window(double title_dbl) {
        if (!IDE::sandbox().checkPermission(IDE::Permission::GuiCreate, "create_window"))
            return 0.0;
        QDialog* dialog = new QDialog();
        dialog->setWindowTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
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
    void flux_qt_on_click_by_name(double btnHandle, double func_dbl) {
        FluxQtBridge::instance().connectSignalByName(btnHandle, SIGNAL(clicked()), dbl_to_str(func_dbl));
    }

    void flux_qt_on_value_changed_by_name(double handle, double func_dbl) {
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(valueChanged(int)), dbl_to_str(func_dbl));
    }

    void flux_qt_on_current_index_changed_by_name(double handle, double func_dbl) {
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(currentIndexChanged(int)), dbl_to_str(func_dbl));
    }

    void flux_qt_on_toggled_by_name(double handle, double func_dbl) {
        FluxQtBridge::instance().connectSignalByName(handle, SIGNAL(toggled(bool)), dbl_to_str(func_dbl));
    }

    // Tab widget
    double flux_qt_create_tabwidget() {
        QTabWidget* tabs = new QTabWidget();
        tabs->setAttribute(Qt::WA_DeleteOnClose);
        tabs->show();
        return FluxQtBridge::instance().registerObject(tabs);
    }

    void flux_qt_tab_add(double tabsHandle, double paneHandle, double title_dbl) {
        QTabWidget* tabs = qobject_cast<QTabWidget*>(
            FluxQtBridge::instance().resolveHandle(tabsHandle));
        QWidget* pane = qobject_cast<QWidget*>(
            FluxQtBridge::instance().resolveHandle(paneHandle));
        if (tabs && pane)
            tabs->addTab(pane, QString::fromUtf8(dbl_to_str(title_dbl)));
    }

    // Generic container widget
    double flux_qt_create_widget() {
        QWidget* w = new QWidget();
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
        return FluxQtBridge::instance().registerObject(w);
    }

    // Text get/set for QLineEdit, QLabel, QPushButton
    void flux_qt_set_text(double handle, double text_dbl) {
        QObject* obj = FluxQtBridge::instance().resolveHandle(handle);
        if (!obj) return;
        QString text = QString::fromUtf8(dbl_to_str(text_dbl));
        if (QLineEdit* edit = qobject_cast<QLineEdit*>(obj)) {
            edit->setText(text);
        } else if (QLabel* label = qobject_cast<QLabel*>(obj)) {
            label->setText(text);
        } else if (QPushButton* btn = qobject_cast<QPushButton*>(obj)) {
            btn->setText(text);
        } else if (QCheckBox* cb = qobject_cast<QCheckBox*>(obj)) {
            cb->setText(text);
        }
    }

    double flux_qt_get_text(double handle) {
        QLineEdit* edit = qobject_cast<QLineEdit*>(
            FluxQtBridge::instance().resolveHandle(handle));
        if (edit) {
            std::string s = edit->text().toStdString();
            static std::vector<std::string> pool;
            pool.push_back(std::move(s));
            return bit_cast<double>(pool.back().c_str());
        }
        return 0.0;
    }

    // Widget name store (key-value for widget handles)
    static QHash<QString, double> s_widgetStore;
    static std::mutex s_storeMutex;

    void flux_qt_store(double name_dbl, double handle) {
        std::lock_guard<std::mutex> lock(s_storeMutex);
        s_widgetStore[QString::fromUtf8(dbl_to_str(name_dbl))] = handle;
    }

    double flux_qt_load(double name_dbl) {
        std::lock_guard<std::mutex> lock(s_storeMutex);
        auto it = s_widgetStore.find(QString::fromUtf8(dbl_to_str(name_dbl)));
        if (it != s_widgetStore.end())
            return it.value();
        return 0.0;
    }

    // === SMART DEFAULTS & SHORTHAND ===

    double flux_qt_create_panel(double title_dbl) {
        QDialog* dialog = new QDialog();
        dialog->setWindowTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        dialog->setLayout(layout);
        dialog->show();
        return FluxQtBridge::instance().registerObject(dialog);
    }

    double flux_qt_create_form_row(double label_dbl, double input_dbl) {
        QWidget* container = new QWidget();
        QHBoxLayout* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        QLabel* lbl = new QLabel(QString::fromUtf8(dbl_to_str(label_dbl)));
        QWidget* input = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(input_dbl));
        if (input) { layout->addWidget(lbl); layout->addWidget(input); }
        container->setAttribute(Qt::WA_DeleteOnClose);
        container->show();
        return FluxQtBridge::instance().registerObject(container);
    }

    double flux_qt_create_button_bar(double btn1_dbl, double btn2_dbl) {
        QWidget* container = new QWidget();
        QHBoxLayout* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        QWidget* b1 = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(btn1_dbl));
        QWidget* b2 = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(btn2_dbl));
        if (b1) layout->addWidget(b1);
        if (b2) layout->addWidget(b2);
        layout->addStretch();
        container->setAttribute(Qt::WA_DeleteOnClose);
        container->show();
        return FluxQtBridge::instance().registerObject(container);
    }

    double flux_qt_create_separator() {
        QFrame* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setAttribute(Qt::WA_DeleteOnClose);
        line->show();
        return FluxQtBridge::instance().registerObject(line);
    }

    double flux_qt_create_group(double title_dbl) {
        QGroupBox* group = new QGroupBox(QString::fromUtf8(dbl_to_str(title_dbl)));
        QVBoxLayout* layout = new QVBoxLayout(group);
        layout->setContentsMargins(8, 8, 8, 8);
        group->setLayout(layout);
        group->setAttribute(Qt::WA_DeleteOnClose);
        group->show();
        return FluxQtBridge::instance().registerObject(group);
    }

    void flux_qt_set_placeholder(double handle_dbl, double text_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (QLineEdit* le = qobject_cast<QLineEdit*>(w))
            le->setPlaceholderText(QString::fromUtf8(dbl_to_str(text_dbl)));
    }

    void flux_qt_set_tooltip(double handle_dbl, double text_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (w) w->setToolTip(QString::fromUtf8(dbl_to_str(text_dbl)));
    }

    void flux_qt_set_enabled(double handle_dbl, double enabled_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (w) w->setEnabled(enabled_dbl != 0.0);
    }

    void flux_qt_set_fixed_size(double handle_dbl, double w_dbl, double h_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (w) w->setFixedSize(static_cast<int>(w_dbl), static_cast<int>(h_dbl));
    }

    double flux_qt_get_value(double handle_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (QSpinBox* sb = qobject_cast<QSpinBox*>(w)) return sb->value();
        if (QSlider* sl = qobject_cast<QSlider*>(w)) return sl->value();
        if (QProgressBar* pb = qobject_cast<QProgressBar*>(w)) return pb->value();
        return 0;
    }

    void flux_qt_set_value(double handle_dbl, double value_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (QSpinBox* sb = qobject_cast<QSpinBox*>(w)) sb->setValue(static_cast<int>(value_dbl));
        if (QSlider* sl = qobject_cast<QSlider*>(w)) sl->setValue(static_cast<int>(value_dbl));
        if (QProgressBar* pb = qobject_cast<QProgressBar*>(w)) pb->setValue(static_cast<int>(value_dbl));
    }

    void flux_qt_set_range(double handle_dbl, double min_dbl, double max_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (QSpinBox* sb = qobject_cast<QSpinBox*>(w)) sb->setRange(static_cast<int>(min_dbl), static_cast<int>(max_dbl));
        if (QSlider* sl = qobject_cast<QSlider*>(w)) sl->setRange(static_cast<int>(min_dbl), static_cast<int>(max_dbl));
        if (QProgressBar* pb = qobject_cast<QProgressBar*>(w)) pb->setRange(static_cast<int>(min_dbl), static_cast<int>(max_dbl));
    }

    void flux_qt_set_stylesheet(double handle_dbl, double css_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (w) w->setStyleSheet(QString::fromUtf8(dbl_to_str(css_dbl)));
    }

    void flux_qt_connect(double handle_dbl, double signal_dbl, double callback_dbl) {
        const char* signal = dbl_to_str(signal_dbl);
        const char* callback = dbl_to_str(callback_dbl);
        if (!signal || !callback) return;
        FluxQtBridge::instance().connectSignalByName(handle_dbl, signal, callback);
    }

    void flux_qt_add_widget_smart(double parent_dbl, double child_dbl) {
        QWidget* parent = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(parent_dbl));
        QWidget* child = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(child_dbl));
        if (!parent || !child) return;
        QLayout* layout = parent->layout();
        if (!layout) { layout = new QVBoxLayout(parent); parent->setLayout(layout); }
        layout->addWidget(child);
    }

    // === Simulation Widget Factories ===

    double flux_qt_create_scope() {
        if (!IDE::sandbox().checkPermission(IDE::Permission::GuiCreate, "create_scope"))
            return 0.0;
        MiniScopeWidget* scope = new MiniScopeWidget();
        scope->setAttribute(Qt::WA_DeleteOnClose);
        scope->show();
        return FluxQtBridge::instance().registerObject(scope);
    }

    double flux_qt_create_waveform_viewer() {
        if (!IDE::sandbox().checkPermission(IDE::Permission::GuiCreate, "create_waveform_viewer"))
            return 0.0;
        WaveformViewer* viewer = new WaveformViewer();
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
        return FluxQtBridge::instance().registerObject(viewer);
    }

    // Create full analog oscilloscope window (the independent instrument window)
    double flux_qt_create_oscilloscope(double name_dbl) {
        if (!IDE::sandbox().checkPermission(IDE::Permission::GuiCreate, "create_oscilloscope"))
            return 0.0;
        const char* name = dbl_to_str(name_dbl);
        QString itemName = name ? QString::fromUtf8(name) : "Scope";
        QUuid itemId = QUuid::createUuid();

        OscilloscopeWindow* win = new OscilloscopeWindow(itemId, itemName);
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
        return FluxQtBridge::instance().registerObject(win);
    }

    // Create waveforms dock panel (scope display + channel controls only)
    double flux_qt_create_scope_dock() {
        QWidget* dock = new QWidget();
        dock->setObjectName("ScopeDock");
        dock->setWindowTitle("Waveforms");
        QVBoxLayout* mainLayout = new QVBoxLayout(dock);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setSpacing(4);

        // Scope display (the CRT-style grid)
        MiniScopeWidget* scope = new MiniScopeWidget(dock);
        mainLayout->addWidget(scope, 3);

        // Channel controls row
        QWidget* controls = new QWidget(dock);
        QHBoxLayout* ctrlLayout = new QHBoxLayout(controls);
        ctrlLayout->setContentsMargins(4, 2, 4, 2);
        ctrlLayout->setSpacing(8);

        for (int i = 0; i < 4; ++i) {
            QGroupBox* ch = new QGroupBox(QString("CH%1").arg(i + 1), dock);
            ch->setStyleSheet("QGroupBox { font-size: 10px; font-weight: bold; }");
            QGridLayout* gl = new QGridLayout(ch);
            gl->setContentsMargins(4, 4, 4, 4);
            gl->setSpacing(2);

            QCheckBox* en = new QCheckBox("On", ch);
            en->setChecked(i < 2);
            gl->addWidget(en, 0, 0);

            QLabel* vLabel = new QLabel("V/d:", ch);
            vLabel->setStyleSheet("font-size: 9px;");
            gl->addWidget(vLabel, 0, 1);
            QDoubleSpinBox* vDiv = new QDoubleSpinBox(ch);
            vDiv->setRange(0.001, 1000.0);
            vDiv->setValue(1.0);
            vDiv->setDecimals(3);
            vDiv->setFixedWidth(70);
            gl->addWidget(vDiv, 0, 2);

            ctrlLayout->addWidget(ch);
        }

        // Timebase control
        QGroupBox* hGroup = new QGroupBox("Trigger", dock);
        hGroup->setStyleSheet("QGroupBox { font-size: 10px; font-weight: bold; }");
        QGridLayout* hGl = new QGridLayout(hGroup);
        hGl->setContentsMargins(4, 4, 4, 4);
        hGl->setSpacing(2);

        QLabel* tLabel = new QLabel("T/d:", hGroup);
        tLabel->setStyleSheet("font-size: 9px;");
        hGl->addWidget(tLabel, 0, 0);
        QDoubleSpinBox* tDiv = new QDoubleSpinBox(hGroup);
        tDiv->setRange(1e-9, 10.0);
        tDiv->setDecimals(6);
        tDiv->setValue(0.001);
        tDiv->setFixedWidth(70);
        hGl->addWidget(tDiv, 0, 1);

        QLabel* sLabel = new QLabel("Src:", hGroup);
        sLabel->setStyleSheet("font-size: 9px;");
        hGl->addWidget(sLabel, 1, 0);
        QComboBox* trigSrc = new QComboBox(hGroup);
        trigSrc->addItems({"CH1", "CH2", "CH3", "CH4"});
        trigSrc->setFixedWidth(70);
        hGl->addWidget(trigSrc, 1, 1);

        ctrlLayout->addWidget(hGroup);
        mainLayout->addWidget(controls);

        dock->setAttribute(Qt::WA_DeleteOnClose);
        dock->show();
        return FluxQtBridge::instance().registerObject(dock);
    }

    // Find a widget by objectName and return a handle
    double flux_qt_adopt(double name_dbl) {
        if (!IDE::sandbox().checkPermission(IDE::Permission::GuiModify, "adopt_widget"))
            return 0.0;
        const char* name = dbl_to_str(name_dbl);
        if (!name) return 0.0;

        QString targetName = QString::fromUtf8(name);

        // Search all top-level widgets
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->objectName() == targetName) {
                return FluxQtBridge::instance().registerObject(w);
            }
            // Search children recursively
            QWidget* found = w->findChild<QWidget*>(targetName);
            if (found) {
                return FluxQtBridge::instance().registerObject(found);
            }
        }

        // Search by class name
        QByteArray className = targetName.toUtf8();
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->metaObject()->className() == className) {
                return FluxQtBridge::instance().registerObject(w);
            }
            QWidget* found = w->findChild<QWidget*>(className);
            if (found) {
                return FluxQtBridge::instance().registerObject(found);
            }
        }

        return 0.0; // Not found
    }

    // List all available widgets matching a pattern
    double flux_qt_list_widgets(double pattern_dbl) {
        const char* pattern = dbl_to_str(pattern_dbl);
        QString search = pattern ? QString::fromUtf8(pattern) : "";
        QString result;

        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->objectName().isEmpty() && w->windowTitle().isEmpty()) continue;

            QString name = w->objectName();
            QString title = w->windowTitle();
            QString className = w->metaObject()->className();

            if (search.isEmpty() ||
                name.contains(search, Qt::CaseInsensitive) ||
                title.contains(search, Qt::CaseInsensitive) ||
                className.contains(search, Qt::CaseInsensitive)) {
                if (!result.isEmpty()) result += "\n";
                result += QString("%1 [%2] \"%3\"")
                    .arg(className, name, title);
            }
        }

        const char* pooled = Flux::Core::pool_workspace_string(result);
        uint64_t raw = reinterpret_cast<uintptr_t>(pooled);
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    // Reparent a widget into a container
    void flux_qt_embed(double widget_dbl, double container_dbl) {
        if (!IDE::sandbox().checkPermission(IDE::Permission::GuiModify, "embed_widget"))
            return;
        QWidget* widget = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_dbl));
        QWidget* container = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(container_dbl));
        if (!widget || !container) return;

        // Reparent the widget
        widget->setParent(container);

        // Add to container's layout
        QLayout* layout = container->layout();
        if (!layout) {
            layout = new QVBoxLayout(container);
            container->setLayout(layout);
        }
        layout->addWidget(widget);
        widget->show();
    }

    // Get widget property value as string
    double flux_qt_get_widget_info(double handle_dbl, double prop_dbl) {
        QWidget* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle_dbl));
        if (!w) return 0.0;

        const char* prop = dbl_to_str(prop_dbl);
        QString propName = QString::fromUtf8(prop);
        QString result;

        if (propName == "name") result = w->objectName();
        else if (propName == "class") result = w->metaObject()->className();
        else if (propName == "title") result = w->windowTitle();
        else if (propName == "visible") result = w->isVisible() ? "true" : "false";
        else if (propName == "width") result = QString::number(w->width());
        else if (propName == "height") result = QString::number(w->height());
        else if (propName == "x") result = QString::number(w->x());
        else if (propName == "y") result = QString::number(w->y());

        const char* pooled = Flux::Core::pool_workspace_string(result);
        uint64_t raw = reinterpret_cast<uintptr_t>(pooled);
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }
}
