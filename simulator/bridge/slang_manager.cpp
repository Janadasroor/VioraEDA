#include "slang_manager.h"

#include <slang/syntax/SyntaxTree.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/Scope.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/symbols/MemberSymbols.h>
#include <slang/ast/ASTVisitor.h>
#include <slang/ast/Expression.h>
#include <slang/ast/expressions/AssignmentExpressions.h>
#include <slang/ast/expressions/OperatorExpressions.h>
#include <slang/ast/expressions/LiteralExpressions.h>
#include <slang/ast/expressions/MiscExpressions.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/diagnostics/TextDiagnosticClient.h>
#include <slang/text/SourceManager.h>

#include <QDebug>
#include <QMap>

namespace {

// Helper to translate Slang expressions to C++ strings
class ExpressionTranslator : public slang::ast::ASTVisitor<ExpressionTranslator, slang::ast::VisitFlags::Expressions> {
public:
    QString result;
    const QMap<std::string_view, int>& inputMap;
    const QMap<std::string_view, int>& outputMap;

    ExpressionTranslator(const QMap<std::string_view, int>& inputs, const QMap<std::string_view, int>& outputs)
        : inputMap(inputs), outputMap(outputs) {}

    // Required by ASTVisitor to dispatch to handle methods
    template<typename T>
    void visit(const T& t) {
        slang::ast::ASTVisitor<ExpressionTranslator, slang::ast::VisitFlags::Expressions>::visit(t);
    }

    void handle(const slang::ast::BinaryExpression& expr) {
        expr.left().visit(*this);
        switch (expr.op) {
            case slang::ast::BinaryOperator::Add: result += " + "; break;
            case slang::ast::BinaryOperator::Subtract: result += " - "; break;
            case slang::ast::BinaryOperator::Multiply: result += " * "; break;
            case slang::ast::BinaryOperator::Divide: result += " / "; break;
            case slang::ast::BinaryOperator::BinaryAnd: result += " & "; break;
            case slang::ast::BinaryOperator::BinaryOr: result += " | "; break;
            case slang::ast::BinaryOperator::BinaryXor: result += " ^ "; break;
            case slang::ast::BinaryOperator::LogicalAnd: result += " && "; break;
            case slang::ast::BinaryOperator::LogicalOr: result += " || "; break;
            case slang::ast::BinaryOperator::Equality: result += " == "; break;
            case slang::ast::BinaryOperator::Inequality: result += " != "; break;
            case slang::ast::BinaryOperator::GreaterThan: result += " > "; break;
            case slang::ast::BinaryOperator::LessThan: result += " < "; break;
            default: result += " /* unsupported op */ "; break;
        }
        expr.right().visit(*this);
    }

    void handle(const slang::ast::UnaryExpression& expr) {
        switch (expr.op) {
            case slang::ast::UnaryOperator::BitwiseNot: result += "~"; break;
            case slang::ast::UnaryOperator::LogicalNot: result += "!"; break;
            case slang::ast::UnaryOperator::Minus: result += "-"; break;
            default: break;
        }
        expr.operand().visit(*this);
    }

    void handle(const slang::ast::NamedValueExpression& expr) {
        auto name = expr.symbol.name;
        if (inputMap.contains(name)) {
            result += QString("(inputs[%1] > 2.5)").arg(inputMap[name]);
        } else if (outputMap.contains(name)) {
            result += QString("outputs[%1]").arg(outputMap[name]); // Should usually be on LHS
        } else {
            result += QString::fromStdString(std::string(name));
        }
    }

    void handle(const slang::ast::IntegerLiteral& expr) {
        result += QString::fromStdString(expr.getValue().toString());
    }
};

} // namespace

SlangManager& SlangManager::instance() {
    static SlangManager inst;
    return inst;
}

