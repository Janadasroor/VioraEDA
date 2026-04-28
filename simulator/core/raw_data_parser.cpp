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

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string allData = ss.str();
    file.close();

    if (allData.empty()) {
        if (error) *error = "Raw file is empty: " + path;
        return false;
    }

    const char* dataPtr = allData.data();
    const char* endPtr = dataPtr + allData.size();

    auto readLine = [&]() -> std::string {
        const char* start = dataPtr;
        while (dataPtr < endPtr && *dataPtr != '\n' && *dataPtr != '\r') {
            dataPtr++;
        }
        std::string line(start, dataPtr - start);
        if (dataPtr < endPtr && *dataPtr == '\r') dataPtr++;
        if (dataPtr < endPtr && *dataPtr == '\n') dataPtr++;
        return trim(line);
    };

    bool collectingData = false;
    bool isComplex = false;
    bool isBinaryFormat = false;
    int numVariables = 0;
    int numPoints = 0;
    std::vector<std::string> varNames;

    RawData data;

    while (dataPtr < endPtr) {
        const char* currentLineStart = dataPtr;
        std::string line = readLine();
        if (line.empty()) continue;

        if (line.rfind("No. Variables:", 0) == 0) {
            try { numVariables = std::stoi(trim(line.substr(14))); } catch (...) { numVariables = 0; }
        } else if (line.rfind("No. Points:", 0) == 0) {
            try { numPoints = std::stoi(trim(line.substr(11))); } catch (...) { numPoints = 0; }
        } else if (line.rfind("Command:", 0) == 0) {
            std::string cmd = toLower(trim(line.substr(8)));
            if (cmd.find("tran") != std::string::npos) data.analysisType = SimAnalysisType::Transient;
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
                std::string vLine = readLine();
                auto parts = tokenize(vLine);
                if (parts.size() >= 2) {
                    varNames.push_back(normalizeWaveformName(parts[1]));
                } else if (parts.size() == 1 && i == 0) {
                    varNames.push_back("time");
                }
            }
        } else if (line.rfind("Values:", 0) == 0) {
            collectingData = true;
            isBinaryFormat = false;
            break;
        } else if (line.rfind("Binary:", 0) == 0) {
            // Found binary marker. The actual data starts immediately after the newline.
            collectingData = true;
            isBinaryFormat = true;
            break;
        }
    }

    if (!collectingData || varNames.empty() || numVariables <= 0 || numVariables > 100000 || numPoints < 0 || numPoints > 100000000) {
        if (error) {
            std::ostringstream oss;
            oss << "Raw file had invalid or excessive metadata: Vars=" << numVariables << ", Points=" << numPoints;
            *error = oss.str();
        }
        return false;
    }

    data.numVariables = numVariables;
    data.numPoints = numPoints;
    data.varNames = std::move(varNames);

    try {
        if (numPoints > 0) {
            data.x.reserve(numPoints);
            data.y.resize(numVariables - 1);
            data.yPhase.resize(numVariables - 1);
            data.hasPhase.resize(numVariables - 1, false);
            for (int i = 0; i < (int)data.y.size(); ++i) data.y[i].reserve(numPoints);
            for (int i = 0; i < (int)data.yPhase.size(); ++i) data.yPhase[i].reserve(numPoints);

            // TRUST ACTUAL DATA: 
            // In shared mode or manual stops, the engine might write 'No. Points: 0' 
            // in the header but then dump actual values. We should trust the tokens.
            if (isBinaryFormat) {
                size_t doublesPerPoint = (size_t)numVariables;
                if (isComplex) doublesPerPoint *= 2;
                size_t availableBytes = endPtr - dataPtr;
                int actualPoints = (doublesPerPoint > 0) ? (int)(availableBytes / (doublesPerPoint * sizeof(double))) : 0;
                
                // If header said 0 but we have bytes, believe the bytes
                int parsePoints = (numPoints > 0) ? std::min(numPoints, actualPoints) : actualPoints;

                data.x.reserve(parsePoints);
                for (int i = 0; i < (int)data.y.size(); ++i) data.y[i].reserve(parsePoints);
                for (int i = 0; i < (int)data.yPhase.size(); ++i) data.yPhase[i].reserve(parsePoints);

                const double* ptr = reinterpret_cast<const double*>(dataPtr);
                for (int p = 0; p < parsePoints; ++p) {
                    // First double is the index (time/frequency)
                    data.x.push_back(*ptr++);

                    for (int v = 1; v < numVariables; ++v) {
                        if (isComplex) {
                            double re = *ptr++;
                            double im = *ptr++;
                            double mag = std::hypot(re, im);
                            double phase = std::atan2(im, re) * 180.0 / std::acos(-1.0);
                            data.y[v - 1].push_back(mag);
                            data.yPhase[v - 1].push_back(phase);
                            data.hasPhase[v - 1] = true;
                        } else {
                            data.y[v - 1].push_back(*ptr++);
                        }
                    }
                }
                data.numPoints = parsePoints;
            } else {
                std::vector<std::string> allTokens;
                {
                    while (dataPtr < endPtr) {
                        const char* start = dataPtr;
                        while (dataPtr < endPtr && !std::isspace(static_cast<unsigned char>(*dataPtr))) dataPtr++;
                        if (dataPtr > start) {
                            allTokens.emplace_back(start, dataPtr - start);
                        }
                        while (dataPtr < endPtr && std::isspace(static_cast<unsigned char>(*dataPtr))) dataPtr++;
                    }
                }

                size_t tokenIdx = 0;
                auto parseComplexToken = [&](const std::string& token, double& mag, double& phase, bool& isComplexVal) -> bool {
                    std::string t = token;
                    if (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
                        t = t.substr(1, t.size() - 2);
                    }

                    size_t commaPos = t.find(',');
                    if (commaPos != std::string::npos) {
                        std::string reStr = trim(t.substr(0, commaPos));
                        std::string imStr = trim(t.substr(commaPos + 1));
                        double reVal = 0.0, imVal = 0.0;
                        if (parseDouble(reStr, reVal) && parseDouble(imStr, imVal)) {
                            mag = std::hypot(reVal, imVal);
                            phase = std::atan2(imVal, reVal) * 180.0 / std::acos(-1.0);
                            isComplexVal = true;
                            return true;
                        }
                    }

                    double val;
                    if (parseDouble(t, val)) {
                        mag = val;
                        phase = 0.0;
                        isComplexVal = false;
                        return true;
                    }
                    return false;
                };

                // Belief-based looping for ASCII: consume tokens until exhausted
                int pointsParsed = 0;
                while (tokenIdx < allTokens.size()) {
                    tokenIdx++; // skip index

                    // X value
                    if (tokenIdx < allTokens.size()) {
                        std::string xTok = allTokens[tokenIdx++];
                        double xVal = 0.0;
                        size_t commaPos = xTok.find(',');
                        if (commaPos != std::string::npos) {
                            std::string reStr = trim(xTok.substr(0, commaPos));
                            parseDouble(reStr, xVal);
                        } else {
                            parseDouble(xTok, xVal);
                        }
                        data.x.push_back(xVal);
                    } else {
                        break;
                    }

                    for (int v = 1; v < numVariables; ++v) {
                        double mag = 0.0, phase = 0.0;
                        bool isComplexVal = false;
                        if (tokenIdx < allTokens.size()) {
                            parseComplexToken(allTokens[tokenIdx++], mag, phase, isComplexVal);
                        }
                        data.y[v - 1].push_back(mag);
                        data.yPhase[v - 1].push_back(phase);
                        if (isComplexVal) data.hasPhase[v - 1] = true;
                    }
                    pointsParsed++;
                    // Guard against runaway loop if file is weird
                    if (numPoints > 0 && pointsParsed >= numPoints) break;
                }
                data.numPoints = pointsParsed;
            } // end if (isBinaryFormat) ... else
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
