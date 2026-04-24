#include "power_electronics_synthesizers.h"
#include "../core/sim_value_parser.h"
#include <QTextStream>

namespace {
    void generateDeadtimePwls(const QVector<QPointF>& points, double deadtime, QString& outHigh, QString& outLow) {
        QTextStream sh(&outHigh), sl(&outLow);
        double lastT = -1.0;
        bool currentState = false; 
        const double tr = 1e-9; // 1ns transition time for better SPICE convergence
        
        if (!points.isEmpty()) {
            currentState = points.first().y() > 0.0;
            sh << "+ 0 " << (currentState ? "1" : "0") << "\n";
            sl << "+ 0 " << (currentState ? "0" : "1") << "\n";
            lastT = 0.0;
        }
        
        for (int i = 1; i < points.size(); ++i) {
            double t = points[i].x();
            bool newState = points[i].y() > 0.0;
            if (newState != currentState) {
                if (newState) {
                    sl << "+ " << QString::number(t, 'g', 15) << " 1\n";
                    sl << "+ " << QString::number(t + tr, 'g', 15) << " 0\n";
                    sh << "+ " << QString::number(t + deadtime, 'g', 15) << " 0\n";
                    sh << "+ " << QString::number(t + deadtime + tr, 'g', 15) << " 1\n";
                } else {
                    sh << "+ " << QString::number(t, 'g', 15) << " 1\n";
                    sh << "+ " << QString::number(t + tr, 'g', 15) << " 0\n";
                    sl << "+ " << QString::number(t + deadtime, 'g', 15) << " 0\n";
                    sl << "+ " << QString::number(t + deadtime + tr, 'g', 15) << " 1\n";
                }
                currentState = newState;
            }
            lastT = t;
        }
        if (lastT >= 0) {
            sh << "+ " << QString::number(lastT + deadtime * 2, 'g', 15) << " " << (currentState ? "1" : "0") << "\n";
            sl << "+ " << QString::number(lastT + deadtime * 2, 'g', 15) << " " << (currentState ? "0" : "1") << "\n";
        }
    }
}

// === HalfBridgeSynthesizer ===

QList<ParameterInfo> HalfBridgeSynthesizer::getRequiredParameters() const {
    return {
        {"VDD", "50V", "Supply Voltage", "V"},
        {"VGATE", "15V", "Gate Drive Voltage", "V"},
        {"PERIOD", "1.0", "Time scale for 1.0 cycle of drawn waveform", "s"},
        {"DEADTIME", "100n", "Deadtime between switching", "s"},
        {"W", "10u", "NMOS Channel Width", "m"},
        {"L", "1u", "NMOS Channel Length", "m"}
    };
}

