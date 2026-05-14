#include "simulation_setup_dialog.h"
#include "theme_manager.h"
#include "../../simulator/core/sim_value_parser.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRegularExpression>

QJsonObject SimulationSetupDialog::Config::toJson() const {
    QJsonObject obj;
    obj["version"] = 1;
    obj["analysisType"] = static_cast<int>(type);
    obj["stop"] = stop;
    obj["step"] = step;
    obj["start"] = start;
    obj["transientSteady"] = transientSteady;
    obj["steadyStateTol"] = steadyStateTol;
    obj["steadyStateDelay"] = steadyStateDelay;
    obj["fStart"] = fStart;
    obj["fStop"] = fStop;
    obj["pts"] = pts;
    obj["acSweepType"] = static_cast<int>(acSweepType);
    obj["commandText"] = commandText;
    obj["rfPort1Source"] = rfPort1Source;
    obj["rfPort2Node"] = rfPort2Node;
    obj["rfZ0"] = rfZ0;
    obj["dcSource"] = dcSource;
    return obj;
}

SimulationSetupDialog::Config SimulationSetupDialog::Config::fromJson(const QJsonObject& obj) {
    Config cfg;
    const int typeVal = obj.value("analysisType").toInt(static_cast<int>(cfg.type));
    switch (typeVal) {
        case static_cast<int>(SimAnalysisType::Transient): cfg.type = SimAnalysisType::Transient; break;
        case static_cast<int>(SimAnalysisType::OP): cfg.type = SimAnalysisType::OP; break;
        case static_cast<int>(SimAnalysisType::DC): cfg.type = SimAnalysisType::DC; break;
        case static_cast<int>(SimAnalysisType::AC): cfg.type = SimAnalysisType::AC; break;
        case static_cast<int>(SimAnalysisType::SParameter): cfg.type = SimAnalysisType::SParameter; break;
        default: cfg.type = SimAnalysisType::Transient; break;
    }
    cfg.stop = obj.value("stop").toDouble(cfg.stop);
    cfg.step = obj.value("step").toDouble(cfg.step);
    cfg.start = obj.value("start").toDouble(cfg.start);
    cfg.transientSteady = obj.value("transientSteady").toBool(cfg.transientSteady);
    cfg.steadyStateTol = obj.value("steadyStateTol").toDouble(cfg.steadyStateTol);
    cfg.steadyStateDelay = obj.value("steadyStateDelay").toDouble(cfg.steadyStateDelay);
    cfg.fStart = obj.value("fStart").toDouble(cfg.fStart);
    cfg.fStop = obj.value("fStop").toDouble(cfg.fStop);
    cfg.pts = obj.value("pts").toInt(cfg.pts);
    cfg.acSweepType = static_cast<SimAcSweepType>(obj.value("acSweepType").toInt(static_cast<int>(cfg.acSweepType)));
    cfg.commandText = obj.value("commandText").toString();
    cfg.rfPort1Source = obj.value("rfPort1Source").toString(cfg.rfPort1Source);
    cfg.rfPort2Node = obj.value("rfPort2Node").toString(cfg.rfPort2Node);
    cfg.rfZ0 = obj.value("rfZ0").toDouble(cfg.rfZ0);
    cfg.dcSource = obj.value("dcSource").toString(cfg.dcSource);
    return cfg;
}

SimulationSetupDialog::SimulationSetupDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Simulation Setup");
    setMinimumWidth(350);
    setupUI();
}

void SimulationSetupDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    
    m_formLayout = new QFormLayout();
    
    m_typeCombo = new QComboBox();
    m_typeCombo->addItems({"Transient (Time)", "DC Operating Point", "DC Sweep", "AC Sweep (Frequency)", "RF S-Parameter", "Interactive (Live)"});
    m_formLayout->addRow("Analysis Type:", m_typeCombo);

    m_acSweepType = new QComboBox();
    m_acSweepType->addItems({"Decade", "Octave", "Linear"});
    m_formLayout->addRow("Sweep Type:", m_acSweepType);

    m_param1 = new QLineEdit("10u");
    m_param2 = new QLineEdit("10m");
    m_param3 = new QLineEdit("0");
    m_param4 = new QLineEdit("V1");
    m_param5 = new QLineEdit("OUT");
    m_param6 = new QLineEdit("50");
    m_steadyCheck = new QCheckBox("Stop at steady state");
    m_steadyTolEdit = new QLineEdit();
    m_steadyDelayEdit = new QLineEdit();

    m_formLayout->addRow("Step Size:", m_param1);
    m_formLayout->addRow("Stop Time:", m_param2);
    m_formLayout->addRow("Start Time:", m_param3);
    m_formLayout->addRow("Steady State:", m_steadyCheck);
    m_formLayout->addRow("ssTol:", m_steadyTolEdit);
    m_formLayout->addRow("ssDelay:", m_steadyDelayEdit);
    m_formLayout->addRow("Port 1 Src:", m_param4);
    m_formLayout->addRow("Port 2 Node:", m_param5);
    m_formLayout->addRow("Ref Z0:", m_param6);
    m_steadyTolEdit->setPlaceholderText("0.01");
    m_steadyDelayEdit->setPlaceholderText("0");

    m_commandLine = new QLineEdit();
    m_commandLine->setStyleSheet(
        "QLineEdit { background: white; color: #3b82f6; border: 1px solid #3b82f6; "
        "font-family: 'Courier New'; font-weight: bold; padding: 4px; }");
    m_commandLine->setPlaceholderText(".tran <tstep> <tstop> [tstart]");
    m_formLayout->addRow("Command:", m_commandLine);
    m_syntaxLabel = new QLabel(this);
    m_syntaxLabel->setStyleSheet("color: #6b7280;");
    m_formLayout->addRow("Syntax:", m_syntaxLabel);

    mainLayout->addLayout(m_formLayout);

    auto* btnLayout = new QHBoxLayout();
    auto* okBtn = new QPushButton("Apply");
    okBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("Cancel");
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationSetupDialog::onAnalysisChanged);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_param1, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_param2, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_param3, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_param4, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_param5, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_param6, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_steadyCheck, &QCheckBox::toggled, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_steadyTolEdit, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_steadyDelayEdit, &QLineEdit::textChanged, this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_acSweepType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationSetupDialog::updateCommandDisplay);
    connect(m_commandLine, &QLineEdit::editingFinished, this, [this]() {
        parseCommandText(m_commandLine->text());
    });
    
    updateCommandDisplay();
}

