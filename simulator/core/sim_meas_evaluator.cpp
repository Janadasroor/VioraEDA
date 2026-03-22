#include "sim_meas_evaluator.h"
#include "sim_value_parser.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>
#include <limits>

namespace {

QString qFromStd(const std::string& s) { return QString::fromStdString(s); }

bool parseSpiceNum(const std::string& s, double& out) {
    return SimValueParser::parseSpiceNumber(qFromStd(s), out);
}

// Find the crossing index in a waveform where signal crosses 'value'
// starting from index 'startIdx'. Returns -1 if not found.
// 'rising': true = look for upward crossing, false = downward
int findCrossingIdx(
    const std::vector<double>& y,
    const std::vector<double>& x,
    double value,
    int startIdx,
    bool rising
) {
    for (int i = std::max(1, startIdx); i < static_cast<int>(y.size()); ++i) {
        double prev = y[i - 1];
        double curr = y[i];
        if (rising) {
            if (prev < value && curr >= value) return i;
        } else {
            if (prev > value && curr <= value) return i;
        }
    }
    return -1;
}

// Interpolate x-value at crossing point
double interpolateX(
    const std::vector<double>& x,
    const std::vector<double>& y,
    int idx,
    double value
) {
    if (idx <= 0 || idx >= static_cast<int>(x.size())) return x.empty() ? 0.0 : x[std::max(0, idx)];
    double y0 = y[idx - 1], y1 = y[idx];
    double x0 = x[idx - 1], x1 = x[idx];
    if (std::abs(y1 - y0) < 1e-30) return x0;
    double frac = (value - y0) / (y1 - y0);
    return x0 + frac * (x1 - x0);
}

} // namespace

std::string SimMeasEvaluator::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return r;
}

MeasFunction SimMeasEvaluator::parseFunction(const std::string& token) {
    std::string t = toLower(token);
    if (t == "max") return MeasFunction::MAX;
    if (t == "min") return MeasFunction::MIN;
    if (t == "pp" || t == "peak-to-peak" || t == "peak_to_peak") return MeasFunction::PP;
    if (t == "avg" || t == "average") return MeasFunction::AVG;
    if (t == "rms") return MeasFunction::RMS;
    if (t == "n") return MeasFunction::N;
    if (t == "integ" || t == "integral") return MeasFunction::INTEG;
    if (t == "min_at") return MeasFunction::MIN_AT;
    if (t == "max_at") return MeasFunction::MAX_AT;
    if (t == "first") return MeasFunction::FIRST;
    if (t == "last") return MeasFunction::LAST;
    if (t == "duty") return MeasFunction::DUTY;
    if (t == "slewrate" || t == "slew_rate") return MeasFunction::SLEWRATE;
    if (t == "slewrate_fall") return MeasFunction::SLEWRATE_FALL;
    if (t == "slewrate_rise") return MeasFunction::SLEWRATE_RISE;
    if (t == "freq" || t == "frequency") return MeasFunction::FREQ;
    if (t == "period") return MeasFunction::PERIOD;
    if (t == "trig") return MeasFunction::TRIG_TARG; // start of trig/targ
    return MeasFunction::Unknown;
}

