#include "sim_value_parser.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <cmath>

namespace {

std::string trim(std::string_view sv) {
    auto start = sv.begin();
    while (start != sv.end() && std::isspace(static_cast<unsigned char>(*start))) ++start;
    if (start == sv.end()) return {};
    auto end = sv.end();
    --end;
    while (end != start && std::isspace(static_cast<unsigned char>(*end))) --end;
    return std::string(start, end + 1 - start);
}

std::string toLower(std::string_view sv) {
    std::string out(sv.size(), '\0');
    std::transform(sv.begin(), sv.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool splitSuffix(std::string_view suffixRaw, double& factor, std::string& unit) {
    std::string suffix = toLower(trim(suffixRaw));

    // Normalize micro and ohm Unicode variants to ASCII equivalents.
    std::string normalized;
    normalized.reserve(suffix.size());
    for (size_t i = 0; i < suffix.size(); ) {
        unsigned char c = static_cast<unsigned char>(suffix[i]);
        if (c == 0xC2 && i + 1 < suffix.size() && static_cast<unsigned char>(suffix[i + 1]) == 0xB5) {
            normalized += 'u';
            i += 2;
        } else if (c == 0xCE && i + 1 < suffix.size() && static_cast<unsigned char>(suffix[i + 1]) == 0xBC) {
            normalized += 'u';
            i += 2;
        } else if (c == 0xCE && i + 1 < suffix.size() && static_cast<unsigned char>(suffix[i + 1]) == 0xA9) {
            normalized += "ohm";
            i += 2;
        } else if (c == 0xE2 && i + 2 < suffix.size() &&
                   static_cast<unsigned char>(suffix[i + 1]) == 0x84 &&
                   static_cast<unsigned char>(suffix[i + 2]) == 0xA6) {
            normalized += "ohm";
            i += 3;
        } else {
            normalized += static_cast<char>(c);
            ++i;
        }
    }
    suffix = normalized;
    factor = 1.0;
    unit = suffix;

    if (suffix.empty()) {
        return true;
    }

    // SPICE standard prefixes. Order matters: 'meg' before 'm'.
    if (suffix.rfind("meg", 0) == 0) {
        factor = 1e6;
        unit = suffix.size() > 3 ? suffix.substr(3) : std::string();
        return true;
    }
    if (suffix.rfind("mil", 0) == 0) {
        factor = 25.4e-6; // 0.001 inch
        unit = suffix.size() > 3 ? suffix.substr(3) : std::string();
        return true;
    }

    static const struct Prefix {
        char key;
        double factor;
    } prefixes[] = {
        {'t', 1e12}, {'g', 1e9}, {'k', 1e3}, {'m', 1e-3},
        {'u', 1e-6}, {'n', 1e-9}, {'p', 1e-12}, {'f', 1e-15},
    };

    for (const auto& p : prefixes) {
        if (!suffix.empty() && suffix[0] == p.key) {
            factor = p.factor;
            unit = suffix.size() > 1 ? suffix.substr(1) : std::string();
            return true;
        }
    }

    return true;
}
} // namespace

namespace SimValueParser {

bool parseSpiceNumber(const std::string& text, double& outValue) {
    std::string normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xB5) {
            normalized += 'u';
            i += 2;
        } else if (c == 0xCE && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xBC) {
            normalized += 'u';
            i += 2;
        } else if (c == 0xCE && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xA9) {
            normalized += "ohm";
            i += 2;
        } else if (c == 0xE2 && i + 2 < text.size() &&
                   static_cast<unsigned char>(text[i + 1]) == 0x84 &&
                   static_cast<unsigned char>(text[i + 2]) == 0xA6) {
            normalized += "ohm";
            i += 3;
        } else {
            normalized += static_cast<char>(c);
            ++i;
        }
    }

    size_t pos = 0;
    while (pos < normalized.size() && std::isspace(static_cast<unsigned char>(normalized[pos]))) ++pos;

    if (pos == normalized.size()) return false;

    size_t start = pos;
    if (normalized[pos] == '+' || normalized[pos] == '-') ++pos;

    bool hasDigits = false;
    while (pos < normalized.size() && std::isdigit(static_cast<unsigned char>(normalized[pos]))) {
        hasDigits = true;
        ++pos;
    }
    if (pos < normalized.size() && normalized[pos] == '.') {
        ++pos;
        while (pos < normalized.size() && std::isdigit(static_cast<unsigned char>(normalized[pos]))) {
            hasDigits = true;
            ++pos;
        }
        // Fail on multiple dots or dot followed by non-digit then another dot (e.g. 1..2)
        if (pos < normalized.size() && normalized[pos] == '.') return false;
    }
    if (!hasDigits) return false;

    if (pos < normalized.size() && (normalized[pos] == 'e' || normalized[pos] == 'E')) {
        size_t ePos = pos;
        ++pos;
        if (pos < normalized.size() && (normalized[pos] == '+' || normalized[pos] == '-')) ++pos;
        if (pos < normalized.size() && std::isdigit(static_cast<unsigned char>(normalized[pos]))) {
            while (pos < normalized.size() && std::isdigit(static_cast<unsigned char>(normalized[pos]))) ++pos;
        } else {
            // "1e" followed by non-digit must fail according to tests
            return false;
        }
    }

    std::string numStr = normalized.substr(start, pos - start);
    double base;
    try {
        base = std::stod(numStr);
    } catch (...) {
        return false;
    }

    std::string suffix;
    while (pos < normalized.size()) {
        unsigned char c = static_cast<unsigned char>(normalized[pos]);
        if (std::isalpha(c)) {
            suffix += static_cast<char>(std::tolower(c));
        } else if (std::isspace(c)) {
            // Ignore
        } else {
            // Junk: stop parsing suffix (but don't fail, e.g. "1,5" -> 1.0)
            break;
        }
        ++pos;
    }

    double factor = 1.0;
    std::string unit;
    splitSuffix(suffix, factor, unit);

    outValue = base * factor;
    return true;
}

} // namespace SimValueParser