void SimulationSetupDialog::onAnalysisChanged(int index) {
    if (!m_formLayout) return;

    auto setLabel = [&](QWidget* field, const QString& text) {
        if (auto* lbl = qobject_cast<QLabel*>(m_formLayout->labelForField(field))) {
            lbl->setText(text);
            lbl->show();
        }
        field->show();
    };

    auto hideField = [&](QWidget* field) {
        if (auto* lbl = m_formLayout->labelForField(field)) lbl->hide();
        field->hide();
    };

    if (index == 0) { // Transient
        setLabel(m_param1, "Step Size:");
        setLabel(m_param2, "Stop Time:");
        setLabel(m_param3, "Start Time:");
        setLabel(m_steadyTolEdit, "ssTol:");
        setLabel(m_steadyDelayEdit, "ssDelay:");
        hideField(m_param4); hideField(m_param5); hideField(m_param6);
        if (auto* lbl = m_formLayout->labelForField(m_steadyCheck)) lbl->show();
        m_steadyCheck->show();
        m_steadyTolEdit->show();
        m_steadyDelayEdit->show();
        m_param1->setText("10u"); m_param2->setText("10m"); m_param3->setText("0");
    } else if (index == 1) { // DC OP
        hideField(m_param1); hideField(m_param2); hideField(m_param3);
        hideField(m_param4); hideField(m_param5); hideField(m_param6);
        if (auto* lbl = m_formLayout->labelForField(m_steadyCheck)) lbl->hide();
        m_steadyCheck->hide();
        hideField(m_steadyTolEdit);
        hideField(m_steadyDelayEdit);
        m_acSweepType->hide();
        if (auto* lbl = m_formLayout->labelForField(m_acSweepType)) lbl->hide();
    } else if (index == 2) { // DC Sweep
        setLabel(m_param1, "Start:");
        setLabel(m_param2, "Stop:");
        setLabel(m_param3, "Step:");
        setLabel(m_param4, "Source:");
        m_param1->show(); m_param2->show(); m_param3->show();
        m_param4->show();
        if (auto* lbl = m_formLayout->labelForField(m_param1)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param2)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param3)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param4)) lbl->show();
        hideField(m_param5); hideField(m_param6);
        if (auto* lbl = m_formLayout->labelForField(m_steadyCheck)) lbl->hide();
        m_steadyCheck->hide();
        hideField(m_steadyTolEdit);
        hideField(m_steadyDelayEdit);
        m_acSweepType->hide();
        if (auto* lbl = m_formLayout->labelForField(m_acSweepType)) lbl->hide();
        m_param1->setText("0"); m_param2->setText("5"); m_param3->setText("0.1");
        m_param4->setText("V1");
    } else if (index == 3) { // AC
        setLabel(m_param1, "Start Freq:");
        setLabel(m_param2, "Stop Freq:");
        setLabel(m_param3, "Points:");
        m_param1->show(); m_param2->show(); m_param3->show();
        if (auto* lbl = m_formLayout->labelForField(m_param1)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param2)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param3)) lbl->show();
        hideField(m_param4); hideField(m_param5); hideField(m_param6);
        if (auto* lbl = m_formLayout->labelForField(m_steadyCheck)) lbl->hide();
        m_steadyCheck->hide();
        hideField(m_steadyTolEdit);
        hideField(m_steadyDelayEdit);
        m_acSweepType->show();
        if (auto* lbl = m_formLayout->labelForField(m_acSweepType)) lbl->show();
        m_param1->setText("10"); m_param2->setText("1meg"); m_param3->setText("100");
    } else if (index == 4) { // RF S-Parameter
        setLabel(m_param1, "Start Freq:");
        setLabel(m_param2, "Stop Freq:");
        setLabel(m_param3, "Points:");
        setLabel(m_param4, "Port 1 Src:");
        setLabel(m_param5, "Port 2 Node:");
        setLabel(m_param6, "Ref Z0:");
        m_param1->show(); m_param2->show(); m_param3->show();
        m_param4->show(); m_param5->show(); m_param6->show();
        if (auto* lbl = m_formLayout->labelForField(m_param1)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param2)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param3)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param4)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param5)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param6)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_steadyCheck)) lbl->hide();
        m_steadyCheck->hide();
        hideField(m_steadyTolEdit);
        hideField(m_steadyDelayEdit);
        m_acSweepType->show();
        if (auto* lbl = m_formLayout->labelForField(m_acSweepType)) lbl->show();
        m_param1->setText("10"); m_param2->setText("1meg"); m_param3->setText("100");
        m_param4->setText("V1"); m_param5->setText("OUT"); m_param6->setText("50");
    } else if (index == 5) { // Real-time
        setLabel(m_param1, "Update Interval (ms):");
        setLabel(m_param2, "Simulated Time Step:");
        m_param1->show(); m_param2->show();
        if (auto* lbl = m_formLayout->labelForField(m_param1)) lbl->show();
        if (auto* lbl = m_formLayout->labelForField(m_param2)) lbl->show();
        hideField(m_param3);
        hideField(m_param4); hideField(m_param5); hideField(m_param6);
        if (auto* lbl = m_formLayout->labelForField(m_steadyCheck)) lbl->hide();
        m_steadyCheck->hide();
        hideField(m_steadyTolEdit);
        hideField(m_steadyDelayEdit);
        m_acSweepType->hide();
        if (auto* lbl = m_formLayout->labelForField(m_acSweepType)) lbl->hide();
        m_param1->setText("50"); m_param2->setText("1m");
    }
    
    updateSyntaxHint();
    updateCommandDisplay();
}