bool SimMeasEvaluator::parseTrigger(
    const std::vector<std::string>& tokens,
    size_t& pos,
    MeasTrigger& trig
) {
    if (pos >= tokens.size()) return false;

    std::string tok = toLower(tokens[pos]);

    // Expect "val=" or "val" keyword
    if (tok == "val") {
        pos++;
        if (pos < tokens.size() && tokens[pos] == "=") pos++;
        if (pos >= tokens.size()) return false;
        if (!parseSpiceNum(tokens[pos], trig.value)) return false;
        pos++;
    } else if (tok.substr(0, 4) == "val=") {
        std::string valStr = tokens[pos].substr(4);
        if (!parseSpiceNum(valStr, trig.value)) return false;
        pos++;
    } else {
        // Direct numeric value (no "val=" keyword)
        double v = 0.0;
        if (parseSpiceNum(tokens[pos], v)) {
            trig.value = v;
            pos++;
        }
    }

    // Parse optional RISE/FALL, TD=, CROSS=
    while (pos < tokens.size()) {
        tok = toLower(tokens[pos]);
        if (tok == "rise" || tok == "rise=1" || tok == "rise=2" || tok == "rise=3") {
            trig.rising = true;
            trig.useRiseFall = true;
            if (tok.find('=') != std::string::npos) {
                int idx = std::stoi(tok.substr(tok.find('=') + 1));
                if (idx > 0) trig.index = idx;
            }
            pos++;
        } else if (tok == "fall" || tok == "fall=1" || tok == "fall=2" || tok == "fall=3") {
            trig.rising = false;
            trig.useRiseFall = true;
            if (tok.find('=') != std::string::npos) {
                int idx = std::stoi(tok.substr(tok.find('=') + 1));
                if (idx > 0) trig.index = idx;
            }
            pos++;
        } else if (tok.substr(0, 3) == "td=") {
            parseSpiceNum(tok.substr(3), trig.td);
            pos++;
        } else if (tok == "td") {
            pos++;
            if (pos < tokens.size()) {
                parseSpiceNum(tokens[pos], trig.td);
                pos++;
            }
        } else if (tok.substr(0, 6) == "cross=") {
            parseSpiceNum(tok.substr(6), trig.cross);
            pos++;
        } else if (tok == "cross") {
            pos++;
            if (pos < tokens.size()) {
                parseSpiceNum(tokens[pos], trig.cross);
                pos++;
            }
        } else {
            break;
        }
    }

    return true;
}

bool SimMeasEvaluator::parse(
    const std::string& line,
    int lineNumber,
    const std::string& sourceName,
    MeasStatement& out
) {
    // Tokenize
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);

    if (tokens.size() < 4) return false;

    size_t pos = 0;

    // Skip ".meas" / ".measur" / ".measure"
    std::string cmd = toLower(tokens[pos]);
    if (cmd == ".meas" || cmd == ".measur" || cmd == ".measure") {
        pos++;
    }
    if (pos >= tokens.size()) return false;

    out.lineNumber = lineNumber;
    out.sourceName = sourceName;

    // Analysis type: tran, ac, dc, noise, etc. (or "cur" for "current")
    out.analysisType = toLower(tokens[pos]);
    pos++;
    if (pos >= tokens.size()) return false;

    // Measurement name
    out.name = tokens[pos];
    pos++;
    if (pos >= tokens.size()) return false;

    // Function or TRIG keyword
    std::string funcTok = toLower(tokens[pos]);

    if (funcTok == "trig" || funcTok == "when") {
        // Conditional measurement: .meas <type> <name> TRIG <signal> VAL=<v> RISE=<n> TARG <signal> VAL=<v> RISE=<n>
        out.hasTrigTarg = true;
        out.function = MeasFunction::TRIG_TARG;
        pos++;

        // Parse TRIG signal
        if (pos >= tokens.size()) return false;
        out.trig.signal = tokens[pos]; // e.g. "V(in)"
        pos++;

        // Parse TRIG conditions (VAL=, RISE=, etc.)
        if (pos < tokens.size() && toLower(tokens[pos]) != "targ") {
            parseTrigger(tokens, pos, out.trig);
        }

        // Expect TARG keyword
        if (pos >= tokens.size() || toLower(tokens[pos]) != "targ") {
            // Some variants use AT instead of TARG
            if (pos < tokens.size() && toLower(tokens[pos]) == "at") {
                pos++;
            } else {
                return false;
            }
        } else {
            pos++;
        }

        // Parse TARG signal
        if (pos >= tokens.size()) return false;
        out.targ.signal = tokens[pos];
        pos++;

        // Parse TARG conditions
        if (pos < tokens.size()) {
            parseTrigger(tokens, pos, out.targ);
        }

        return true;
    }

    // Direct function form: .meas <type> <name> <func> <signal>
    MeasFunction func = parseFunction(funcTok);
    if (func == MeasFunction::Unknown) return false;
    out.function = func;
    pos++;

    if (pos >= tokens.size()) return false;

    // Signal expression (may contain parentheses like V(out) or I(V1))
    out.signal = tokens[pos];
    pos++;

    // Optional range window: FROM=<x> TO=<x> (or FROM <x> TO <x>)
    while (pos < tokens.size()) {
        const std::string t = toLower(tokens[pos]);
        if (t.rfind("from=", 0) == 0) {
            const std::string value = tokens[pos].substr(5);
            double parsed = 0.0;
            if (parseSpiceNum(value, parsed)) {
                out.hasFrom = true;
                out.from = parsed;
            }
            pos++;
            continue;
        }
        if (t == "from") {
            pos++;
            if (pos < tokens.size()) {
                double parsed = 0.0;
                if (parseSpiceNum(tokens[pos], parsed)) {
                    out.hasFrom = true;
                    out.from = parsed;
                }
                pos++;
            }
            continue;
        }
        if (t.rfind("to=", 0) == 0) {
            const std::string value = tokens[pos].substr(3);
            double parsed = 0.0;
            if (parseSpiceNum(value, parsed)) {
                out.hasTo = true;
                out.to = parsed;
            }
            pos++;
            continue;
        }
        if (t == "to") {
            pos++;
            if (pos < tokens.size()) {
                double parsed = 0.0;
                if (parseSpiceNum(tokens[pos], parsed)) {
                    out.hasTo = true;
                    out.to = parsed;
                }
                pos++;
            }
            continue;
        }
        pos++;
    }

    return true;
}

