#ifndef RAW_DATA_PARSER_H
#define RAW_DATA_PARSER_H

#include <string>
#include <vector>
#include "sim_results.h"

// Forward-declare for Qt overload
class QString;

struct RawData {
    std::vector<std::string> varNames;
    SimAnalysisType analysisType = SimAnalysisType::OP;
    int numVariables = 0;
    int numPoints = 0;
    std::vector<double> x;
    std::vector<std::vector<double>> y;
    std::vector<std::vector<double>> yPhase;
    std::vector<bool> hasPhase;

    SimResults toSimResults() const;
};

class RawDataParser {
public:
    static bool loadRawAscii(const std::string& path, RawData* out, std::string* error = nullptr);
};

#endif // RAW_DATA_PARSER_H
