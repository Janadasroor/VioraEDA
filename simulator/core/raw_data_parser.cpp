#include "raw_data_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string toUpper(std::string_view s) {
    std::string out(s.size(), '\0');
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) ++start;
    auto end = s.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
    return std::string(start, end);
}

std::string toLower(std::string_view s) {
    std::string out(s.size(), '\0');
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string normalizeWaveformName(const std::string& rawName) {
    std::string name = trim(rawName);
    if (name.empty()) return rawName;

    // Handle v1#branch -> I(V1)
    size_t hashPos = name.find('#');
    if (hashPos != std::string::npos) {
        std::string before = name.substr(0, hashPos);
        return "I(" + toUpper(before) + ")";
    }

    // Handle V(net) or I(net) wrappers
    if (name.size() >= 4 && (name[1] == '(') && name.back() == ')') {
        char type = std::toupper(static_cast<unsigned char>(name[0]));
        std::string inner = toUpper(name.substr(2, name.size() - 3));
        return std::string(1, type) + "(" + inner + ")";
    }

    // Handle @ref[param] -> I(REF) for common current parameters
    if (name.size() > 3 && name[0] == '@' && name.find('[') != std::string::npos && name.back() == ']') {
        size_t openBracket = name.find('[');
        std::string ref = toUpper(name.substr(1, openBracket - 1));
        std::string param = toLower(name.substr(openBracket + 1, name.size() - openBracket - 2));
        if (param == "i" || param == "id" || param == "ic" || param == "ib" || param == "ie") {
            // For transistors, we might want to keep the suffix, but for most, I(REF) is what's probed
            if (param == "ic") return "I(" + ref + "(C))";
            if (param == "ib") return "I(" + ref + "(B))";
            if (param == "ie") return "I(" + ref + "(E))";
            return "I(" + ref + ")";
        }
    }

    return toUpper(name);
}

// Tokenize a line into whitespace-separated tokens
std::vector<std::string> tokenize(std::string_view line) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i == line.size()) break;
        size_t start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        tokens.emplace_back(line.substr(start, i - start));
    }
    return tokens;
}

