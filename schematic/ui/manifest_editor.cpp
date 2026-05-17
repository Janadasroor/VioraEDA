#include "manifest_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileInfo>

ManifestEditor::ManifestEditor(const QString& manifestPath, QWidget* parent)
    : QDialog(parent), m_manifestPath(manifestPath) {
    setupUI();
    loadManifest();
    setWindowTitle("Edit Manifest — " + QFileInfo(manifestPath).absolutePath());
    resize(450, 550);
    setModal(true);
}

void ManifestEditor::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    // Basic info
    auto* form = new QFormLayout();
    form->setSpacing(6);

    m_idEdit = new QLineEdit();
    form->addRow("ID:", m_idEdit);

    m_nameEdit = new QLineEdit();
    form->addRow("Name:", m_nameEdit);

    m_versionEdit = new QLineEdit();
    form->addRow("Version:", m_versionEdit);

    m_authorEdit = new QLineEdit();
    form->addRow("Author:", m_authorEdit);

    m_mainFileEdit = new QLineEdit();
    form->addRow("Main File:", m_mainFileEdit);

    m_descEdit = new QTextEdit();
    m_descEdit->setMaximumHeight(50);
    form->addRow("Description:", m_descEdit);

    layout->addLayout(form);

    // Menu entries
    auto* menuLabel = new QLabel("Menu Entries:");
    menuLabel->setStyleSheet("font-weight: bold; color: #d4d4d4;");
    layout->addWidget(menuLabel);

    m_menuList = new QListWidget();
    m_menuList->setMaximumHeight(80);
    layout->addWidget(m_menuList);

    auto* menuRow = new QHBoxLayout();
    m_menuPathEdit = new QLineEdit();
    m_menuPathEdit->setPlaceholderText("Extensions/My Extension");
    m_menuActionEdit = new QLineEdit();
    m_menuActionEdit->setPlaceholderText("open_panel");

    auto* addMenuBtn = new QPushButton("+");
    addMenuBtn->setFixedWidth(30);
    auto* removeMenuBtn = new QPushButton("−");
    removeMenuBtn->setFixedWidth(30);

    menuRow->addWidget(m_menuPathEdit);
    menuRow->addWidget(m_menuActionEdit);
    menuRow->addWidget(addMenuBtn);
    menuRow->addWidget(removeMenuBtn);
    layout->addLayout(menuRow);

    connect(addMenuBtn, &QPushButton::clicked, this, &ManifestEditor::onAddMenuEntry);
    connect(removeMenuBtn, &QPushButton::clicked, this, &ManifestEditor::onRemoveMenuEntry);

    // Context entries
    auto* ctxLabel = new QLabel("Context Handlers:");
    ctxLabel->setStyleSheet("font-weight: bold; color: #d4d4d4;");
    layout->addWidget(ctxLabel);

    m_contextList = new QListWidget();
    m_contextList->setMaximumHeight(80);
    layout->addWidget(m_contextList);

    auto* ctxRow = new QHBoxLayout();
    m_contextTypeEdit = new QLineEdit();
    m_contextTypeEdit->setPlaceholderText("Resistor");
    m_contextActionEdit = new QLineEdit();
    m_contextActionEdit->setPlaceholderText("handle_resistor");

    auto* addCtxBtn = new QPushButton("+");
    addCtxBtn->setFixedWidth(30);
    auto* removeCtxBtn = new QPushButton("−");
    removeCtxBtn->setFixedWidth(30);

    ctxRow->addWidget(m_contextTypeEdit);
    ctxRow->addWidget(m_contextActionEdit);
    ctxRow->addWidget(addCtxBtn);
    ctxRow->addWidget(removeCtxBtn);
    layout->addLayout(ctxRow);

    connect(addCtxBtn, &QPushButton::clicked, this, &ManifestEditor::onAddContext);
    connect(removeCtxBtn, &QPushButton::clicked, this, &ManifestEditor::onRemoveContext);

    // Status + buttons
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #888; font-size: 9pt;");
    layout->addWidget(m_statusLabel);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_saveBtn = new QPushButton("Save");
    m_saveBtn->setStyleSheet(
        "QPushButton { background: #10b981; color: white; border: none; padding: 8px 24px; border-radius: 4px; font-weight: bold; } "
        "QPushButton:hover { background: #059669; }"
    );
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        if (save()) accept();
    });

    auto* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(
        "QPushButton { background: #3e3e42; color: #ccc; border: 1px solid #555; padding: 8px 20px; border-radius: 4px; }"
    );
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    layout->addLayout(btnLayout);

    // Track changes
    QList<QLineEdit*> edits = {m_idEdit, m_nameEdit, m_versionEdit, m_authorEdit, m_mainFileEdit,
                                     m_menuPathEdit, m_menuActionEdit, m_contextTypeEdit, m_contextActionEdit};
    for (auto* e : edits) {
        connect(e, &QLineEdit::textChanged, this, &ManifestEditor::onTextChanged);
    }
    connect(m_descEdit, &QTextEdit::textChanged, this, &ManifestEditor::onTextChanged);
}

