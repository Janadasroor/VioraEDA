#include "bv_mos_synthesizer.h"
#include <QStringList>
#include <QTextStream>

QList<ParameterInfo> BVMosSynthesizer::getRequiredParameters() const {
    return {
        {"VDD", "5V", "Supply Voltage for the MOS buffer", "V"},
        {"L", "1u", "MOSFET Channel Length", "m"},
        {"W_N", "2u", "NMOS Channel Width", "m"},
        {"W_P", "4u", "PMOS Channel Width", "m"},
        {"RON_B", "10", "BV Output Resistance (Smoothness)", "Ω"}
    };
}

QString BVMosSynthesizer::synthesizeNetlist(const QString& subcktName,
                                            const QVector<QPointF>& points, 
                                            const QMap<QString, QString>& params) const {
    QString out;
    QTextStream stream(&out);
    
    QString vdd = params.value("VDD", "5V");
    QString L = params.value("L", "1u");
    QString wN = params.value("W_N", "2u");
    QString wP = params.value("W_P", "4u");
    QString rOnB = params.value("RON_B", "10");

    stream << ".subckt " << subcktName << " out vdd_node gnd_node\n";
    stream << "* Synthesized using BV Source + MOS Buffer Topology\n\n";

    // 1. Build the PWL expression for the raw waveform
    // Instead of complex shift registers, we map the drawn points to a behavioral/PWL representation
    stream << "* 1. Raw Waveform Generation\n";
    stream << "V_raw raw_node gnd_node PWL(\n";
    for (int i = 0; i < points.size(); ++i) {
        stream << "+ " << QString::number(points[i].x(), 'g', 10) << " " << QString::number(points[i].y(), 'g', 6) << "\n";
    }
    stream << "+ )\n\n";

    // 2. Add an intermediate BV/Resistive network to smooth transitions if needed
    // This provides gradients for the solver (following best practices for SMPS/Power models)
    stream << "* 2. Behavioral smoothing\n";
    stream << "B_smooth smooth_node gnd_node V=v(raw_node)\n";
    stream << "R_smooth smooth_node gnd_node 10Meg ; High-impedance pulldown logic\n";
    stream << "R_out smooth_node gate_node " << rOnB << "\n";
    stream << "C_gate gate_node gnd_node 1p ; Gate capacitance proxy\n\n";

    // 3. Output MOS Buffer Stage
    // We instantiate generic N and P MOS models to act as the driving elements
    stream << "* 3. Output MOS Buffer\n";
    stream << "MP1 out gate_node vdd_node vdd_node PMOS_GEN W=" << wP << " L=" << L << "\n";
    stream << "MN1 out gate_node gnd_node gnd_node NMOS_GEN W=" << wN << " L=" << L << "\n\n";
    
    // We embed generic MOS models so the subcircuit is standalone
    stream << ".model NMOS_GEN NMOS(Level=1 VTO=1.0 KP=50u)\n";
    stream << ".model PMOS_GEN PMOS(Level=1 VTO=-1.0 KP=20u)\n";

    stream << ".ends " << subcktName << "\n";
    
    return out;
}