QString HalfBridgeSynthesizer::synthesizeNetlist(const QString& subcktName,
                                                 const QVector<QPointF>& points, 
                                                 const QMap<QString, QString>& params) const {
    QString out;
    QTextStream stream(&out);
    
    double deadtime = 100e-9;
    SimValueParser::parseSpiceNumber(params.value("DEADTIME", "100n").toStdString(), deadtime);
    
    double period = 1.0;
    SimValueParser::parseSpiceNumber(params.value("PERIOD", "1.0").toStdString(), period);

    // Scale points by period
    QVector<QPointF> scaledPoints = points;
    for (auto& pt : scaledPoints) pt.setX(pt.x() * period);
    
    QString pwlHigh, pwlLow;
    generateDeadtimePwls(scaledPoints, deadtime, pwlHigh, pwlLow);

    stream << ".subckt " << subcktName << " out vdd_node gnd_node\n";
    stream << "* All-NMOS Half-Bridge with Algorithmic Deadtime\n\n";

    stream << "* 1. Control Logic (Generated Conditions)\n";
    stream << "V_ctrl_high ctrl_high gnd_node PWL(\n" << pwlHigh << "+ )\n";
    stream << "R_pd1 ctrl_high gnd_node 10Meg\n";
    stream << "V_ctrl_low ctrl_low gnd_node PWL(\n" << pwlLow << "+ )\n";
    stream << "R_pd2 ctrl_low gnd_node 10Meg\n\n";

    stream << "* 2. Gate Drives (Floating & Ground-referenced)\n";
    QString vgate = params.value("VGATE", "15");
    if (vgate.endsWith("V") || vgate.endsWith("v")) vgate.chop(1);
    
    // B-sources: measure ctrl relative to local gnd_node for correct drive scaling
    // Use V(n1, n2) syntax which is most robust in ngspice subcircuits
    stream << "B_high gate_high out V=V(ctrl_high, gnd_node) * " << vgate << "\n";
    stream << "R_g_high gate_high out 10Meg\n";
    stream << "B_low gate_low gnd_node V=V(ctrl_low, gnd_node) * " << vgate << "\n";
    stream << "R_g_low gate_low gnd_node 10Meg\n\n";

    stream << "* 3. Solver Safety (Prevent Singular Matrix)\n";
    stream << "R_safe1 out 0 100G\n";
    stream << "R_safe2 gate_high 0 100G\n";
    stream << "R_safe3 gate_low 0 100G\n";
    stream << "R_safe4 ctrl_high 0 100G\n";
    stream << "R_safe5 ctrl_low 0 100G\n\n";

    stream << "* 4. Power Stage (All NMOS)\n";
    QString w = params.value("W", "10u");
    QString l = params.value("L", "1u");
    stream << "M_high vdd_node gate_high out out NMOS_GEN W=" << w << " L=" << l << "\n";
    stream << "M_low out gate_low gnd_node gnd_node NMOS_GEN W=" << w << " L=" << l << "\n\n";

    stream << ".model NMOS_GEN NMOS(Level=1 VTO=2.0 KP=50u)\n";
    stream << ".ends " << subcktName << "\n";
    
    return out;
}

// === PushPullSynthesizer ===

QList<ParameterInfo> PushPullSynthesizer::getRequiredParameters() const {
    return {
        {"VGATE", "15V", "Gate Drive Voltage", "V"},
        {"PERIOD", "1.0", "Time scale for 1 cycle", "s"},
        {"DEADTIME", "100n", "Deadtime between switching", "s"},
        {"W", "10u", "NMOS Channel Width", "m"},
        {"L", "1u", "NMOS Channel Length", "m"}
    };
}

QString PushPullSynthesizer::synthesizeNetlist(const QString& subcktName,
                                                 const QVector<QPointF>& points, 
                                                 const QMap<QString, QString>& params) const {
    QString out;
    QTextStream stream(&out);
    
    double deadtime = 100e-9;
    SimValueParser::parseSpiceNumber(params.value("DEADTIME", "100n").toStdString(), deadtime);
    
    double period = 1.0;
    SimValueParser::parseSpiceNumber(params.value("PERIOD", "1.0").toStdString(), period);

    QVector<QPointF> scaledPoints = points;
    for (auto& pt : scaledPoints) pt.setX(pt.x() * period);

    QString pwlHigh, pwlLow;
    generateDeadtimePwls(scaledPoints, deadtime, pwlHigh, pwlLow);

    stream << ".subckt " << subcktName << " out1 out2 gnd_node\n";
    stream << "* All-NMOS Push-Pull with Algorithmic Deadtime\n\n";

    stream << "* 1. Control Logic (Generated Conditions)\n";
    stream << "V_ctrl_1 ctrl_1 gnd_node PWL(\n" << pwlHigh << "+ )\n";
    stream << "R_pd1 ctrl_1 gnd_node 10Meg\n";
    stream << "V_ctrl_2 ctrl_2 gnd_node PWL(\n" << pwlLow << "+ )\n";
    stream << "R_pd2 ctrl_2 gnd_node 10Meg\n\n";

    stream << "* 2. Gate Drives\n";
    QString vgate = params.value("VGATE", "15");
    if (vgate.endsWith("V") || vgate.endsWith("v")) vgate.chop(1);
    
    stream << "E_g1 gate_1 gnd_node VALUE={ V(ctrl_1) * " << vgate << " }\n";
    stream << "R_g_1 gate_1 gnd_node 10Meg\n";
    stream << "E_g2 gate_2 gnd_node VALUE={ V(ctrl_2) * " << vgate << " }\n";
    stream << "R_g_2 gate_2 gnd_node 10Meg\n\n";

    stream << "* 3. Power Stage (NMOS pair to ground)\n";
    QString w = params.value("W", "10u");
    QString l = params.value("L", "1u");
    stream << "M_1 out1 gate_1 gnd_node gnd_node NMOS_GEN W=" << w << " L=" << l << "\n";
    stream << "M_2 out2 gate_2 gnd_node gnd_node NMOS_GEN W=" << w << " L=" << l << "\n\n";

    stream << ".model NMOS_GEN NMOS(Level=1 VTO=2.0 KP=50u)\n";
    stream << ".ends " << subcktName << "\n";
    
    return out;
}

