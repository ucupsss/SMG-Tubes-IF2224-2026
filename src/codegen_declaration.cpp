#include "codegen.hpp"

void CodeGenerator::generateDeclaration(const DeclarationNode& node) {
    switch (node.kind) {
        case ASTNodeKind::VarDecl:
            generateVarDecl(static_cast<const VarDeclNode&>(node));
            break;
        case ASTNodeKind::ConstDecl:
            generateConstDecl(static_cast<const ConstDeclNode&>(node));
            break;
        case ASTNodeKind::TypeDecl:
            generateTypeDecl(static_cast<const TypeDeclNode&>(node));
            break;
        case ASTNodeKind::ProcDecl:
            generateProcDecl(static_cast<const ProcDeclNode&>(node));
            break;
        case ASTNodeKind::FuncDecl:
            generateFuncDecl(static_cast<const FuncDeclNode&>(node));
            break;
        default:
            diagnostic("unsupported declaration node for code generation", node.location);
            break;
    }
}

void CodeGenerator::generateVarDecl(const VarDeclNode& /*node*/) {
    // Variable declarations are materialized in btab/tab by semantic analysis.
    // Program-level allocation is emitted once by generateProgram via INT.
}

void CodeGenerator::generateConstDecl(const ConstDeclNode& /*node*/) {
    // Constants do not emit runtime instructions; expressions load them as LIT.
}

void CodeGenerator::generateTypeDecl(const TypeDeclNode& /*node*/) {
    // Types only guide semantic annotation and address calculations.
}

void CodeGenerator::generateProcDecl(const ProcDeclNode& node) {
    if (!symbolTable) {
        diagnostic("missing symbol table for procedure declaration", node.location);
        return;
    }

    if (node.blockIndex <= 0 || node.blockIndex >= static_cast<int>(symbolTable->btab().size())) {
        diagnostic("procedure block index is out of range for '" + node.name + "'", node.location);
        return;
    }

    if (hasByReferenceParameter(node.blockIndex)) {
        diagnostic(
            "procedure '" + node.name + "' has by-reference parameter(s), which are not implemented yet",
            node.location
        );
        return;
    }

    const BTabEntry& block = symbolTable->btabAt(node.blockIndex);
    const int address = nextAddress();
    registerRoutine(node.tabIndex, address, block, false);

    const int savedBlock = currentBlockIndex;
    const int savedLevel = currentLexLevel;
    const int savedFunctionTab = currentFunctionTabIndex;
    const int savedFunctionReturnOffset = currentFunctionReturnOffset;

    currentBlockIndex = node.blockIndex;
    currentLexLevel = node.lexLevel + 1;
    currentFunctionTabIndex = 0;
    currentFunctionReturnOffset = 0;

    Instruction init;
    init.opcode = OpCode::INT;
    init.operand = frameHeaderSize + block.psze + block.vsze;
    emit(init);

    for (const auto& declaration : node.declarations) {
        if (declaration) {
            generateDeclaration(*declaration);
        }
    }

    if (node.body) {
        generateBlock(*node.body);
    } else {
        diagnostic("procedure '" + node.name + "' is missing its body", node.location);
    }

    Instruction ret;
    ret.opcode = OpCode::RET;
    emit(ret);

    currentBlockIndex = savedBlock;
    currentLexLevel = savedLevel;
    currentFunctionTabIndex = savedFunctionTab;
    currentFunctionReturnOffset = savedFunctionReturnOffset;
}

void CodeGenerator::generateFuncDecl(const FuncDeclNode& node) {
    if (!symbolTable) {
        diagnostic("missing symbol table for function declaration", node.location);
        return;
    }

    if (node.blockIndex <= 0 || node.blockIndex >= static_cast<int>(symbolTable->btab().size())) {
        diagnostic("function block index is out of range for '" + node.name + "'", node.location);
        return;
    }

    if (hasByReferenceParameter(node.blockIndex)) {
        diagnostic(
            "function '" + node.name + "' has by-reference parameter(s), which are not implemented yet",
            node.location
        );
        return;
    }

    const BTabEntry& block = symbolTable->btabAt(node.blockIndex);
    const int address = nextAddress();
    registerRoutine(node.tabIndex, address, block, true);

    const int savedBlock = currentBlockIndex;
    const int savedLevel = currentLexLevel;
    const int savedFunctionTab = currentFunctionTabIndex;
    const int savedFunctionReturnOffset = currentFunctionReturnOffset;

    currentBlockIndex = node.blockIndex;
    currentLexLevel = node.lexLevel + 1;
    currentFunctionTabIndex = node.tabIndex;
    currentFunctionReturnOffset = frameHeaderSize + block.psze + block.vsze;

    Instruction init;
    init.opcode = OpCode::INT;
    init.operand = frameHeaderSize + block.psze + block.vsze + 1;
    emit(init);

    for (const auto& declaration : node.declarations) {
        if (declaration) {
            generateDeclaration(*declaration);
        }
    }

    if (node.body) {
        generateBlock(*node.body);
    } else {
        diagnostic("function '" + node.name + "' is missing its body", node.location);
    }

    Instruction ret;
    ret.opcode = OpCode::RET;
    emit(ret);

    currentBlockIndex = savedBlock;
    currentLexLevel = savedLevel;
    currentFunctionTabIndex = savedFunctionTab;
    currentFunctionReturnOffset = savedFunctionReturnOffset;
}