void SimulationSetupDialog::updateCommandDisplay() {
    if (!m_commandLine) return;
    
    int idx = m_typeCombo->currentIndex();
    QString cmd;
    
    if (idx == 0) { // Transient
        cmd = QString(".tran %1 %2 %3")
            .arg(m_param1->text())
            .arg(m_param2->text())
            .arg(m_param3->text());
        if (m_steadyCheck->isChecked()) cmd += " steady";
    } else if (idx == 1) { // DC OP
        cmd = ".op";
    } else if (idx == 2) { // DC Sweep
        cmd = QString(".dc %1 %2 %3 %4")
            .arg(m_param4->text())
            .arg(m_param1->text())
            .arg(m_param2->text())
            .arg(m_param3->text());
    } else if (idx == 3) { // AC Sweep
        QString sweepType = "dec";
        if (m_acSweepType->currentIndex() == 1) sweepType = "oct";
        else if (m_acSweepType->currentIndex() == 2) sweepType = "lin";
        cmd = QString(".ac %1 %2 %3 %4")
            .arg(sweepType)
            .arg(m_param3->text())
            .arg(m_param1->text())
            .arg(m_param2->text());
    } else if (idx == 4) { // RF S-Parameter
        QString sweepType = "dec";
        if (m_acSweepType->currentIndex() == 1) sweepType = "oct";
        else if (m_acSweepType->currentIndex() == 2) sweepType = "lin";
        cmd = QString(".ac %1 %2 %3 %4\n.net V(%5) %6 Rin=%7 Rout=%7\n.sp %1 %2 %3 %4")
            .arg(sweepType)
            .arg(m_param3->text())
            .arg(m_param1->text())
            .arg(m_param2->text())
            .arg(m_param5->text())
            .arg(m_param4->text())
            .arg(m_param6->text());
    } else { // Real-time
        cmd = QString(".tran %1 %2 0")
            .arg(m_param2->text())
            .arg(m_param1->text());
    }
    
    m_commandLine->setText(cmd);
    updateSyntaxHint();
}

void SimulationSetupDialog::parseCommandText(const QString& command) {
    QString cmd = command.trimmed().toLower();
    
    m_typeCombo->blockSignals(true);
    m_param1->blockSignals(true);
    m_param2->blockSignals(true);
    m_param3->blockSignals(true);
    
    if (cmd.startsWith(".tran")) {
        m_typeCombo->setCurrentIndex(0);
        QStringList parts = cmd.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            m_param1->setText(parts[1]);
            m_param2->setText(parts[2]);
            if (parts.size() >= 4) m_param3->setText(parts[3]);
            m_steadyCheck->setChecked(parts.contains("steady"));
        }
    } else if (cmd.startsWith(".op")) {
        m_typeCombo->setCurrentIndex(1);
    } else if (cmd.startsWith(".dc")) {
        m_typeCombo->setCurrentIndex(2);
        QStringList parts = cmd.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 5) {
            m_param4->setText(parts[1]); // source
            m_param1->setText(parts[2]); // start
            m_param2->setText(parts[3]); // stop
            m_param3->setText(parts[4]); // step
        }
    } else if (cmd.startsWith(".ac")) {
        m_typeCombo->setCurrentIndex(3);
        QStringList lines = command.split("\n");
        QString acLine;
        QString netLine;
        for (const QString& l : lines) {
            QString tl = l.trimmed().toLower();
            if (tl.startsWith(".ac")) acLine = tl;
            else if (tl.startsWith(".net")) netLine = tl;
        }

        if (!netLine.isEmpty()) {
            m_typeCombo->setCurrentIndex(4); // S-Parameter
            // Parse .net V(out) src Rin=50 Rout=50
            QRegularExpression re(".net\\s+v\\(([^\\)]+)\\)\\s+(\\S+)(\\s+rin=(\\S+))?(\\s+rout=(\\S+))?", QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = re.match(netLine);
            if (match.hasMatch()) {
                m_param5->setText(match.captured(1)); // out
                m_param4->setText(match.captured(2)); // src
                if (!match.captured(4).isEmpty()) m_param6->setText(match.captured(4)); // z0
            }
        }

        if (!acLine.isEmpty()) {
            QStringList parts = acLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                if (parts[1] == "dec") m_acSweepType->setCurrentIndex(0);
                else if (parts[1] == "oct") m_acSweepType->setCurrentIndex(1);
                else if (parts[1] == "lin") m_acSweepType->setCurrentIndex(2);
                m_param3->setText(parts[2]);
                m_param1->setText(parts[3]);
                m_param2->setText(parts[4]);
            }
        }
    } else if (cmd.startsWith(".net")) {
        m_typeCombo->setCurrentIndex(4);
        QRegularExpression re(".net\\s+v\\(([^\\)]+)\\)\\s+(\\S+)(\\s+rin=(\\S+))?(\\s+rout=(\\S+))?", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(cmd);
        if (match.hasMatch()) {
            m_param5->setText(match.captured(1)); // out
            m_param4->setText(match.captured(2)); // src
            if (!match.captured(4).isEmpty()) m_param6->setText(match.captured(4)); // z0
        }
    }
    
    m_typeCombo->blockSignals(false);
    m_param1->blockSignals(false);
    m_param2->blockSignals(false);
    m_param3->blockSignals(false);
    
    updateSyntaxHint();
    updateCommandDisplay();
}