// === MatrixSynthesizer ===

QList<ParameterInfo> MatrixSynthesizer::getRequiredParameters() const {
    return {
        {"VGATE", "15V", "Gate Drive Voltage", "V"},
        {"PERIOD", "1.0", "Time scale for 1 cycle", "s"},
        {"W", "10u", "NMOS Channel Width", "m"},
        {"L", "1u", "NMOS Channel Length", "m"}
    };
}

QString MatrixSynthesizer::synthesizeNetlist(const QString& subcktName,
                                             const QVector<QPointF>& points, 
                                             const QMap<QString, QString>& params) const {
    QString out;
    QTextStream stream(&out);

    double period = 1.0;
    SimValueParser::parseSpiceNumber(params.value("PERIOD", "1.0").toStdString(), period);
    
    QString pwl;
    QTextStream sp(&pwl);
    
    double lastT = -1.0;
    bool currentState = false; 
    if (!points.isEmpty()) {
        currentState = points.first().y() > 0.0;
        sp << "+ 0 " << (currentState ? "1" : "0") << "\n";
        lastT = 0.0;
    }
    for (int i = 1; i < points.size(); ++i) {
        double t = points[i].x() * period;
        bool newState = points[i].y() > 0.0;
        if (newState != currentState) {
            sp << "+ " << QString::number(t, 'g', 15) << " " << (currentState ? "1" : "0") << "\n";
            sp << "+ " << QString::number(t + 1e-12, 'g', 15) << " " << (newState ? "1" : "0") << "\n";
            currentState = newState;
        }
        lastT = t;
    }
    if (lastT >= 0) {
        sp << "+ " << QString::number(lastT + 1e-9, 'g', 15) << " " << (currentState ? "1" : "0") << "\n";
    }

    stream << ".subckt " << subcktName << " in_node out_node gnd_node\n";
    stream << "* Bidirectional NMOS Matrix Switch (Common Source)\n\n";

    stream << "* 1. Control Logic\n";
    stream << "V_ctrl ctrl gnd_node PWL(\n" << pwl << "+ )\n";
    stream << "R_pd ctrl gnd_node 10Meg\n\n";

    stream << "* 2. Floating Gate Drive\n";
    QString vgate = params.value("VGATE", "15");
    if (vgate.endsWith("V") || vgate.endsWith("v")) vgate.chop(1);
    
    stream << "B_gate gate common_s V=V(ctrl, gnd_node) * " << vgate << "\n";
    stream << "R_g gate common_s 10Meg\n";
    stream << "R_safe1 common_s 0 100G\n";
    stream << "R_safe2 gate 0 100G\n\n";

    stream << "* 3. Power Stage (Back-to-back NMOS)\n";
    QString w = params.value("W", "10u");
    QString l = params.value("L", "1u");
    // Matrix switch is bidirectional, sources are connected together.
    stream << "M_1 in_node gate common_s common_s NMOS_GEN W=" << w << " L=" << l << "\n";
    stream << "M_2 out_node gate common_s common_s NMOS_GEN W=" << w << " L=" << l << "\n\n";

    stream << ".model NMOS_GEN NMOS(Level=1 VTO=2.0 KP=50u)\n";
    stream << ".ends " << subcktName << "\n";
    
    return out;
}

