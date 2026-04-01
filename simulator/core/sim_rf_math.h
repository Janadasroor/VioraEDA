#ifndef SIM_RF_MATH_H
#define SIM_RF_MATH_H

#include <complex>
#include <vector>
#include <array>

/**
 * @brief RF & Microwave mathematics for S, Y, Z parameter transformations.
 */
class SimRFMath {
public:
    struct SMatrix2Port {
        std::complex<double> s11, s12, s21, s22;
        
        // Helper to treat as array if needed
        std::complex<double>& operator[](int i) {
            if (i == 0) return s11;
            if (i == 1) return s12;
            if (i == 2) return s21;
            return s22;
        }
        const std::complex<double>& operator[](int i) const {
            if (i == 0) return s11;
            if (i == 1) return s12;
            if (i == 2) return s21;
            return s22;
        }
    };

    /**
     * @brief Converts Y-parameters to S-parameters.
     * @param y Matrix [y11, y12, y21, y22]
     * @param z0 Reference impedance (usually 50.0)
     */
    static SMatrix2Port yToS(const SMatrix2Port& y, double z0 = 50.0);

    /**
     * @brief Converts Y-parameters to S-parameters (individual components).
     */
    static SMatrix2Port yToS(std::complex<double> y11, std::complex<double> y12,
                             std::complex<double> y21, std::complex<double> y22, double z0 = 50.0);

    /**
     * @brief Converts S-parameters to Y-parameters.
     */
    static SMatrix2Port StoY(const SMatrix2Port& s, double z0 = 50.0);

    /**
     * @brief Converts S-parameters to Z-parameters.
     */
    static SMatrix2Port StoZ(const SMatrix2Port& s, double z0 = 50.0);

    /**
     * @brief Calculates the Reflection Coefficient (Gamma) from impedance.
     */
    static std::complex<double> impedanceToGamma(std::complex<double> z, double z0 = 50.0);

    /**
     * @brief Calculates Impedance from Reflection Coefficient (Gamma).
     */
    static std::complex<double> gammaToImpedance(std::complex<double> gamma, double z0 = 50.0);

    /**
     * @brief Alias for gammaToImpedance.
     */
    static std::complex<double> gammaToZ(std::complex<double> gamma, double z0 = 50.0) {
        return gammaToImpedance(gamma, z0);
    }

    /**
     * @brief Calculates Rollett Stability Factor (K).
     * K > 1 and Delta < 1 indicates unconditional stability.
     */
    static double stabilityFactorK(const SMatrix2Port& s);

    /**
     * @brief Calculates Rollett Stability Factor (K) from individual S-parameters.
     */
    static double stabilityFactorK(std::complex<double> s11, std::complex<double> s12,
                                   std::complex<double> s21, std::complex<double> s22);

    /**
     * @brief Calculates the determinant of the S-matrix (Delta).
     */
    static std::complex<double> delta(const SMatrix2Port& s);
};

#endif // SIM_RF_MATH_H
