/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Regression tests for raw-file parser hardening (results-pipeline audit):
 *  - per-vector reserve() capped across all vectors (multi-TB alloc attempt)
 *  - binary prologue consumes exactly one line ending (0x0A/0x0D payload safe)
 *  - short trailing rows discarded whole (no column shift, no garbage)
 *  - truncated binary rows discarded (every read() status checked)
 *  - S-parameter conversion clamped to actually-parsed data
 */

#include <QtTest>
#include <QTemporaryFile>
#include <QFile>
#include <cstring>
#include <cstdint>
#include <complex>

#include "raw_data_parser.h"

namespace {

std::vector<QString>& tempPaths() {
    static std::vector<QString> paths;
    return paths;
}

QString writeTempFile(const QByteArray& content) {
    QTemporaryFile f;
    if (!f.open()) qFatal("cannot create temp file");
    f.setAutoRemove(false);
    f.write(content);
    f.close();
    tempPaths().push_back(f.fileName());
    return f.fileName();
}

void appendDouble(QByteArray& out, double v) {
    out.append(reinterpret_cast<const char*>(&v), sizeof(double));
}

double doubleFromBytes(std::uint8_t b0) {
    unsigned char bytes[8] = {b0, 0, 0, 0, 0, 0, 0, 0};
    double v = 0.0;
    std::memcpy(&v, bytes, sizeof(double));
    return v;
}

} // namespace

class TestRawDataParser : public QObject {
    Q_OBJECT
private slots:
    void asciiHappyPath();
    void asciiShortTrailingRowDiscarded();
    void binaryProloguePreserves0APayload();
    void binaryBlankLineAfterMarkerTolerated();
    void binaryTruncatedRowDiscarded();
    void hugeHeaderClaimsDoNotReserveTerabytes();
    void sparamRaggedInputClamped();
    void interpolateAtGuardsRaggedWaves();
    void cleanupTestCase() {
        for (const auto& p : tempPaths()) QFile::remove(p);
        tempPaths().clear();
    }
};

void TestRawDataParser::asciiHappyPath() {
    const QByteArray content =
        "Test happy path\n"
        "No. Variables: 2\n"
        "No. Points: 2\n"
        "Variables:\n"
        "0\ttime\ttime\n"
        "1\tv(out)\tvoltage\n"
        "Values:\n"
        "0\t0.0\t0.0\n"
        "1\t1e-3\t1.0\n";
    RawData data;
    std::string err;
    QVERIFY2(RawDataParser::loadRawAscii(writeTempFile(content).toStdString(), &data, &err),
             err.c_str());
    QCOMPARE(data.numPoints, 2);
    QCOMPARE(data.x.size(), size_t(2));
    QCOMPARE(data.y.size(), size_t(1));
    QCOMPARE(data.y[0].size(), size_t(2));
    QCOMPARE(data.x[0], 0.0);
    QCOMPARE(data.x[1], 1e-3);
    QCOMPARE(data.y[0][0], 0.0);
    QCOMPARE(data.y[0][1], 1.0);
}

void TestRawDataParser::asciiShortTrailingRowDiscarded() {
    // Claims 3 points but the third row has index+x only: it must be dropped
    // whole so the committed rows keep their columns.
    const QByteArray content =
        "Test short row\n"
        "No. Variables: 2\n"
        "No. Points: 3\n"
        "Variables:\n"
        "0\ttime\ttime\n"
        "1\tv(out)\tvoltage\n"
        "Values:\n"
        "0\t0.0\t0.0\n"
        "1\t1e-3\t1.0\n"
        "2\t2e-3\n";
    RawData data;
    std::string err;
    QVERIFY2(RawDataParser::loadRawAscii(writeTempFile(content).toStdString(), &data, &err),
             err.c_str());
    QCOMPARE(data.numPoints, 2);
    QCOMPARE(data.x.size(), size_t(2));
    QCOMPARE(data.y[0].size(), size_t(2));
    QCOMPARE(data.y[0][1], 1.0);
}

void TestRawDataParser::binaryProloguePreserves0APayload() {
    // First payload double starts with a 0x0A byte: the prologue skip must
    // consume exactly the one line ending after "Binary:", not the payload.
    const double x0 = doubleFromBytes(0x0A);
    QByteArray content =
        "Test binary prologue\n"
        "No. Variables: 2\n"
        "No. Points: 2\n"
        "Variables:\n"
        "0\ttime\ttime\n"
        "1\tv(out)\tvoltage\n"
        "Binary:\n";
    appendDouble(content, x0);
    appendDouble(content, 1.5);
    appendDouble(content, 2.0);
    appendDouble(content, 2.5);
    RawData data;
    std::string err;
    QVERIFY2(RawDataParser::loadRawAscii(writeTempFile(content).toStdString(), &data, &err),
             err.c_str());
    QCOMPARE(data.numPoints, 2);
    QCOMPARE(data.x.size(), size_t(2));
    QVERIFY2(std::memcmp(&data.x[0], &x0, sizeof(double)) == 0,
             "first payload double corrupted by prologue skip");
    QCOMPARE(data.y[0].size(), size_t(2));
    QCOMPARE(data.y[0][0], 1.5);
    QCOMPARE(data.y[0][1], 2.5);
}

