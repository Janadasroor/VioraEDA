#include "new_extension_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>

static QString configDir() {
    return QDir::homePath() + "/.config/VioraEDA/extensions";
}

NewExtensionDialog::NewExtensionDialog(QWidget* parent)
    : QDialog(parent) {
    setupUI();
    setWindowTitle("New Extension");
    resize(420, 400);
    setModal(true);
}

void NewExtensionDialog::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Form
    auto* form = new QFormLayout();
    form->setSpacing(8);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("My Extension");
    form->addRow("Name:", m_nameEdit);

    m_idEdit = new QLineEdit();
    m_idEdit->setPlaceholderText("my-extension");
    form->addRow("ID:", m_idEdit);

    m_authorEdit = new QLineEdit();
    m_authorEdit->setPlaceholderText("Your Name");
    form->addRow("Author:", m_authorEdit);

    m_versionEdit = new QLineEdit("0.1.0");
    form->addRow("Version:", m_versionEdit);

    m_descEdit = new QTextEdit();
    m_descEdit->setPlaceholderText("What does this extension do?");
    m_descEdit->setMaximumHeight(60);
    form->addRow("Description:", m_descEdit);

    m_templateCombo = new QComboBox();
    m_templateCombo->addItem("Empty (no UI)");
    m_templateCombo->addItem("Panel (window with widgets)");
    m_templateCombo->addItem("Calculator");
    form->addRow("Template:", m_templateCombo);

    layout->addLayout(form);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #888; font-size: 9pt;");
    layout->addWidget(m_statusLabel);

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_createBtn = new QPushButton("Create");
    m_createBtn->setStyleSheet(
        "QPushButton { background: #10b981; color: white; border: none; padding: 8px 24px; border-radius: 4px; font-weight: bold; } "
        "QPushButton:hover { background: #059669; } "
        "QPushButton:disabled { background: #555; color: #888; }"
    );
    m_createBtn->setEnabled(false);
    connect(m_createBtn, &QPushButton::clicked, this, &NewExtensionDialog::onCreate);

    auto* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(
        "QPushButton { background: #3e3e42; color: #ccc; border: 1px solid #555; padding: 8px 20px; border-radius: 4px; }"
    );
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(m_createBtn);
    layout->addLayout(btnLayout);

    // Auto-generate ID from name
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewExtensionDialog::onIdChanged);
}

void NewExtensionDialog::onIdChanged(const QString& text) {
    if (m_idEdit->text().isEmpty() || m_idEdit->text() == generateId(m_idEdit->text())) {
        m_idEdit->setText(generateId(text));
    }
    m_id = m_idEdit->text().trimmed();
    m_createBtn->setEnabled(!m_id.isEmpty());

    // Show target path
    QString target = configDir() + "/" + m_id;
    if (QDir(target).exists()) {
        m_statusLabel->setText(QString("<span style='color:#f87171;'>Warning: extension '%1' already exists</span>").arg(m_id));
        m_createBtn->setEnabled(false);
    } else {
        m_statusLabel->setText(QString("Will create: %1").arg(target));
    }
}

QString NewExtensionDialog::generateId(const QString& name) const {
    QString id = name.toLower().trimmed();
    id.replace(QRegularExpression("[^a-z0-9]+"), "-");
    id.replace(QRegularExpression("^-+|-+$"), "");
    return id;
}

void NewExtensionDialog::onCreate() {
    m_id = m_idEdit->text().trimmed();
    if (m_id.isEmpty()) return;

    QString dirPath = configDir() + "/" + m_id;
    QDir dir(dirPath);
    if (dir.exists()) {
        QMessageBox::warning(this, "Error", QString("Extension '%1' already exists.").arg(m_id));
        return;
    }
    if (!dir.mkpath(".")) {
        QMessageBox::critical(this, "Error", "Cannot create extension directory.");
        return;
    }

    // Write manifest.json
    QFile manifest(dir.filePath("manifest.json"));
    if (manifest.open(QIODevice::WriteOnly)) {
        manifest.write(scaffoldManifest().toUtf8());
        manifest.close();
    }

    // Write main.flux
    QFile main(dir.filePath("main.flux"));
    if (main.open(QIODevice::WriteOnly)) {
        main.write(scaffoldMain(m_templateCombo->currentText()).toUtf8());
        main.close();
    }

    m_path = dirPath;
    accept();
}

