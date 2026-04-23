#include "ideal_switch_synthesizer.h"
#include <QTextStream>

QList<ParameterInfo> IdealSwitchSynthesizer::getRequiredParameters() const {
    return {
        {"VDD", "12V", "Supply Voltage", "V"},
        {"RON", "0.01", "Switch ON resistance", "Ω"},
        {"ROFF", "1Meg", "Switch OFF resistance", "Ω"},
        {"RT_FALL", "1n", "Signal rise/fall transition smoothing", "s"}
    };
}

QString IdealSwitchSynthesizer::synthesizeNetlist(const QString& subcktName,
                                                  const QVector<QPointF>& points, 
                                                  const QMap<QString, QString>& params) const {
    QString out;
    QTextStream stream(&out);
    
    QString vdd = params.value("VDD", "12V");
    QString ron = params.value("RON", "0.01");
    QString roff = params.value("ROFF", "1Meg");

    stream << ".subckt " << subcktName << " out vdd_node gnd_node\n";
    stream << "* Optimized Behavioral Switch Synthesis\n";
    stream << "* Uses B-Source current/voltage mapping for high convergence\n\n";

    // 1. PWL Control Signal
    stream << "* 1. Control Signal Logic\n";
    stream << "V_ctrl ctrl_node gnd_node PWL(\n";
    for (int i = 0; i < points.size(); ++i) {
        stream << "+ " << QString::number(points[i].x(), 'g', 10) << " " << QString::number(points[i].y(), 'g', 6) << "\n";
    }
    stream << "+ )\n\n";

    // 2. Linear Smoothing Stage
    stream << "* 2. Signal conditioning with 10Meg pulldown\n";
    stream << "R_pd ctrl_node gnd_node 10Meg\n";
    stream << "E_clamped gate_node gnd_node Value={LIMIT(0, V(ctrl_node), 1)}\n";
    stream << "R_gate gate_node gnd_node 10Meg\n\n";

    // 3. Behavioral Switch (Half-Bridge or Single Switch depending on topology)
    // We'll implement a Half-Bridge style similar to the MOS buffer
    stream << "* 3. Behavioral Output Stage (Smoothed Resistors)\n";
    // Upper Switch (High side) - Active when V(gate) is HIGH
    stream << "B_sw_high vdd_node out V=I(B_sw_high) * ( (" << roff << ") - ((" << roff << ")-(" << ron << ")) * V(gate_node) )\n";
    // Lower Switch (Low side) - Active when V(gate) is LOW
    stream << "B_sw_low out gnd_node V=I(B_sw_low) * ( (" << ron << ") + ((" << roff << ")-(" << ron << ")) * V(gate_node) )\n\n";

    stream << ".ends " << subcktName << "\n";
    
    return out;
}