void ManifestEditor::loadManifest() {
    QFile file(m_manifestPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError) return;

    QJsonObject obj = doc.object();
    m_idEdit->setText(obj["id"].toString());
    m_nameEdit->setText(obj["name"].toString());
    m_versionEdit->setText(obj["version"].toString("0.1.0"));
    m_authorEdit->setText(obj["author"].toString());
    m_mainFileEdit->setText(obj["main"].toString("main.flux"));
    m_descEdit->setPlainText(obj["description"].toString());

    parseMenuEntries(obj);
    parseContexts(obj);
}

void ManifestEditor::parseMenuEntries(const QJsonObject& obj) {
    m_menuList->clear();
    if (obj.contains("menu")) {
        QJsonArray arr = obj["menu"].toArray();
        for (const auto& val : arr) {
            QJsonObject entry = val.toObject();
            QString path = entry["path"].toString();
            QString action = entry["action"].toString();
            m_menuList->addItem(QString("%s → %s").arg(path, action));
        }
    }
}

void ManifestEditor::parseContexts(const QJsonObject& obj) {
    m_contextList->clear();
    if (obj.contains("contexts")) {
        QJsonArray arr = obj["contexts"].toArray();
        for (const auto& val : arr) {
            QJsonObject entry = val.toObject();
            QString type = entry["componentType"].toString();
            QString action = entry["action"].toString();
            m_contextList->addItem(QString("%s → %s").arg(type, action));
        }
    }
}

void ManifestEditor::onAddMenuEntry() {
    QString path = m_menuPathEdit->text().trimmed();
    QString action = m_menuActionEdit->text().trimmed();
    if (path.isEmpty() || action.isEmpty()) return;
    m_menuList->addItem(QString("%s → %s").arg(path, action));
    m_menuPathEdit->clear();
    m_menuActionEdit->clear();
    onTextChanged();
}

void ManifestEditor::onRemoveMenuEntry() {
    auto* item = m_menuList->currentItem();
    if (item) {
        delete m_menuList->takeItem(m_menuList->row(item));
        onTextChanged();
    }
}

void ManifestEditor::onAddContext() {
    QString type = m_contextTypeEdit->text().trimmed();
    QString action = m_contextActionEdit->text().trimmed();
    if (type.isEmpty() || action.isEmpty()) return;
    m_contextList->addItem(QString("%s → %s").arg(type, action));
    m_contextTypeEdit->clear();
    m_contextActionEdit->clear();
    onTextChanged();
}

void ManifestEditor::onRemoveContext() {
    auto* item = m_contextList->currentItem();
    if (item) {
        delete m_contextList->takeItem(m_contextList->row(item));
        onTextChanged();
    }
}

void ManifestEditor::onTextChanged() {
    m_modified = true;
    m_statusLabel->setText("Unsaved changes");
}

bool ManifestEditor::save() {
    QString id = m_idEdit->text().trimmed();
    if (id.isEmpty()) {
        QMessageBox::warning(this, "Error", "Extension ID is required.");
        return false;
    }

    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = m_nameEdit->text().trimmed();
    obj["version"] = m_versionEdit->text().trimmed();
    obj["author"] = m_authorEdit->text().trimmed();
    obj["description"] = m_descEdit->toPlainText().trimmed();
    obj["main"] = m_mainFileEdit->text().trimmed();

    // Menu entries
    QJsonArray menuArr;
    for (int i = 0; i < m_menuList->count(); ++i) {
        QString text = m_menuList->item(i)->text();
        QStringList parts = text.split(" → ");
        if (parts.size() == 2) {
            QJsonObject entry;
            entry["path"] = parts[0].trimmed();
            entry["action"] = parts[1].trimmed();
            menuArr.append(entry);
        }
    }
    if (!menuArr.isEmpty()) obj["menu"] = menuArr;

    // Contexts
    QJsonArray ctxArr;
    for (int i = 0; i < m_contextList->count(); ++i) {
        QString text = m_contextList->item(i)->text();
        QStringList parts = text.split(" → ");
        if (parts.size() == 2) {
            QJsonObject entry;
            entry["componentType"] = parts[0].trimmed();
            entry["action"] = parts[1].trimmed();
            ctxArr.append(entry);
        }
    }
    if (!ctxArr.isEmpty()) obj["contexts"] = ctxArr;

    QJsonDocument doc(obj);
    QFile file(m_manifestPath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Cannot write to manifest file.");
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_modified = false;
    m_statusLabel->setText("Saved ✓");
    return true;
}
