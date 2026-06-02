#include "codegen.hpp"

#include <exception>
#include <string>

bool CodeGenerationResult::success() const {
    return diagnostics.empty();
}

CodeGenerationResult CodeGenerator::generate(const ProgramNode& root, const SymbolTable& symbols) {
    reset(symbols);

    try {
        generateProgram(root);
    } catch (const std::exception& error) {
        diagnostic("internal code generation error: " + std::string(error.what()), root.location);
    } catch (...) {
        diagnostic("internal code generation error: unknown exception", root.location);
    }

    return result;
}

void CodeGenerator::reset(const SymbolTable& symbols) {
    symbolTable = &symbols;
    result = {};
}

void CodeGenerator::generateProgram(const ProgramNode& node) {
    if (!symbolTable) {
        diagnostic("missing symbol table for code generation", node.location);
        return;
    }

    int variableCount = 0;
    if (node.blockIndex >= 0 && node.blockIndex < static_cast<int>(symbolTable->btab().size())) {
        variableCount = symbolTable->btabAt(node.blockIndex).vsze;
    } else {
        diagnostic("program block index is out of range", node.location);
    }

    if (variableCount < 0) {
        diagnostic("program variable size is negative", node.location);
        variableCount = 0;
    }

    Instruction init;
    init.opcode = OpCode::INT;
    init.operand = frameHeaderSize + variableCount;
    emit(init);

    for (const auto& declaration : node.declarations) {
        if (!declaration) {
            continue;
        }

        if (declaration->kind == ASTNodeKind::ProcDecl ||
            declaration->kind == ASTNodeKind::FuncDecl) {
            diagnostic(
                "procedure/function code generation is not implemented yet",
                declaration->location
            );
        }
    }

    if (node.body) {
        generateBlock(*node.body);
    } else {
        diagnostic("program body is missing", node.location);
    }

    Instruction ret;
    ret.opcode = OpCode::RET;
    emit(ret);
}

void CodeGenerator::generateBlock(const BlockNode& node) {
    for (const auto& statement : node.statements) {
        if (!statement) {
            continue;
        }

        generateStatement(*statement);
    }
}

int CodeGenerator::emit(Instruction instruction) {
    result.program.instructions.push_back(std::move(instruction));
    return static_cast<int>(result.program.instructions.size()) - 1;
}

void CodeGenerator::patchOperand(int instructionIndex, int operand) {
    if (instructionIndex < 0 ||
        instructionIndex >= static_cast<int>(result.program.instructions.size())) {
        diagnostic("cannot patch invalid instruction address " + std::to_string(instructionIndex));
        return;
    }

    result.program.instructions[static_cast<size_t>(instructionIndex)].operand = operand;
}

int CodeGenerator::nextAddress() const {
    return static_cast<int>(result.program.instructions.size());
}

int CodeGenerator::variableAddress(const TabEntry& entry) {
    if (entry.obj != OBJ_VARIABLE) {
        diagnostic("identifier '" + entry.identifier + "' is not stored as a variable");
        return 0;
    }

    if (entry.adr < 0) {
        diagnostic("variable '" + entry.identifier + "' has an invalid address");
        return 0;
    }

    if (entry.nrm == 0) {
        diagnostic(
            "by-reference parameter addressing is not implemented yet for '" + entry.identifier + "'"
        );
    }

    if (entry.lev != 0) {
        diagnostic(
            "non-global variable addressing is not implemented yet for '" + entry.identifier + "'"
        );
    }

    return frameHeaderSize + entry.adr;
}

void CodeGenerator::diagnostic(const std::string& message, const SourceLocation& location) {
    result.diagnostics.push_back({message, location.line, location.column});
}