void TestRawDataParser::binaryBlankLineAfterMarkerTolerated() {
    // Same payload as above but with one blank line after "Binary:" — the
    // writer variant the old skip loop tolerated must keep parsing.
    const double x0 = doubleFromBytes(0x0A);
    QByteArray content =
        "Test binary blank line\n"
        "No. Variables: 2\n"
        "No. Points: 2\n"
        "Variables:\n"
        "0\ttime\ttime\n"
        "1\tv(out)\tvoltage\n"
        "Binary:\n"
        "\n";
    appendDouble(content, x0);
    appendDouble(content, 1.5);
    appendDouble(content, 2.0);
    appendDouble(content, 2.5);
    RawData data;
    std::string err;
    QVERIFY2(RawDataParser::loadRawAscii(writeTempFile(content).toStdString(), &data, &err),
             err.c_str());
    QCOMPARE(data.numPoints, 2);
    QVERIFY2(std::memcmp(&data.x[0], &x0, sizeof(double)) == 0,
             "payload misaligned after blank line");
    QCOMPARE(data.y[0][1], 2.5);
}

void TestRawDataParser::binaryTruncatedRowDiscarded() {
    // One full row plus 8 stray bytes (half of the next x): only the full
    // row may be committed, with no garbage doubles.
    QByteArray content =
        "Test binary truncation\n"
        "No. Variables: 2\n"
        "No. Points: 2\n"
        "Variables:\n"
        "0\ttime\ttime\n"
        "1\tv(out)\tvoltage\n"
        "Binary:\n";
    appendDouble(content, 1.0);
    appendDouble(content, 10.0);
    appendDouble(content, 2.0); // orphan x of a row whose y is missing
    RawData data;
    std::string err;
    QVERIFY2(RawDataParser::loadRawAscii(writeTempFile(content).toStdString(), &data, &err),
             err.c_str());
    QCOMPARE(data.numPoints, 1);
    QCOMPARE(data.x.size(), size_t(1));
    QCOMPARE(data.y[0].size(), size_t(1));
    QCOMPARE(data.x[0], 1.0);
    QCOMPARE(data.y[0][0], 10.0);
}

void TestRawDataParser::hugeHeaderClaimsDoNotReserveTerabytes() {
    // 5000 vars x ~100M points must not attempt a multi-TB reserve; with no
    // payload the parse completes (empty) instead of failing closed.
    QByteArray content =
        "Test huge header\n"
        "No. Variables: 5000\n"
        "No. Points: 99999999\n"
        "Variables:\n";
    for (int i = 0; i < 5000; ++i) {
        content += QByteArray::number(i) + "\tv" + QByteArray::number(i) + "\tvoltage\n";
    }
    content += "Values:\n";
    RawData data;
    std::string err;
    QVERIFY2(RawDataParser::loadRawAscii(writeTempFile(content).toStdString(), &data, &err),
             err.c_str());
    QCOMPARE(data.numPoints, 0);
    QVERIFY(data.x.empty());
}

void TestRawDataParser::sparamRaggedInputClamped() {
    // Header claims 3 points but only 1 row parsed: conversion must clamp to
    // real data instead of indexing past the ragged vectors.
    RawData data;
    data.analysisType = SimAnalysisType::SParameter;
    data.varNames = {"frequency", "S11"};
    data.numVariables = 2;
    data.numPoints = 3;
    data.x = {1e6, 2e6, 3e6};
    data.y = {{0.5}};
    data.yPhase = {{0.0}};
    data.hasPhase = {true};
    const SimResults res = data.toSimResults();
    QCOMPARE(res.sParameterResults.size(), size_t(3));
    QCOMPARE(res.sParameterResults[0].s11, std::polar(0.5, 0.0));
    QCOMPARE(res.sParameterResults[1].s11, std::complex<double>(0.0, 0.0));
    QCOMPARE(res.sParameterResults[2].s11, std::complex<double>(0.0, 0.0));
}

void TestRawDataParser::interpolateAtGuardsRaggedWaves() {
    // Empty-y waves are skipped; ragged waves clamp to the common prefix
    // instead of indexing past yData.
    SimResults res;
    SimWaveform ok;
    ok.name = "V(OUT)";
    ok.xData = {0.0, 1.0, 2.0};
    ok.yData = {0.0, 10.0, 20.0};
    SimWaveform ragged;
    ragged.name = "V(RAG)";
    ragged.xData = {0.0, 1.0, 2.0};
    ragged.yData = {5.0};
    SimWaveform emptyY;
    emptyY.name = "V(EMPTY)";
    emptyY.xData = {0.0, 1.0};
    res.waveforms = {ok, ragged, emptyY};
    const SimResults::Snapshot snap = res.interpolateAt(0.5);
    QCOMPARE(snap.nodeVoltages.at("OUT"), 5.0);
    QCOMPARE(snap.nodeVoltages.at("RAG"), 5.0);
    QVERIFY(snap.nodeVoltages.find("EMPTY") == snap.nodeVoltages.end());
}

QTEST_APPLESS_MAIN(TestRawDataParser)
#include "test_raw_data_parser.moc"