void SimulationSetupDialog::updateSyntaxHint() {
    if (!m_syntaxLabel || !m_typeCombo) return;
    const int idx = m_typeCombo->currentIndex();
    if (idx == 0) {
        m_syntaxLabel->setText(".tran <tstep> <tstop> [tstart] [steady]");
    } else if (idx == 1) {
        m_syntaxLabel->setText(".op");
    } else if (idx == 2) {
        m_syntaxLabel->setText(".dc <src> <vstart> <vstop> <vstep>");
    } else if (idx == 3) {
        m_syntaxLabel->setText(".ac <dec/oct/lin> <points> <fstart> <fstop>");
    } else if (idx == 4) {
        m_syntaxLabel->setText(".net V(<out>) <src> Rin=<z0> Rout=<z0>");
    } else {
        m_syntaxLabel->setText(".tran <rt_step> <update_interval> 0");
    }
}

SimulationSetupDialog::Config SimulationSetupDialog::getConfig() const {
    Config cfg;
    int idx = m_typeCombo->currentIndex();
    if (idx == 0) cfg.type = SimAnalysisType::Transient;
    else if (idx == 1) cfg.type = SimAnalysisType::OP;
    else if (idx == 2) cfg.type = SimAnalysisType::DC;
    else if (idx == 3) cfg.type = SimAnalysisType::AC;
    else if (idx == 4) cfg.type = SimAnalysisType::SParameter;
    else cfg.type = SimAnalysisType::RealTime;
    
    if (cfg.type == SimAnalysisType::Transient) {
        double parsed = 0.0;
        if (SimValueParser::parseSpiceNumber(m_param1->text().trimmed(), parsed)) cfg.step = parsed;
        if (SimValueParser::parseSpiceNumber(m_param2->text().trimmed(), parsed)) cfg.stop = parsed;
        if (SimValueParser::parseSpiceNumber(m_param3->text().trimmed(), parsed)) cfg.start = parsed;
        cfg.transientSteady = m_steadyCheck->isChecked();
        if (SimValueParser::parseSpiceNumber(m_steadyTolEdit->text().trimmed(), parsed)) cfg.steadyStateTol = parsed;
        if (SimValueParser::parseSpiceNumber(m_steadyDelayEdit->text().trimmed(), parsed)) cfg.steadyStateDelay = parsed;
    } else if (cfg.type == SimAnalysisType::DC) {
        double parsed = 0.0;
        if (SimValueParser::parseSpiceNumber(m_param1->text().trimmed(), parsed)) cfg.fStart = parsed;
        if (SimValueParser::parseSpiceNumber(m_param2->text().trimmed(), parsed)) cfg.fStop = parsed;
        if (SimValueParser::parseSpiceNumber(m_param3->text().trimmed(), parsed)) cfg.step = parsed;
        cfg.dcSource = m_param4->text().trimmed();
    } else if (cfg.type == SimAnalysisType::AC) {
        double parsed = 0.0;
        if (SimValueParser::parseSpiceNumber(m_param1->text().trimmed(), parsed)) cfg.fStart = parsed;
        if (SimValueParser::parseSpiceNumber(m_param2->text().trimmed(), parsed)) cfg.fStop = parsed;
        cfg.pts = std::max(1, m_param3->text().trimmed().toInt());
        cfg.acSweepType = static_cast<SimAcSweepType>(m_acSweepType->currentIndex());
    } else if (cfg.type == SimAnalysisType::SParameter) {
        double parsed = 0.0;
        if (SimValueParser::parseSpiceNumber(m_param1->text().trimmed(), parsed)) cfg.fStart = parsed;
        if (SimValueParser::parseSpiceNumber(m_param2->text().trimmed(), parsed)) cfg.fStop = parsed;
        cfg.acSweepType = static_cast<SimAcSweepType>(m_acSweepType->currentIndex());
        cfg.pts = std::max(1, m_param3->text().trimmed().toInt());
        cfg.rfPort1Source = m_param4->text().trimmed();
        cfg.rfPort2Node = m_param5->text().trimmed();
        if (SimValueParser::parseSpiceNumber(m_param6->text().trimmed(), parsed)) cfg.rfZ0 = parsed;
    } else if (cfg.type == SimAnalysisType::RealTime) {
        cfg.rtIntervalMs = std::max(10, m_param1->text().trimmed().toInt());
        double parsed = 0.0;
        if (SimValueParser::parseSpiceNumber(m_param2->text().trimmed(), parsed)) cfg.rtStep = parsed;
    }

    cfg.commandText = m_commandLine->text();
    return cfg;
}

