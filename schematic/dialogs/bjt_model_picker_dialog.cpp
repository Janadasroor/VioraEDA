#include "bjt_model_picker_dialog.h"

#include "../../simulator/bridge/model_library_manager.h"
#include "../../core/theme_manager.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

BjtModelPickerDialog::BjtModelPickerDialog(bool pnp, QWidget* parent)
    : QDialog(parent), m_pnp(pnp) {
    setWindowTitle(m_pnp ? "Pick PNP Model" : "Pick NPN Model");
    setModal(true);
    setMinimumSize(500, 400);

    auto* layout = new QVBoxLayout(this);

    auto* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Search:"));
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Filter models by name...");
    searchLayout->addWidget(m_searchEdit);
    layout->addLayout(searchLayout);

    m_modelList = new QListWidget();
    m_modelList->setAlternatingRowColors(true);
    layout->addWidget(m_modelList);

    m_detailLabel = new QLabel("Select a model to see details");
    m_detailLabel->setStyleSheet("color: #888; font-size: 11px; padding: 4px;");
    m_detailLabel->setWordWrap(true);
    layout->addWidget(m_detailLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &BjtModelPickerDialog::applySelected);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &BjtModelPickerDialog::filterModels);
    connect(m_modelList, &QListWidget::itemDoubleClicked, this, &BjtModelPickerDialog::onModelSelected);
    connect(m_modelList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        if (current) {
            m_detailLabel->setText(current->data(Qt::UserRole + 1).toString());
        }
    });

    if (ThemeManager::theme()) {
        setStyleSheet(ThemeManager::theme()->widgetStylesheet());
    }

    loadModels();
}

void BjtModelPickerDialog::loadModels() {
    const auto allModels = ModelLibraryManager::instance().allModels();
    const QString wantedType = m_pnp ? "PNP" : "NPN";

    for (const auto& mi : allModels) {
        if (mi.type.compare(wantedType, Qt::CaseInsensitive) != 0) continue;

        QString params = mi.params.join(", ");
        if (params.length() > 120) params = params.left(117) + "...";

        auto* item = new QListWidgetItem(mi.name);
        item->setData(Qt::UserRole, mi.name);
        item->setData(
            Qt::UserRole + 1,
            QString("%1\nType: %2\nParams: %3\nLibrary: %4")
                .arg(mi.name, wantedType, params, QFileInfo(mi.libraryPath).fileName()));
        m_modelList->addItem(item);
    }

    if (m_modelList->count() > 0) {
        m_modelList->setCurrentRow(0);
    }
}

void BjtModelPickerDialog::filterModels(const QString& text) {
    for (int i = 0; i < m_modelList->count(); ++i) {
        auto* item = m_modelList->item(i);
        const bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void BjtModelPickerDialog::onModelSelected(QListWidgetItem* item) {
    if (!item) return;
    m_selectedModel = item->data(Qt::UserRole).toString();
    accept();
}

void BjtModelPickerDialog::applySelected() {
    auto* item = m_modelList->currentItem();
    if (item) {
        m_selectedModel = item->data(Qt::UserRole).toString();
    }
    accept();
}

QString BjtModelPickerDialog::selectedModel() const {
    return m_selectedModel;
}
