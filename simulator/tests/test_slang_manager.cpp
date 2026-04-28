#include "../bridge/slang_manager.h"
#include <iostream>
#include <cassert>
#include <QString>
#include <QList>

int main() {
    SlangManager& manager = SlangManager::instance();

    QString svSource = 
        "module MyLogic(input logic a, input logic b, output logic y);\n"
        "    assign y = a & ~b;\n"
        "endmodule\n";

    QString error;
    QList<SlangManager::PortInfo> ports = manager.extractPorts(svSource, "MyLogic", &error);
    
    if (!error.isEmpty()) {
        std::cerr << "Port extraction failed: " << error.toStdString() << std::endl;
        return 1;
    }

    assert(ports.size() == 3);
    assert(ports[0].name == "a" && ports[0].isInput);
    assert(ports[1].name == "b" && ports[1].isInput);
    assert(ports[2].name == "y" && !ports[2].isInput);

    QString cppCode = manager.translateToCpp(svSource, "MyLogic", &error);
    if (cppCode.isEmpty()) {
        std::cerr << "Translation failed: " << error.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Generated C++ Code:\n" << cppCode.toStdString() << std::endl;

    // Verify expected logic is present in the generated code
    assert(cppCode.contains("extern \"C\" void MyLogic_JIT"));
    assert(cppCode.contains("outputs[0] = ((inputs[0] > 2.5) & ~(inputs[1] > 2.5)) ? 5.0 : 0.0;"));

    std::cout << "simulator.slang_manager: all tests passed" << std::endl;
    return 0;
}
