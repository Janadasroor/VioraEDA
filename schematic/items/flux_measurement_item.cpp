#include "flux_measurement_item.h"
#include "../ui/flux_code_editor.h"
#include <QPainter>
#include <QJsonObject>
#include <QDialog>
#include <QTabWidget>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFontMetricsF>

FluxMeasurementItem::FluxMeasurementItem(QPointF pos, QGraphicsItem* parent)
    : SchematicItem(parent)
{
    setPos(pos);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setZValue(100);
    
    m_font = QFont("Consolas", 10, QFont::Bold);
    m_expression = "return 0.0;";
    
    updateSize();
}

QJsonObject FluxMeasurementItem::toJson() const {
    QJsonObject json; 
    json["type"] = itemTypeName();
    json["id"] = id().toString();
    json["x"] = pos().x();
    json["y"] = pos().y();
    json["expression"] = m_expression;
    json["scriptFile"] = m_scriptFile;
    json["prefix"] = m_prefix;
    json["unit"] = m_unit;
    json["result"] = m_result;
    return json;
}

bool FluxMeasurementItem::fromJson(const QJsonObject& json) {
    if (json.contains("id")) setId(QUuid::fromString(json["id"].toString()));
    if (json.contains("x")) setPos(json["x"].toDouble(), json["y"].toDouble());
    if (json.contains("expression")) m_expression = json["expression"].toString();
    if (json.contains("scriptFile")) m_scriptFile = json["scriptFile"].toString();
    if (json.contains("prefix")) m_prefix = json["prefix"].toString();
    if (json.contains("unit")) m_unit = json["unit"].toString();
    if (json.contains("result")) m_result = json["result"].toString();
    updateSize();
    return true;
}

SchematicItem* FluxMeasurementItem::clone() const {
    auto* newItem = new FluxMeasurementItem(pos(), parentItem());
    newItem->setExpression(m_expression);
    newItem->setScriptFile(m_scriptFile);
    newItem->setPrefix(m_prefix);
    newItem->setUnit(m_unit);
    newItem->setResult(m_result);
    return newItem;
}

void FluxMeasurementItem::setScriptFile(const QString& path) {
    m_scriptFile = path;
    update();
}

void FluxMeasurementItem::updateSize() {
    prepareGeometryChange();
    QFontMetricsF fm(m_font);
    QString text = QString("%1: %2 %3").arg(m_prefix, m_result, m_unit).trimmed();
    QRectF rect = fm.boundingRect(text);
    
    // Add padding and space for icon
    m_size = QSizeF(rect.width() + 30, qMax(24.0, rect.height() + 10));
}

QRectF FluxMeasurementItem::boundingRect() const {
    return QRectF(0, 0, m_size.width(), m_size.height());
}

void FluxMeasurementItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Background
    QRectF rect = boundingRect();
    QColor bgColor(40, 40, 45, 200); // Dark semi-transparent
    QColor borderColor(59, 130, 246); // Blue outline
    
    if (isSelected()) {
        borderColor = QColor(16, 185, 129); // Green if selected
        bgColor = QColor(40, 50, 45, 220);
    }
    
    painter->setBrush(bgColor);
    painter->setPen(QPen(borderColor, 1.5));
    painter->drawRoundedRect(rect, 4, 4);
    
    // Icon (probe)
    painter->setPen(QPen(QColor(250, 204, 21), 2)); // Yellow
    painter->drawLine(8, rect.height() / 2 + 5, 14, rect.height() / 2 - 5);
    painter->setBrush(QColor(250, 204, 21));
    painter->drawEllipse(QPointF(14, rect.height() / 2 - 5), 2, 2);
    
    // Text
    painter->setFont(m_font);
    painter->setPen(Qt::white);
    
    QString text = QString("%1: %2 %3").arg(m_prefix, m_result, m_unit).trimmed();
    painter->drawText(QRectF(22, 0, m_size.width() - 26, m_size.height()), Qt::AlignLeft | Qt::AlignVCenter, text);
}

void FluxMeasurementItem::setExpression(const QString& expr) {
    m_expression = expr;
    updateSize();
    update();
}

void FluxMeasurementItem::setResult(const QString& res) {
    m_result = res;
    updateSize();
    update();
}

void FluxMeasurementItem::setPrefix(const QString& pre) {
    m_prefix = pre;
    updateSize();
    update();
}

void FluxMeasurementItem::setUnit(const QString& unit) {
    m_unit = unit;
    updateSize();
    update();
}

void FluxMeasurementItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    Q_UNUSED(event)
    openPropertiesDialog();
}

void FluxMeasurementItem::openPropertiesDialog() {
    QDialog dlg;
    dlg.setWindowTitle("Edit Measurement Probe");
    dlg.resize(600, 500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
    QTabWidget* tabs = new QTabWidget();
    
    // --- Tab 1: Label & Units ---
    QWidget* labelPage = new QWidget();
    QFormLayout* form = new QFormLayout(labelPage);
    QLineEdit* preEdit = new QLineEdit(m_prefix);
    QLineEdit* unitEdit = new QLineEdit(m_unit);
    form->addRow("Display Label:", preEdit);
    form->addRow("Unit Symbol:", unitEdit);
    tabs->addTab(labelPage, "Appearance");

    // --- Tab 2: Script ---
    QWidget* scriptPage = new QWidget();
    QVBoxLayout* scriptLayout = new QVBoxLayout(scriptPage);
    
    QHBoxLayout* fileLayout = new QHBoxLayout();
    QLineEdit* fileEdit = new QLineEdit(m_scriptFile);
    fileEdit->setPlaceholderText("Optional: Path to .flux file");
    QPushButton* browseBtn = new QPushButton("Browse...");
    fileLayout->addWidget(new QLabel("Source File:"));
    fileLayout->addWidget(fileEdit);
    fileLayout->addWidget(browseBtn);
    scriptLayout->addLayout(fileLayout);

    QLabel* help = new QLabel("If Source File is set, the embedded code below is ignored.");
    help->setStyleSheet("color: #fbbf24; font-size: 9pt; font-style: italic;");
    scriptLayout->addWidget(help);
    
    Flux::CodeEditor* editor = new Flux::CodeEditor(nullptr, nullptr, &dlg);
    editor->setPlainText(m_expression);
    scriptLayout->addWidget(editor, 1);
    
    tabs->addTab(scriptPage, "Calculation (FluxScript)");
    mainLayout->addWidget(tabs);

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* cancelBtn = new QPushButton("Cancel");
    QPushButton* okBtn = new QPushButton("Apply");
    okBtn->setDefault(true);
    btns->addStretch();
    btns->addWidget(cancelBtn);
    btns->addWidget(okBtn);
    mainLayout->addLayout(btns);
    
    QObject::connect(browseBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&dlg, "Select FluxScript", "", "FluxScript (*.flux)");
        if (!path.isEmpty()) fileEdit->setText(path);
    });

    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    
    if (dlg.exec() == QDialog::Accepted) {
        setPrefix(preEdit->text());
        setUnit(unitEdit->text());
        setScriptFile(fileEdit->text());
        setExpression(editor->toPlainText());
    }
}
