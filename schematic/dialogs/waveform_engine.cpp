#include "waveform_engine.h"
#include "../items/voltage_source_item.h"
#include "../core/sim_value_parser.h"
#include <QtMath>
#include <cmath>
#include <random>
#include <QRegularExpression>

namespace WaveformEngine {

struct Parser {
    QString s;
    int pos = 0;
    double x_val;
    bool error = false;

    double parseExpression();
    double parseTerm();
    double parseFactor();
    double parseValue();
};

double Parser::parseExpression() {
    double res = parseTerm();
    while (pos < s.length()) {
        if (s[pos] == '+') { pos++; res += parseTerm(); }
        else if (s[pos] == '-') { pos++; res -= parseTerm(); }
        else break;
    }
    return res;
}

double Parser::parseTerm() {
    double res = parseFactor();
    while (pos < s.length()) {
        if (s[pos] == '*') { pos++; res *= parseFactor(); }
        else if (s[pos] == '/') {
            pos++;
            double div = parseFactor();
            if (std::abs(div) > 1e-20) res /= div;
        }
        else break;
    }
    return res;
}

double Parser::parseFactor() {
    if (pos >= s.length()) return 0;
    if (s[pos] == '(') {
        pos++;
        double res = parseExpression();
        if (pos < s.length() && s[pos] == ')') pos++;
        return res;
    }
    return parseValue();
}

double Parser::parseValue() {
    if (pos >= s.length()) return 0;
    if (s.mid(pos, 1) == "x") { pos++; return x_val; }
    
    // Check functions
    if (s.mid(pos, 4) == "sin(") { pos += 4; double v = parseExpression(); if (pos < s.length() && s[pos] == ')') pos++; return std::sin(v); }
    if (s.mid(pos, 4) == "cos(") { pos += 4; double v = parseExpression(); if (pos < s.length() && s[pos] == ')') pos++; return std::cos(v); }
    if (s.mid(pos, 4) == "tan(") { pos += 4; double v = parseExpression(); if (pos < s.length() && s[pos] == ')') pos++; return std::tan(v); }
    if (s.mid(pos, 4) == "exp(") { pos += 4; double v = parseExpression(); if (pos < s.length() && s[pos] == ')') pos++; return std::exp(v); }
    if (s.mid(pos, 4) == "log(") { pos += 4; double v = parseExpression(); if (pos < s.length() && s[pos] == ')') pos++; return std::log(v); }
    if (s.mid(pos, 5) == "sqrt(") { pos += 5; double v = parseExpression(); if (pos < s.length() && s[pos] == ')') pos++; return std::sqrt(v); }

    int start = pos;
    if (s[pos] == '-') pos++;
    while (pos < s.length() && (s[pos].isDigit() || s[pos] == '.')) pos++;
    return s.mid(start, pos - start).toDouble();
}

QVector<QPointF> generateSine(int points) {
    QVector<QPointF> res;
    for (int i = 0; i <= points; ++i) {
        double x = (double)i / points;
        res.append(QPointF(x, std::sin(2.0 * M_PI * x)));
    }
    return res;
}

QVector<QPointF> generateSquare(double dutyCycle) {
    QVector<QPointF> res;
    res.append(QPointF(0, 1));
    res.append(QPointF(dutyCycle, 1));
    res.append(QPointF(dutyCycle, -1));
    res.append(QPointF(1, -1));
    return res;
}

QVector<QPointF> generateTriangle() {
    QVector<QPointF> res;
    res.append(QPointF(0, 0));
    res.append(QPointF(0.25, 1));
    res.append(QPointF(0.75, -1));
    res.append(QPointF(1, 0));
    return res;
}

QVector<QPointF> generateSawtooth() {
    QVector<QPointF> res;
    res.append(QPointF(0, -1));
    res.append(QPointF(1, 1));
    return res;
}

QVector<QPointF> generatePulse(double v1, double v2, double delay, double width, double riseFall) {
    QVector<QPointF> res;
    // Normalized to 0..1 range
    res.append(QPointF(0, v1));
    if (delay > 0) res.append(QPointF(delay, v1));
    res.append(QPointF(delay + riseFall, v2));
    res.append(QPointF(delay + riseFall + width, v2));
    res.append(QPointF(delay + riseFall + width + riseFall, v1));
    res.append(QPointF(1.0, v1));
    return res;
}

QVector<QPointF> generateBitstream(const QString& bits) {
    QVector<QPointF> res;
    if (bits.isEmpty()) return res;

    QVector<double> values;
    for (int i = 0; i < bits.length(); ++i) {
        if (bits[i] == '1') {
            values.append(1.0);
        } else if (bits[i] == '0') {
            values.append(0.0);
        } else if (bits[i] == '-' && i + 1 < bits.length() && bits[i+1] == '1') {
            values.append(-1.0);
            i++; // Skip the '1'
        } else if (bits[i].toLower() == 'n') {
            values.append(-1.0);
        }
    }

    if (values.isEmpty()) return res;

    double dt = 1.0 / values.length();
    for (int i = 0; i < values.length(); ++i) {
        res.append(QPointF(i * dt, values[i]));
    }
    // Final closure point at the very end
    res.append(QPointF(1.0, values.last()));

    return res;
}

double evaluateFormula(const QString& formula, double x, bool* ok) {
    Parser p;
    p.s = formula.toLower().replace(" ", "");
    p.x_val = x;
    double res = p.parseExpression();
    if (ok) *ok = !p.error;
    return res;
}

QVector<QPointF> generateFromFormula(const QString& formula, int points, bool* ok) {
    QVector<QPointF> res;
    for (int i = 0; i <= points; ++i) {
        double x = (double)i / points;
        bool evalOk = true;
        double y = evaluateFormula(formula, x, &evalOk);
        if (!evalOk) { if (ok) *ok = false; return res; }
        res.append(QPointF(x, y));
    }
    if (ok) *ok = true;
    return res;
}

void smooth(QVector<QPointF>& points) {
    if (points.size() < 3) return;
    QVector<QPointF> next;
    next.append(points.first());
    for (int i = 1; i < points.size() - 1; ++i) {
        double y = (points[i-1].y() + points[i].y() + points[i+1].y()) / 3.0;
        next.append(QPointF(points[i].x(), y));
    }
    next.append(points.last());
    points = next;
}

void addNoise(QVector<QPointF>& points, double factor) {
    static std::mt19937 gen(42);
    std::uniform_real_distribution<> dis(-factor, factor);
    for (auto& p : points) {
        p.setY(p.y() + dis(gen));
    }
}

QString convertToPwl(QVector<QPointF> points, const ExportParams& params) {
    if (points.isEmpty()) return "";
    
    // Sort by time
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x();
    });

    QStringList tokens;
    double lastT = -1e20;
    double lastY = 0;

    for (const auto& p : points) {
        double rawT = p.x() * params.period;
        double rawY = p.y() * params.amplitude + params.offset;

        if (rawT > lastT + 1e-18) {
            if (params.isStepMode && lastT >= 0) {
                // Add vertical step
                if (std::abs(rawY - lastY) > 1e-12) {
                    tokens << QString::number(rawT, 'g', 15) << QString::number(lastY, 'g', 15);
                }
            }
            tokens << QString::number(rawT, 'g', 15) << QString::number(rawY, 'g', 15);
            lastT = rawT;
            lastY = rawY;
        }
    }
    return tokens.join(' ');
}

