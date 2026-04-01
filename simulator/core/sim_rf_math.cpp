#include "sim_rf_math.h"
#include <cmath>

using SMatrix2Port = SimRFMath::SMatrix2Port;

SMatrix2Port SimRFMath::yToS(const SMatrix2Port& Y, double z0) {
    // Normalization: y = Y * z0
    std::complex<double> y11 = Y[0] * z0;
    std::complex<double> y12 = Y[1] * z0;
    std::complex<double> y21 = Y[2] * z0;
    std::complex<double> y22 = Y[3] * z0;

    // S = (I - y) * (I + y)^-1
    // Let M = I + y
    std::complex<double> m11 = 1.0 + y11;
    std::complex<double> m12 = y12;
    std::complex<double> m21 = y21;
    std::complex<double> m22 = 1.0 + y22;

    std::complex<double> det = m11 * m22 - m12 * m21;
    if (std::abs(det) < 1e-18) return {0, 0, 0, 0};

    // Inverse of M
    std::complex<double> inv11 = m22 / det;
    std::complex<double> inv12 = -m12 / det;
    std::complex<double> inv21 = -m21 / det;
    std::complex<double> inv22 = m11 / det;

    // I - y
    std::complex<double> i_y11 = 1.0 - y11;
    std::complex<double> i_y12 = -y12;
    std::complex<double> i_y21 = -y21;
    std::complex<double> i_y22 = 1.0 - y22;

    // S = (I - y) * M_inv
    SMatrix2Port s;
    s[0] = i_y11 * inv11 + i_y12 * inv21; // s11
    s[1] = i_y11 * inv12 + i_y12 * inv22; // s12
    s[2] = i_y21 * inv11 + i_y22 * inv21; // s21
    s[3] = i_y21 * inv12 + i_y22 * inv22; // s22

    return s;
}

SMatrix2Port SimRFMath::yToS(std::complex<double> y11, std::complex<double> y12,
                             std::complex<double> y21, std::complex<double> y22, double z0) {
    return yToS({y11, y12, y21, y22}, z0);
}

SMatrix2Port SimRFMath::StoY(const SMatrix2Port& s, double z0) {
    // y = (I - S) * (I + S)^-1
    // Y = y / z0
    std::complex<double> m11 = 1.0 + s[0];
    std::complex<double> m12 = s[1];
    std::complex<double> m21 = s[2];
    std::complex<double> m22 = 1.0 + s[3];

    std::complex<double> det = m11 * m22 - m12 * m21;
    if (std::abs(det) < 1e-18) return {0, 0, 0, 0};

    std::complex<double> inv11 = m22 / det;
    std::complex<double> inv12 = -m12 / det;
    std::complex<double> inv21 = -m21 / det;
    std::complex<double> inv22 = m11 / det;

    std::complex<double> i_s11 = 1.0 - s[0];
    std::complex<double> i_s12 = -s[1];
    std::complex<double> i_s21 = -s[2];
    std::complex<double> i_s22 = 1.0 - s[3];

    SMatrix2Port y;
    y[0] = (i_s11 * inv11 + i_s12 * inv21) / z0;
    y[1] = (i_s11 * inv12 + i_s12 * inv22) / z0;
    y[2] = (i_s21 * inv11 + i_s22 * inv21) / z0;
    y[3] = (i_s21 * inv12 + i_s22 * inv22) / z0;

    return y;
}

SMatrix2Port SimRFMath::StoZ(const SMatrix2Port& s, double z0) {
    // z = (I + S) * (I - S)^-1
    // Z = z * z0
    std::complex<double> m11 = 1.0 - s[0];
    std::complex<double> m12 = -s[1];
    std::complex<double> m21 = -s[2];
    std::complex<double> m22 = 1.0 - s[3];

    std::complex<double> det = m11 * m22 - m12 * m21;
    if (std::abs(det) < 1e-18) return {0, 0, 0, 0};

    std::complex<double> inv11 = m22 / det;
    std::complex<double> inv12 = -m12 / det;
    std::complex<double> inv21 = -m21 / det;
    std::complex<double> inv22 = m11 / det;

    std::complex<double> i_s11 = 1.0 + s[0];
    std::complex<double> i_s12 = s[1];
    std::complex<double> i_s21 = s[2];
    std::complex<double> i_s22 = 1.0 + s[3];

    SMatrix2Port z;
    z[0] = (i_s11 * inv11 + i_s12 * inv21) * z0;
    z[1] = (i_s11 * inv12 + i_s12 * inv22) * z0;
    z[2] = (i_s21 * inv11 + i_s22 * inv21) * z0;
    z[3] = (i_s21 * inv12 + i_s22 * inv22) * z0;

    return z;
}

std::complex<double> SimRFMath::impedanceToGamma(std::complex<double> z, double z0) {
    return (z - z0) / (z + z0);
}

std::complex<double> SimRFMath::gammaToImpedance(std::complex<double> gamma, double z0) {
    return z0 * (1.0 + gamma) / (1.0 - gamma);
}

double SimRFMath::stabilityFactorK(const SMatrix2Port& s) {
    std::complex<double> d = delta(s);
    double den = 2.0 * std::abs(s[1] * s[2]);
    if (den == 0.0) return 999.0; // Infinite stable
    
    return (1.0 - std::pow(std::abs(s[0]), 2) - std::pow(std::abs(s[3]), 2) + std::pow(std::abs(d), 2)) / den;
}

double SimRFMath::stabilityFactorK(std::complex<double> s11, std::complex<double> s12,
                                   std::complex<double> s21, std::complex<double> s22) {
    return stabilityFactorK({s11, s12, s21, s22});
}

std::complex<double> SimRFMath::delta(const SMatrix2Port& s) {
    return s[0] * s[3] - s[1] * s[2];
}
