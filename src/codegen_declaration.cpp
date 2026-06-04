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
    diagnostic(
        "procedure declaration code generation is not implemented yet for '" + node.name + "'",
        node.location
    );
}

void CodeGenerator::generateFuncDecl(const FuncDeclNode& node) {
    diagnostic(
        "function declaration code generation is not implemented yet for '" + node.name + "'",
        node.location
    );
}