const SimWaveform* SimMeasEvaluator::findWaveform(
    const SimResults& results,
    const std::string& signal
) {
    if (signal.empty() || results.waveforms.empty()) return nullptr;

    std::string s = signal;
    // Strip outer parens if present: V(out) -> V(out), out -> V(out)
    // Also handle I(V1) format

    for (const auto& w : results.waveforms) {
        std::string wn = w.name;
        // Exact match
        if (wn == s) return &w;
        // Case-insensitive match
        std::string wnl = toLower(wn);
        std::string sl = toLower(s);
        if (wnl == sl) return &w;

        // Bare name matching V(name)
        if (wn.length() > 2 && wn[0] == 'V' && wn[1] == '(' && wn.back() == ')') {
            std::string bare = wn.substr(2, wn.length() - 3);
            if (toLower(bare) == sl || bare == s) return &w;
        }
        // Try with V() wrapper
        std::string wrapped = "V(" + s + ")";
        if (toLower(wn) == toLower(wrapped)) return &w;
    }
    return nullptr;
}

double SimMeasEvaluator::evalMAX(const SimWaveform& w) {
    if (w.yData.empty()) return 0.0;
    return *std::max_element(w.yData.begin(), w.yData.end());
}

double SimMeasEvaluator::evalMIN(const SimWaveform& w) {
    if (w.yData.empty()) return 0.0;
    return *std::min_element(w.yData.begin(), w.yData.end());
}

double SimMeasEvaluator::evalPP(const SimWaveform& w) {
    if (w.yData.empty()) return 0.0;
    auto [mn, mx] = std::minmax_element(w.yData.begin(), w.yData.end());
    return *mx - *mn;
}

double SimMeasEvaluator::evalAVG(const SimWaveform& w) {
    if (w.yData.empty()) return 0.0;
    double sum = 0.0;
    for (double v : w.yData) sum += v;
    return sum / static_cast<double>(w.yData.size());
}

double SimMeasEvaluator::evalRMS(const SimWaveform& w) {
    if (w.yData.empty()) return 0.0;
    double sumSq = 0.0;
    for (double v : w.yData) sumSq += v * v;
    return std::sqrt(sumSq / static_cast<double>(w.yData.size()));
}

double SimMeasEvaluator::evalINTEG(const SimWaveform& w) {
    if (w.xData.size() < 2 || w.yData.size() < 2) return 0.0;
    double integral = 0.0;
    size_t n = std::min(w.xData.size(), w.yData.size());
    for (size_t i = 1; i < n; ++i) {
        double dt = w.xData[i] - w.xData[i - 1];
        integral += 0.5 * (w.yData[i] + w.yData[i - 1]) * dt;
    }
    return integral;
}

double SimMeasEvaluator::evalFIRST(const SimWaveform& w) {
    return w.yData.empty() ? 0.0 : w.yData.front();
}

double SimMeasEvaluator::evalLAST(const SimWaveform& w) {
    return w.yData.empty() ? 0.0 : w.yData.back();
}

