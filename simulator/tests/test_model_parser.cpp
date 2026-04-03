#include "../core/sim_model_parser.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void testModelInheritance() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string lib =
        ".model BASE NMOS (KP=100u VTO=1.0)\n"
        ".model FAST BASE (KP=200u)\n";

    const bool ok = SimModelParser::parseLibrary(netlist, lib, SimModelParseOptions(), &diags);
    require(ok, "parseLibrary failed for inheritance case");

    const SimModel* base = netlist.findModel("BASE");
    const SimModel* fast = netlist.findModel("FAST");
    require(base != nullptr, "BASE model missing");
    require(fast != nullptr, "FAST model missing");
    require(fast->type == SimComponentType::MOSFET_NMOS, "FAST type did not inherit NMOS");
    require(fast->params.count("KP") != 0, "FAST KP missing");
    require(fast->params.count("VTO") != 0, "FAST VTO missing");
    require(fast->params.at("KP") > base->params.at("KP"), "FAST KP override did not apply");
}

void testLibSectionFilter() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string content =
        ".lib TT\n"
        ".model MTT NMOS (KP=100u)\n"
        ".endl\n"
        ".lib FF\n"
        ".model MFF NMOS (KP=200u)\n"
        ".endl\n";

    SimModelParseOptions options;
    options.activeLibSection = "FF";
    const bool ok = SimModelParser::parseLibrary(netlist, content, options, &diags);
    require(ok, "parseLibrary failed for .lib section filtering");
    require(netlist.findModel("MFF") != nullptr, "filtered section model MFF missing");
    require(netlist.findModel("MTT") == nullptr, "unexpected model from skipped section");
}

void testIncludeResolverAndDiagnostics() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;

    SimModelParseOptions options;
    options.sourceName = "root.lib";
    options.includeResolver = [](const std::string& path, std::string& outContent) -> bool {
        if (path == "child.lib") {
            outContent = ".model CHILD D (IS=1e-14 N=1.2)\n";
            return true;
        }
        return false;
    };

    const std::string content =
        ".include child.lib\n"
        ".model BROKEN UNKNOWN_TYPE (KP=1)\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, options, &diags);
    require(ok, "non-strict parseLibrary should not hard fail");
    require(netlist.findModel("CHILD") != nullptr, "included child model missing");

    bool sawBrokenDiag = false;
    for (const auto& d : diags) {
        if (d.line == 2 && (d.severity == SimParseDiagnosticSeverity::Error || d.severity == SimParseDiagnosticSeverity::Warning) &&
            d.message.find("unknown model type or base model") != std::string::npos) {
            sawBrokenDiag = true;
            break;
        }
    }
    
    if (!sawBrokenDiag) {
        std::cerr << "Diagnostics found:" << std::endl;
        for (const auto& d : diags) {
            std::cerr << "  Line " << d.line << " [" << (int)d.severity << "]: " << d.message << " (Text: " << d.text << ")" << std::endl;
        }
    }
    require(sawBrokenDiag, "expected line-based diagnostic for broken .model");
}

void testSubcktNamedNodeMapping() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;

    const std::string content =
        ".subckt INV IN OUT VDD VSS\n"
        "R1 OUT IN 1k\n"
        ".ends\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, SimModelParseOptions(), &diags);
    require(ok, "subckt parse failed");

    const SimSubcircuit* inv = netlist.findSubcircuit("INV");
    require(inv != nullptr, "subcircuit INV missing");
    require(inv->components.size() == 1, "unexpected subcircuit component count");
    require(inv->components[0].nodes.size() == 2, "subcircuit node mapping size mismatch");
    require(inv->components[0].nodes[0] == 2, "OUT pin did not map to pin index 2");
    require(inv->components[0].nodes[1] == 1, "IN pin did not map to pin index 1");
}

void testCSWParsing() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string content =
        ".model MyCSW CSW (IT=0.5 IH=0.1 RON=1 ROFF=1Meg)\n"
        "W1 1 2 VCTRL MyCSW\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, SimModelParseOptions(), &diags);
    require(ok, "parseLibrary failed for CSW case");

    const SimModel* csw = netlist.findModel("MyCSW");
    require(csw != nullptr, "MyCSW model missing");
    require(csw->type == SimComponentType::CSW, "MyCSW type is not CSW");
    require(csw->params.at("IT") == 0.5, "IT parameter mismatch");
    require(csw->params.at("IH") == 0.1, "IH parameter mismatch");
    require(csw->params.at("RON") == 1.0, "RON parameter mismatch");
    require(csw->params.at("ROFF") == 1e6, "ROFF parameter mismatch");

    // Check primitive parsing
    // In a flat netlist, W1 should be found if it was in a subcircuit, 
    // but here we are parsing into a "library" netlist which usually stores models and subcircuits.
    // However, if it's top-level primitives, they might be skipped or stored depending on implementation.
    // Let's check if the parser handles W records.
}

void testJfetModelParsing() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string content =
        ".model JN_A NJF (BETA=1m VTO=-2 LAMBDA=0.02)\n"
        ".model JP_A PJF (BETA=800u VTO=2 LAMBDA=0.03)\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, SimModelParseOptions(), &diags);
    require(ok, "parseLibrary failed for JFET model case");

    const SimModel* njf = netlist.findModel("JN_A");
    const SimModel* pjf = netlist.findModel("JP_A");
    require(njf != nullptr, "NJF model missing");
    require(pjf != nullptr, "PJF model missing");
    require(njf->type == SimComponentType::JFET_NJF, "NJF type parse mismatch");
    require(pjf->type == SimComponentType::JFET_PJF, "PJF type parse mismatch");
}