QVector<QPointF> pointsFromVoltageSource(const VoltageSourceItem* item) {
    if (!item) return {};
    return parseSpiceFunction(item->value());
}

QVector<QPointF> parseSpiceFunction(const QString& spiceFunc) {
    QString v = spiceFunc.trimmed().toUpper();
    
    auto captureParams = [](const QString& v, const QString& func) -> QStringList {
        QRegularExpression re(func + "\\s*\\(([^\\)]*)\\)");
        auto match = re.match(v);
        if (match.hasMatch()) {
            return match.captured(1).split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        }
        return {};
    };

    if (v.contains("SINE")) {
        // Simple 1 cycle sine for preview/architecture
        return generateSine(100);
    } else if (v.contains("PULSE")) {
        QStringList p = captureParams(v, "PULSE");
        if (p.size() >= 7) {
             double period = 1.0;
             SimValueParser::parseSpiceNumber(p[6].toStdString(), period);
             double width = 0.5;
             SimValueParser::parseSpiceNumber(p[5].toStdString(), width);
             // Normalize to 0..1 scale
             return generatePulse(0, 1, 0, width / period, 0.001);
        }
        return generateSquare();
    } else if (v.contains("PWL")) {
        QStringList p = captureParams(v, "PWL");
        QVector<QPointF> pts;
        double tMax = 1e-12;
        for (int i = 0; i < p.size() - 1; i += 2) {
            double t = 0, y = 0;
            SimValueParser::parseSpiceNumber(p[i].toStdString(), t);
            SimValueParser::parseSpiceNumber(p[i+1].toStdString(), y);
            pts.append(QPointF(t, y));
            if (t > tMax) tMax = t;
        }
        // Normalize time to 0..1
        if (!pts.isEmpty() && tMax > 1e-11) {
            for (auto& pt : pts) pt.setX(pt.x() / tMax);
        }
        return pts;
    }

    return generateSine(50); // Fallback
}

} // namespace WaveformEngine
