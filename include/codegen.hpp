#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "ast.hpp"
#include "intermediate.hpp"
#include "symbol_table.hpp"

#include <string>
#include <vector>

struct CodegenDiagnostic {
    std::string message;
    int line = -1;
    int column = -1;
};

struct CodeGenerationResult {
    IntermediateProgram program;
    std::vector<CodegenDiagnostic> diagnostics;

    bool success() const;
};

class CodeGenerator {
public:
    CodeGenerationResult generate(const ProgramNode& root, const SymbolTable& symbols);

private:
    static constexpr int frameHeaderSize = 3;

    void reset(const SymbolTable& symbols);
    void generateProgram(const ProgramNode& node);
    void generateBlock(const BlockNode& node);
    void generateStatement(const StatementNode& node);
    void generateAssignment(const AssignNode& node);
    void generateIf(const IfNode& node);
    void generateWhile(const WhileNode& node);
    void generateRepeat(const RepeatNode& node);
    void generateFor(const ForNode& node);
    void generateCase(const CaseNode& node);
    void generateProcedureCall(const ProcCallNode& node);
    void generateExpression(const ExpressionNode& node);

    bool emitLoadAddressable(const ExpressionNode& node);
    bool emitStoreAddressable(const ExpressionNode& node);
    bool emitConstant(const TabEntry& entry);

    int emit(Instruction instruction);
    void patchOperand(int instructionIndex, int operand);
    int nextAddress() const;

    int variableAddress(const TabEntry& entry);
    void diagnostic(const std::string& message, const SourceLocation& location = {});

    const SymbolTable* symbolTable = nullptr;
    CodeGenerationResult result;
};

#endif