bool SimMeasEvaluator::findCrossingTime(
    const SimWaveform& w,
    const MeasTrigger& trigger,
    double& outTime
) {
    if (w.xData.empty() || w.yData.empty()) return false;
    if (w.xData.size() != w.yData.size()) return false;

    int startIdx = 0;
    // Apply time delay
    if (trigger.td > 0.0) {
        for (size_t i = 0; i < w.xData.size(); ++i) {
            if (w.xData[i] >= trigger.td) {
                startIdx = static_cast<int>(i);
                break;
            }
        }
    }

    bool rising = trigger.rising;
    int count = 0;
    int targetIdx = trigger.useRiseFall ? trigger.index : 1;
    int searchFrom = startIdx;

    while (searchFrom < static_cast<int>(w.yData.size())) {
        int idx = findCrossingIdx(w.yData, w.xData, trigger.value, searchFrom, rising);
        if (idx < 0) return false;
        count++;
        if (count >= targetIdx) {
            outTime = interpolateX(w.xData, w.yData, idx, trigger.value);
            return true;
        }
        searchFrom = idx + 1;
    }

    return false;
}

std::vector<MeasResult> SimMeasEvaluator::evaluate(
    const std::vector<MeasStatement>& statements,
    const SimResults& results,
    const std::string& analysisType
) {
    std::vector<MeasResult> results_out;
    std::string atype = toLower(analysisType);

    for (const auto& stmt : statements) {
        if (toLower(stmt.analysisType) != atype) continue;

        MeasResult mr;
        mr.name = stmt.name;

        if (stmt.function == MeasFunction::TRIG_TARG) {
            // Conditional measurement: find TARG time minus TRIG time
            const SimWaveform* trigW = findWaveform(results, stmt.trig.signal);
            const SimWaveform* targW = findWaveform(results, stmt.targ.signal);

            if (!trigW) {
                mr.error = "TRIG signal not found: " + stmt.trig.signal;
                results_out.push_back(mr);
                continue;
            }
            if (!targW) {
                mr.error = "TARG signal not found: " + stmt.targ.signal;
                results_out.push_back(mr);
                continue;
            }

            double trigTime = 0.0, targTime = 0.0;
            if (!findCrossingTime(*trigW, stmt.trig, trigTime)) {
                mr.error = "TRIG crossing not found";
                results_out.push_back(mr);
                continue;
            }
            if (!findCrossingTime(*targW, stmt.targ, targTime)) {
                mr.error = "TARG crossing not found";
                results_out.push_back(mr);
                continue;
            }

            mr.value = targTime - trigTime;
            mr.valid = true;
            results_out.push_back(mr);
            continue;
        }

        // Direct function evaluation
        const SimWaveform* w = findWaveform(results, stmt.signal);
        if (!w) {
            mr.error = "Signal not found: " + stmt.signal;
            results_out.push_back(mr);
            continue;
        }

        SimWaveform view = *w;
        if (stmt.hasFrom || stmt.hasTo) {
            const double lo = stmt.hasFrom ? stmt.from : -std::numeric_limits<double>::infinity();
            const double hi = stmt.hasTo ? stmt.to : std::numeric_limits<double>::infinity();
            std::vector<double> xWin;
            std::vector<double> yWin;
            const size_t n = std::min(view.xData.size(), view.yData.size());
            xWin.reserve(n);
            yWin.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                const double x = view.xData[i];
                if (x < lo || x > hi) continue;
                xWin.push_back(x);
                yWin.push_back(view.yData[i]);
            }
            view.xData.swap(xWin);
            view.yData.swap(yWin);
            if (view.xData.empty() || view.yData.empty()) {
                mr.error = "No points in requested FROM/TO window";
                results_out.push_back(mr);
                continue;
            }
        }

        switch (stmt.function) {
            case MeasFunction::MAX:
                mr.value = evalMAX(view);
                mr.valid = true;
                break;
            case MeasFunction::MIN:
                mr.value = evalMIN(view);
                mr.valid = true;
                break;
            case MeasFunction::PP:
                mr.value = evalPP(view);
                mr.valid = true;
                break;
            case MeasFunction::AVG:
                mr.value = evalAVG(view);
                mr.valid = true;
                break;
            case MeasFunction::RMS:
                mr.value = evalRMS(view);
                mr.valid = true;
                break;
            case MeasFunction::N:
                mr.value = static_cast<double>(view.yData.size());
                mr.valid = true;
                break;
            case MeasFunction::INTEG:
                mr.value = evalINTEG(view);
                mr.valid = true;
                break;
            case MeasFunction::FIRST:
                mr.value = evalFIRST(view);
                mr.valid = true;
                break;
            case MeasFunction::LAST:
                mr.value = evalLAST(view);
                mr.valid = true;
                break;
            case MeasFunction::MIN_AT: {
                if (view.yData.empty() || view.xData.empty()) {
                    mr.error = "Empty waveform";
                    break;
                }
                auto mnIt = std::min_element(view.yData.begin(), view.yData.end());
                size_t idx = static_cast<size_t>(std::distance(view.yData.begin(), mnIt));
                mr.value = view.xData[idx];
                mr.valid = true;
                break;
            }
            case MeasFunction::MAX_AT: {
                if (view.yData.empty() || view.xData.empty()) {
                    mr.error = "Empty waveform";
                    break;
                }
                auto mxIt = std::max_element(view.yData.begin(), view.yData.end());
                size_t idx = static_cast<size_t>(std::distance(view.yData.begin(), mxIt));
                mr.value = view.xData[idx];
                mr.valid = true;
                break;
            }
            case MeasFunction::FREQ: {
                if (view.xData.size() < 2 || view.yData.empty()) {
                    mr.error = "Insufficient data for FREQ";
                    break;
                }
                // Estimate frequency by counting zero crossings (rising)
                double midY = evalAVG(view);
                int crossings = 0;
                for (size_t i = 1; i < view.yData.size(); ++i) {
                    if (view.yData[i - 1] < midY && view.yData[i] >= midY) crossings++;
                }
                double totalTime = view.xData.back() - view.xData.front();
                if (totalTime > 0 && crossings > 0) {
                    mr.value = static_cast<double>(crossings) / totalTime;
                    mr.valid = true;
                } else {
                    mr.error = "No crossings found";
                }
                break;
            }
            case MeasFunction::PERIOD: {
                if (view.xData.size() < 2 || view.yData.empty()) {
                    mr.error = "Insufficient data for PERIOD";
                    break;
                }
                double midY = evalAVG(view);
                double firstCross = 0.0, secondCross = 0.0;
                int found = 0;
                for (size_t i = 1; i < view.yData.size(); ++i) {
                    if (view.yData[i - 1] < midY && view.yData[i] >= midY) {
                        double t = interpolateX(view.xData, view.yData, static_cast<int>(i), midY);
                        if (found == 0) firstCross = t;
                        else if (found == 1) { secondCross = t; break; }
                        found++;
                    }
                }
                if (found >= 2) {
                    mr.value = secondCross - firstCross;
                    mr.valid = true;
                } else {
                    mr.error = "Could not find two periods";
                }
                break;
            }
            case MeasFunction::DUTY: {
                if (view.xData.size() < 2 || view.yData.empty()) {
                    mr.error = "Insufficient data for DUTY";
                    break;
                }
                double midY = evalAVG(view);
                double highTime = 0.0;
                for (size_t i = 1; i < view.yData.size(); ++i) {
                    if (view.yData[i] >= midY) {
                        highTime += view.xData[i] - view.xData[i - 1];
                    }
                }
                double totalTime = view.xData.back() - view.xData.front();
                if (totalTime > 0) {
                    mr.value = highTime / totalTime;
                    mr.valid = true;
                } else {
                    mr.error = "Zero time span";
                }
                break;
            }
            case MeasFunction::SLEWRATE:
            case MeasFunction::SLEWRATE_RISE:
            case MeasFunction::SLEWRATE_FALL: {
                if (view.xData.size() < 2 || view.yData.empty()) {
                    mr.error = "Insufficient data for SLEWRATE";
                    break;
                }
                double maxSlew = 0.0;
                for (size_t i = 1; i < std::min(view.xData.size(), view.yData.size()); ++i) {
                    double dt = view.xData[i] - view.xData[i - 1];
                    if (dt <= 0) continue;
                    double slew = (view.yData[i] - view.yData[i - 1]) / dt;
                    if (stmt.function == MeasFunction::SLEWRATE) {
                        if (std::abs(slew) > std::abs(maxSlew)) maxSlew = slew;
                    } else if (stmt.function == MeasFunction::SLEWRATE_RISE) {
                        if (slew > maxSlew) maxSlew = slew;
                    } else {
                        if (slew < maxSlew) maxSlew = slew;
                    }
                }
                mr.value = maxSlew;
                mr.valid = true;
                break;
            }
            default:
                mr.error = "Unsupported function";
                break;
        }

        results_out.push_back(mr);
    }

    return results_out;
}