void testVdmosModelParsing() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string content =
        ".model MN_A VDMOS (VTO=2 KP=100u)\n"
        ".model MP_A VDMOS (pchan VTO=-2 KP=80u)\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, SimModelParseOptions(), &diags);
    require(ok, "parseLibrary failed for VDMOS model case");

    const SimModel* mn = netlist.findModel("MN_A");
    const SimModel* mp = netlist.findModel("MP_A");
    require(mn != nullptr, "VDMOS NMOS model missing");
    require(mp != nullptr, "VDMOS PMOS model missing");
    require(mn->type == SimComponentType::MOSFET_NMOS, "VDMOS default type parse mismatch");
    require(mp->type == SimComponentType::MOSFET_PMOS, "VDMOS pchan type parse mismatch");
}

void testNonAsciiAndXSpiceParsing() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    // content with non-ASCII (Japanese Shift-JIS style) and XSPICE and [ lines
    const std::string content =
        "* \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8a\xca\x82\xcc\x83\x52\x83\x81\x83\x93\x83\x67\x82\xc5\x82\xb7\n"
        ".model Q1 NPN (BF=100)\n"
        "[PART]\n"
        "NAME=SomeMetadata\n"
        ".model D1 D_AND (vlow=0 vhigh=5)\n"
        ".model D2 DFF (clk_delay=1n)\n";

    SimModelParseOptions options;
    options.sourceName = "test_mixed.lib";
    const bool ok = SimModelParser::parseLibrary(netlist, content, options, &diags);
    
    require(ok, "parseLibrary failed for non-ASCII/mixed content");
    require(netlist.findModel("Q1") != nullptr, "Q1 model missing");
    require(netlist.findModel("D1") != nullptr, "D_AND model missing");
    require(netlist.findModel("D2") != nullptr, "DFF model missing");
    
    for (const auto& d : diags) {
        if (d.severity == SimParseDiagnosticSeverity::Warning && 
            d.message.find("unsupported primitive") != std::string::npos &&
            d.text.find("[PART]") != std::string::npos) {
            throw std::runtime_error("Found warning for [PART] which should be ignored");
        }
        if (d.severity == SimParseDiagnosticSeverity::Warning &&
            d.message.find("unknown model type") != std::string::npos) {
            throw std::runtime_error("Found warning for unknown model type (XSPICE should be recognized): " + d.text);
        }
    }
}

void testBsim4MultilineModelParsing() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string content =
        ".model MY_BSIM4 NMOS (LEVEL=54\n"
        "+ TNOM=27 TOX=2.1e-9 VTH0=0.35\n"
        "+ K1=0.45 U0=280 LINT=1e-8\n"
        "+ RDSW=150 NF=1 KF=1e-24 AF=1)\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, SimModelParseOptions(), &diags);
    require(ok, "parseLibrary failed for multiline BSIM4 card");

    const SimModel* bsim4 = netlist.findModel("MY_BSIM4");
    require(bsim4 != nullptr, "MY_BSIM4 model missing");
    require(bsim4->type == SimComponentType::MOSFET_NMOS, "MY_BSIM4 type parse mismatch");
    require(bsim4->params.count("LEVEL") != 0 && bsim4->params.at("LEVEL") == 54.0, "LEVEL parse mismatch");
    require(bsim4->params.count("TOX") != 0 && bsim4->params.at("TOX") == 2.1e-9, "TOX parse mismatch");
    require(bsim4->params.count("KF") != 0 && bsim4->params.at("KF") == 1e-24, "KF parse mismatch");
    require(bsim4->params.count("AF") != 0 && bsim4->params.at("AF") == 1.0, "AF parse mismatch");
}

void testMutualInductorCardIsAccepted() {
    SimNetlist netlist;
    std::vector<SimParseDiagnostic> diags;
    const std::string content =
        "L1 in 0 10u\n"
        "L2 out 0 22u\n"
        "K1 L1 L2 0.99\n";

    const bool ok = SimModelParser::parseLibrary(netlist, content, SimModelParseOptions(), &diags);
    require(ok, "parseLibrary failed for mutual inductor card");
    require(netlist.components().size() == 2, "inductor primitives should still be parsed");

    for (const auto& d : diags) {
        if (d.message.find("unsupported primitive") != std::string::npos &&
            d.text.find("K1") != std::string::npos) {
            throw std::runtime_error("mutual inductor card incorrectly reported as unsupported");
        }
    }
}

} // namespace

int main() {
    try {
        testModelInheritance();
        testLibSectionFilter();
        testIncludeResolverAndDiagnostics();
        testSubcktNamedNodeMapping();
        testCSWParsing();
        testJfetModelParsing();
        testVdmosModelParsing();
        testNonAsciiAndXSpiceParsing();
        testBsim4MultilineModelParsing();
        testMutualInductorCardIsAccepted();
        std::cout << "[PASS] model parser compatibility checks passed." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << std::endl;
        return 1;
    }
}