QString NewExtensionDialog::scaffoldManifest() const {
    return QString(R"({
  "id": "%1",
  "name": "%2",
  "version": "%3",
  "author": "%4",
  "description": "%5",
  "main": "main.flux",
  "hooks": {
    "onActivate": "on_activate"
  },
  "menu": [
    {"path": "%2", "action": "open_panel"}
  ]
}
)").arg(m_id,
        m_nameEdit->text().trimmed().isEmpty() ? m_id : m_nameEdit->text().trimmed(),
        m_versionEdit->text().trimmed(),
        m_authorEdit->text().trimmed(),
        m_descEdit->toPlainText().trimmed().isEmpty() ? "Edit this description" : m_descEdit->toPlainText().trimmed());
}

QString NewExtensionDialog::scaffoldMain(const QString& templateType) const {
    QString name = m_nameEdit->text().trimmed().isEmpty() ? m_id : m_nameEdit->text().trimmed();

    if (templateType == "Empty (no UI)") {
        return QString(R"(def init() {
    viora_flux_print("%1 loaded")
}

def on_activate() {
    // Called when extension is activated
}

def open_panel() {
    // Called from Extensions menu
}
)").arg(name);
    }

    if (templateType == "Panel (window with widgets)") {
        return QString(R"(def init() {
    viora_flux_print("%1 loaded")
}

def open_panel() {
    win = flux_qt_create_window("%1")
    box = flux_qt_create_layout("vbox")
    flux_qt_set_layout(win, box)

    lbl = flux_qt_create_label("Hello from %1!")
    flux_qt_layout_add_widget(box, lbl)

    btn = flux_qt_create_button("Click Me")
    flux_qt_layout_add_widget(box, btn)

    flux_qt_on_click_by_name(btn, "on_click")
    flux_qt_set_window_size(win, 300.0, 150.0)
}

def on_click() {
    flux_qt_msg_box("%1", "Button clicked!")
}
)").arg(name);
    }

    // Calculator template
    return QString(R"(def init() {
    viora_flux_print("%1 loaded")
}

def on_calculate() {
    val1 = flux_qt_get_property(input1, "value")
    val2 = flux_qt_get_property(input2, "value")
    result = val1 + val2
    flux_qt_set_property(result_display, "value", result)
}

def open_panel() {
    win = flux_qt_create_window("%1")
    box = flux_qt_create_layout("vbox")
    flux_qt_set_layout(win, box)

    lbl1 = flux_qt_create_label("Value 1:")
    flux_qt_layout_add_widget(box, lbl1)
    input1 = flux_qt_create_spinbox()
    flux_qt_set_property(input1, "value", 0.0)
    flux_qt_layout_add_widget(box, input1)

    lbl2 = flux_qt_create_label("Value 2:")
    flux_qt_layout_add_widget(box, lbl2)
    input2 = flux_qt_create_spinbox()
    flux_qt_set_property(input2, "value", 0.0)
    flux_qt_layout_add_widget(box, input2)

    btn = flux_qt_create_button("Calculate")
    flux_qt_layout_add_widget(box, btn)

    result_label = flux_qt_create_label("Result:")
    flux_qt_layout_add_widget(box, result_label)
    result_display = flux_qt_create_spinbox()
    flux_qt_set_property(result_display, "enabled", 0.0)
    flux_qt_set_property(result_display, "readOnly", 1.0)
    flux_qt_layout_add_widget(box, result_display)

    flux_qt_on_click_by_name(btn, "on_calculate")
    flux_qt_set_window_size(win, 280.0, 250.0)
}
)").arg(name);
}