bool parseDouble(std::string_view sv, double& out) {
    std::string str(sv);
    size_t pos = 0;
    try {
        out = std::stod(str, &pos);
        while (pos < str.size() && std::isspace(static_cast<unsigned char>(str[pos]))) ++pos;
        return pos == str.size();
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

bool RawDataParser::loadRawAscii(const std::string& path, RawData* out, std::string* error) {
    if (!out) return false;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (error) *error = "Could not open raw file: " + path;
        return false;
    }

    RawData data;
    std::string line;
    bool collectingData = false;
    bool isComplex = false;
    bool isBinaryFormat = false;
    int numVariables = 0;
    int numPoints = 0;
    std::vector<std::string> varNames;

    auto readHeaderLine = [&]() -> std::string {
        std::string l;
        if (std::getline(file, l)) {
            return trim(l);
        }
        return "";
    };

    while (file) {
        line = readHeaderLine();
        if (line.empty()) {
            if (file.eof()) break;
            continue;
        }

        if (line.rfind("No. Variables:", 0) == 0) {
            try { numVariables = std::stoi(trim(line.substr(14))); } catch (...) { numVariables = 0; }
        } else if (line.rfind("No. Points:", 0) == 0) {
            try { numPoints = std::stoi(trim(line.substr(11))); } catch (...) { numPoints = 0; }
        } else if (line.rfind("Command:", 0) == 0) {
            std::string cmd = toLower(trim(line.substr(8)));
            if (cmd.find("tran") != std::string::npos) data.analysisType = SimAnalysisType::Transient;
            else if (cmd.find("net") != std::string::npos || cmd.find("sp") != std::string::npos) data.analysisType = SimAnalysisType::SParameter;
            else if (cmd.find("ac") != std::string::npos) data.analysisType = SimAnalysisType::AC;
            else if (cmd.find("dc") != std::string::npos) data.analysisType = SimAnalysisType::DC;
            else if (cmd.find("op") != std::string::npos) data.analysisType = SimAnalysisType::OP;
            else if (cmd.find("noise") != std::string::npos) data.analysisType = SimAnalysisType::Noise;
        } else if (line.rfind("Title:", 0) == 0) {
            std::string title = toLower(trim(line.substr(6)));
            if (data.analysisType == SimAnalysisType::OP) {
                if (title.find("transient") != std::string::npos) data.analysisType = SimAnalysisType::Transient;
                else if (title.find("ac analysis") != std::string::npos) data.analysisType = SimAnalysisType::AC;
                else if (title.find("dc transfer") != std::string::npos) data.analysisType = SimAnalysisType::DC;
            }
        } else if (line.rfind("Flags:", 0) == 0) {
            std::string flags = toLower(line);
            if (flags.find("complex") != std::string::npos) isComplex = true;
        } else if (line.rfind("Variables:", 0) == 0) {
            for (int i = 0; i < numVariables; ++i) {
                std::string vLine = readHeaderLine();
                auto parts = tokenize(vLine);
                if (parts.size() >= 2) {
                    std::string name = normalizeWaveformName(parts[1]);
                    varNames.push_back(name);
                    
                    // Auto-detect S-Parameters based on variable names
                    if (name == "S(1,1)" || name == "S11" || name == "S(2,1)" || name == "S21") {
                        data.analysisType = SimAnalysisType::SParameter;
                    }
                } else if (parts.size() == 1 && i == 0) {
                    varNames.push_back("time");
                }
            }
        } else if (line.rfind("Values:", 0) == 0) {
            collectingData = true;
            isBinaryFormat = false;
            break;
        } else if (line.rfind("Binary:", 0) == 0) {
            collectingData = true;
            isBinaryFormat = true;
            break;
        }
    }

    if (!collectingData || varNames.empty() || numVariables <= 0 || numVariables > 100000) {
        if (error) {
            std::ostringstream oss;
            oss << "Raw file had invalid or excessive metadata: Vars=" << numVariables;
            *error = oss.str();
        }
        return false;
    }

    data.numVariables = numVariables;
    data.numPoints = numPoints;
    data.varNames = std::move(varNames);

    if (numVariables > 5000) {
        if (error) *error = "Too many variables in raw file (>5000).";
        return false;
    }

    try {
        if (numVariables > 0) {
            data.y.resize(numVariables - 1);
            data.yPhase.resize(numVariables - 1);
            data.hasPhase.resize(numVariables - 1, false);
            
            // Hard limit to 100M points to prevent OOM/mimalloc corruption
            if (numPoints > 0 && numPoints < 100000000) {
                data.x.reserve(numPoints);
                for (int i = 0; i < (int)data.y.size(); ++i) data.y[i].reserve(numPoints);
                for (int i = 0; i < (int)data.yPhase.size(); ++i) data.yPhase[i].reserve(numPoints);
            } else if (numPoints >= 100000000) {
                 if (error) *error = "Simulation results exceed 100 million points. Aborting to prevent crash.";
                 return false;
            }

            if (isBinaryFormat) {
                // Skip any trailing whitespace/newlines after "Binary:" marker
                char c;
                while (file.get(c) && (c == '\n' || c == '\r'));
                if (!file.eof()) file.unget();

                int pointsParsed = 0;
                while (file) {
                    double xVal;
                    if (!file.read(reinterpret_cast<char*>(&xVal), sizeof(double))) break;
                    
                    if (isComplex) {
                        double xValImag;
                        file.read(reinterpret_cast<char*>(&xValImag), sizeof(double));
                        // Frequency is always real, so we just use the real part.
                    }
                    data.x.push_back(xVal);

                    for (int v = 1; v < numVariables; ++v) {
                        if (isComplex) {
                            double re, im;
                            file.read(reinterpret_cast<char*>(&re), sizeof(double));
                            file.read(reinterpret_cast<char*>(&im), sizeof(double));
                            double mag = std::hypot(re, im);
                            double phase = std::atan2(im, re) * 180.0 / 3.14159265358979323846;
                            data.y[v - 1].push_back(mag);
                            data.yPhase[v - 1].push_back(phase);
                            data.hasPhase[v - 1] = true;
                        } else {
                            double val;
                            file.read(reinterpret_cast<char*>(&val), sizeof(double));
                            data.y[v - 1].push_back(val);
                        }
                    }
                    pointsParsed++;
                    if (numPoints > 0 && pointsParsed >= numPoints) break;
                }
                data.numPoints = pointsParsed;
            } else {
                int pointsParsed = 0;
                std::string token;
                
                auto parseComplex = [&](const std::string& t, double& mag, double& phase, bool& isComplexVal) {
                    std::string s = t;
                    if (s.size() >= 2 && s.front() == '(' && s.back() == ')') s = s.substr(1, s.size() - 2);
                    size_t commaPos = s.find(',');
                    if (commaPos != std::string::npos) {
                        double re = 0.0, im = 0.0;
                        parseDouble(s.substr(0, commaPos), re);
                        parseDouble(s.substr(commaPos + 1), im);
                        mag = std::hypot(re, im);
                        phase = std::atan2(im, re) * 180.0 / 3.14159265358979323846;
                        isComplexVal = true;
                    } else {
                        parseDouble(s, mag);
                        phase = 0.0;
                        isComplexVal = false;
                    }
                };

                while (file >> token) {
                    // Token is the index, skip it
                    if (!(file >> token)) break; // This is the X value
                    
                    double xVal = 0.0;
                    size_t commaPos = token.find(',');
                    if (commaPos != std::string::npos) {
                        parseDouble(token.substr(0, commaPos), xVal);
                    } else {
                        parseDouble(token, xVal);
                    }
                    data.x.push_back(xVal);

                    for (int v = 1; v < numVariables; ++v) {
                        if (!(file >> token)) break;
                        double mag = 0.0, phase = 0.0;
                        bool isComplexVal = false;
                        parseComplex(token, mag, phase, isComplexVal);
                        
                        data.y[v - 1].push_back(mag);
                        data.yPhase[v - 1].push_back(phase);
                        if (isComplexVal) data.hasPhase[v - 1] = true;
                    }
                    pointsParsed++;
                    if (numPoints > 0 && pointsParsed >= numPoints) break;
                }
                data.numPoints = pointsParsed;
            }
        }
    } catch (const std::bad_alloc&) {
        if (error) *error = "Memory allocation failed for simulation results.";
        return false;
    }

    *out = std::move(data);
    return true;
}

SimResults RawData::toSimResults() const {
    SimResults res;
    res.analysisType = analysisType;
    res.xAxisName = varNames.empty() ? "time" : varNames[0];

    if (!varNames.empty()) {
        std::string axisName = toUpper(trim(varNames[0]));
        if (axisName == "TIME" && res.analysisType == SimAnalysisType::OP && numPoints > 1) {
            res.analysisType = SimAnalysisType::Transient;
        } else if ((axisName == "FREQ" || axisName == "FREQUENCY") && res.analysisType == SimAnalysisType::OP && numPoints > 1) {
            res.analysisType = SimAnalysisType::AC;
        }
    }

    if (numPoints <= 0) return res;

    std::vector<double> stdX(x.begin(), x.end());

    for (size_t i = 1; i < varNames.size(); ++i) {
        const std::string& rawName = varNames[i];
        const std::string name = normalizeWaveformName(rawName);
        SimWaveform w;
        w.name = name;
        w.xData = stdX;

        if (i - 1 < y.size()) {
            w.yData = std::vector<double>(y[i - 1].begin(), y[i - 1].end());
            if (i - 1 < yPhase.size() && i - 1 < hasPhase.size() && hasPhase[i - 1]) {
                w.yPhase = std::vector<double>(yPhase[i - 1].begin(), yPhase[i - 1].end());
            }
        }
        res.waveforms.push_back(std::move(w));

        if (numPoints == 1 && i - 1 < y.size()) {
            double val = y[i-1].empty() ? 0.0 : y[i-1][0];
            if (name.size() > 2 && name.substr(0, 2) == "V(") {
                std::string node = name.substr(2);
                if (!node.empty() && node.back() == ')') node.pop_back();
                res.nodeVoltages[node] = val;
            } else if (name.size() > 2 && name.substr(0, 2) == "I(") {
                std::string branch = name.substr(2);
                if (!branch.empty() && branch.back() == ')') branch.pop_back();
                res.branchCurrents[branch] = val;
            }
        }
    }

    if (res.analysisType == SimAnalysisType::SParameter) {
        res.sParameterResults.resize(numPoints);
        for (int p = 0; p < numPoints; ++p) {
            res.sParameterResults[p].frequency = x[p];
        }

        for (size_t i = 1; i < varNames.size(); ++i) {
            const std::string name = normalizeWaveformName(varNames[i]);
            // Match S(1,1), S(2,1)... or S11, S21...
            bool isS11 = (name == "S(1,1)" || name == "S11");
            bool isS21 = (name == "S(2,1)" || name == "S21");
            bool isS12 = (name == "S(1,2)" || name == "S12");
            bool isS22 = (name == "S(2,2)" || name == "S22");

            if (isS11 || isS21 || isS12 || isS22) {
                for (int p = 0; p < numPoints; ++p) {
                    double mag = y[i-1][p];
                    double phase = hasPhase[i - 1] ? yPhase[i - 1][p] : 0.0;
                    std::complex<double> val = std::polar(mag, phase * (3.14159265358979323846 / 180.0));
                    if (isS11) res.sParameterResults[p].s11 = val;
                    else if (isS21) res.sParameterResults[p].s21 = val;
                    else if (isS12) res.sParameterResults[p].s12 = val;
                    else if (isS22) res.sParameterResults[p].s22 = val;
                }
            }
        }
    }

    return res;
}

SimResults::Snapshot SimResults::interpolateAt(double t) const {
    Snapshot snap;
    for (const auto& wave : waveforms) {
        if (wave.xData.empty()) continue;

        double val = 0.0;
        if (t <= wave.xData.front()) {
            val = wave.yData.front();
        } else if (t >= wave.xData.back()) {
            val = wave.yData.back();
        } else {
            auto it = std::lower_bound(wave.xData.begin(), wave.xData.end(), t);
            size_t i1 = std::distance(wave.xData.begin(), it);
            if (i1 == 0) {
                val = wave.yData.front();
            } else if (i1 >= wave.xData.size()) {
                val = wave.yData.back();
            } else {
                size_t i0 = i1 - 1;
                double x0 = wave.xData[i0];
                double x1 = wave.xData[i1];
                double y0 = wave.yData[i0];
                double y1 = wave.yData[i1];
                if (std::abs(x1 - x0) < 1e-18) {
                    val = y0;
                } else {
                    double factor = (t - x0) / (x1 - x0);
                    val = y0 + factor * (y1 - y0);
                }
            }
        }

        if (wave.name.size() > 3 && (wave.name.substr(0, 2) == "V(" || wave.name.substr(0, 2) == "v(")) {
            std::string node = wave.name.substr(2, wave.name.size() - 3);
            snap.nodeVoltages[node] = val;
        } else if (wave.name.size() > 3 && (wave.name.substr(0, 2) == "I(" || wave.name.substr(0, 2) == "i(")) {
            std::string branch = wave.name.substr(2, wave.name.size() - 3);
            snap.branchCurrents[branch] = val;
        }
    }
    return snap;
}
