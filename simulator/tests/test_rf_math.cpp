#include "../core/sim_rf_math.h"
#include <QDebug>
#include <QtTest>
#include <complex>
#include <iostream>

class TestRFMath : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testYtoS() {
        // Ideal 50 ohm resistor in shunt (Y = 1/50 = 0.02)
        // For a 2-port shunt, Y11=Y22=Y12=Y21 = 0.02
        std::complex<double> y11(0.02, 0), y12(-0.02, 0), y21(-0.02, 0), y22(0.02, 0);
        
        auto s = SimRFMath::yToS(y11, y12, y21, y22, 50.0);
        
        // S11 = (1 - 2*y11*z0) / (...) ? No, let's use the standard formula.
        // For a series 50 ohm resistor, S21 should be 0.5 (linear) -> -6dB
        
        qDebug() << "S11 real:" << s.s11.real() << "imag:" << s.s11.imag();
        qDebug() << "S21 real:" << s.s21.real() << "imag:" << s.s21.imag();
        
        // S11 for 50 ohm load on 50 ohm line = 0
        // Our Y matrix describes the component.
        
        // Verification: If Y is a pi-network or T-network...
    }

    void testImpedanceConversion() {
        // Gamma = 0 -> Z = 50
        std::complex<double> g0(0, 0);
        QCOMPARE(SimRFMath::gammaToZ(g0, 50.0).real(), 50.0);
        QCOMPARE(SimRFMath::gammaToZ(g0, 50.0).imag(), 0.0);

        // Gamma = 1 (Open) -> Z = inf
        std::complex<double> gOpen(1.0, 0);
        auto zOpen = SimRFMath::gammaToZ(gOpen, 50.0);
        QVERIFY(std::abs(zOpen) > 1e9);

        // Gamma = -1 (Short) -> Z = 0
        std::complex<double> gShort(-1.0, 0);
        QCOMPARE(std::abs(SimRFMath::gammaToZ(gShort, 50.0)), 0.0);
    }

    void testStability() {
        // Case: S11=0.5, S22=0.5, S21=2.0, S12=0.1
        // Delta = S11*S22 - S12*S21 = 0.25 - 0.2 = 0.05
        // K = (1 - |S11|^2 - |S22|^2 + |Delta|^2) / (2 * |S21*S12|)
        // K = (1 - 0.25 - 0.25 + 0.0025) / (2 * 0.2) = 0.5025 / 0.4 = 1.25625
        
        std::complex<double> s11(0.5, 0), s12(0.1, 0), s21(2.0, 0), s22(0.5, 0);
        double k = SimRFMath::stabilityFactorK(s11, s12, s21, s22);
        QCOMPARE(k, 1.25625);
    }
};

QTEST_MAIN(TestRFMath)
#include "test_rf_math.moc"