void SimulationSetupDialog::setConfig(const Config& cfg) {
    if (cfg.type == SimAnalysisType::Transient) m_typeCombo->setCurrentIndex(0);
    else if (cfg.type == SimAnalysisType::OP) m_typeCombo->setCurrentIndex(1);
    else if (cfg.type == SimAnalysisType::DC) m_typeCombo->setCurrentIndex(2);
    else if (cfg.type == SimAnalysisType::AC) m_typeCombo->setCurrentIndex(3);
    else if (cfg.type == SimAnalysisType::SParameter) m_typeCombo->setCurrentIndex(4);
    else m_typeCombo->setCurrentIndex(5);

    if (cfg.type == SimAnalysisType::Transient) {
        m_param1->setText(QString::number(cfg.step, 'g', 12));
        m_param2->setText(QString::number(cfg.stop, 'g', 12));
        m_param3->setText(QString::number(cfg.start, 'g', 12));
        m_steadyCheck->setChecked(cfg.transientSteady);
        m_steadyTolEdit->setText(cfg.steadyStateTol > 0.0 ? QString::number(cfg.steadyStateTol, 'g', 12) : QString());
        m_steadyDelayEdit->setText(cfg.steadyStateDelay > 0.0 ? QString::number(cfg.steadyStateDelay, 'g', 12) : QString());
    } else if (cfg.type == SimAnalysisType::DC) {
        m_param1->setText(QString::number(cfg.fStart, 'g', 12));
        m_param2->setText(QString::number(cfg.fStop, 'g', 12));
        m_param3->setText(QString::number(cfg.step, 'g', 12));
        m_param4->setText(cfg.dcSource);
    } else if (cfg.type == SimAnalysisType::AC) {
        m_param1->setText(QString::number(cfg.fStart, 'g', 12));
        m_param2->setText(QString::number(cfg.fStop, 'g', 12));
        m_param3->setText(QString::number(cfg.pts));
        m_acSweepType->setCurrentIndex(static_cast<int>(cfg.acSweepType));
    } else if (cfg.type == SimAnalysisType::SParameter) {
        m_param1->setText(QString::number(cfg.fStart, 'g', 12));
        m_param2->setText(QString::number(cfg.fStop, 'g', 12));
        m_acSweepType->setCurrentIndex(static_cast<int>(cfg.acSweepType));
        m_param3->setText(QString::number(cfg.pts));
        m_param4->setText(cfg.rfPort1Source);
        m_param5->setText(cfg.rfPort2Node);
        m_param6->setText(QString::number(cfg.rfZ0, 'g', 12));
    } else if (cfg.type == SimAnalysisType::RealTime) {
        m_param1->setText(QString::number(cfg.rtIntervalMs));
        m_param2->setText(QString::number(cfg.rtStep, 'g', 12));
    }
    
    if (!cfg.commandText.isEmpty()) {
        m_commandLine->setText(cfg.commandText);
        parseCommandText(cfg.commandText); // Ensure UI fields are updated
    } else {
        updateCommandDisplay();
    }
}