QList<SlangManager::PortInfo> SlangManager::extractPorts(const QString& svSource, const QString& moduleName, QString* error) {
    QList<PortInfo> ports;
    
    slang::SourceManager sourceManager;
    auto tree = slang::syntax::SyntaxTree::fromText(svSource.toStdString(), sourceManager);
    
    slang::ast::Compilation compilation;
    compilation.addSyntaxTree(tree);
    
    auto diagnostics = compilation.getAllDiagnostics();
    if (!diagnostics.empty() && error) {
        slang::DiagnosticEngine diagEngine(sourceManager);
        auto client = std::make_shared<slang::TextDiagnosticClient>();
        diagEngine.addClient(client);
        for (auto& diag : diagnostics) {
            diagEngine.issue(diag);
        }
        *error = QString::fromStdString(client->getString());
        // We continue if there are no fatal errors
    }

    auto& root = compilation.getRoot();
    const slang::ast::InstanceSymbol* targetModule = nullptr;
    for (auto* instance : root.topInstances) {
        if (instance->name == moduleName.toStdString()) {
            targetModule = instance;
            break;
        }
    }

    if (!targetModule) {
        if (error && (error->isEmpty())) *error = QString("Module %1 not found.").arg(moduleName);
        return ports;
    }

    for (auto& symbol : targetModule->body.members()) {
        if (symbol.kind == slang::ast::SymbolKind::Port) {
            auto& p = symbol.as<slang::ast::PortSymbol>();
            PortInfo info;
            info.name = QString::fromStdString(std::string(p.name));
            info.isInput = (p.direction == slang::ast::ArgumentDirection::In);
            info.width = (int)p.getType().getBitWidth();
            ports << info;
        }
    }

    return ports;
}

QString SlangManager::translateToCpp(const QString& svSource, const QString& moduleName, QString* error) {
    slang::SourceManager sourceManager;
    auto tree = slang::syntax::SyntaxTree::fromText(svSource.toStdString(), sourceManager);
    slang::ast::Compilation compilation;
    compilation.addSyntaxTree(tree);

    auto& root = compilation.getRoot();
    const slang::ast::InstanceSymbol* targetModule = nullptr;
    for (auto* instance : root.topInstances) {
        if (instance->name == moduleName.toStdString()) {
            targetModule = instance;
            break;
        }
    }

    if (!targetModule) return "";

    QMap<std::string_view, int> inputMap;
    QMap<std::string_view, int> outputMap;
    int inIdx = 0, outIdx = 0;

    for (auto& symbol : targetModule->body.members()) {
        if (symbol.kind == slang::ast::SymbolKind::Port) {
            auto& p = symbol.as<slang::ast::PortSymbol>();
            if (p.direction == slang::ast::ArgumentDirection::In) inputMap[p.name] = inIdx++;
            else outputMap[p.name] = outIdx++;
        }
    }

    QString body;
    for (auto& symbol : targetModule->body.members()) {
        if (symbol.kind == slang::ast::SymbolKind::ContinuousAssign) {
            auto& assign = symbol.as<slang::ast::ContinuousAssignSymbol>();
            
            // Handle simple assignment like: assign y = a & b;
            auto& expr = assign.getAssignment().as<slang::ast::AssignmentExpression>();
            
            ExpressionTranslator leftTrans(inputMap, outputMap);
            expr.left().visit(leftTrans);
            
            ExpressionTranslator rightTrans(inputMap, outputMap);
            expr.right().visit(rightTrans);
            
            // Convert boolean logic back to SPICE-friendly voltage levels (0V/5V)
            body += QString("    %1 = (%2) ? 5.0 : 0.0;\n")
                        .arg(leftTrans.result)
                        .arg(rightTrans.result);
        }
    }

    QString cppCode;
    cppCode += "#include <cmath>\n";
    cppCode += "extern \"C\" void " + moduleName + "_JIT(double time, const double* inputs, double* outputs) {\n";
    cppCode += body;
    cppCode += "}\n";

    return cppCode;
}
