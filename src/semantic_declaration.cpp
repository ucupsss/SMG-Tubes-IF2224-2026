#include "semantic.hpp"
#include "semantic_utils.hpp"

#include <utility>

namespace {

using semantic_util::makeErrorType;
using semantic_util::makePrimitiveType;
using semantic_util::makeVoidType;

std::string formatDuplicateMessage(const std::string& name) {
    return "redeclaration of identifier '" + name + "' in the same scope";
}

void addDiagnostic(
    std::vector<SemanticDiagnostic>& diagnostics,
    const std::string& message,
    const SourceLocation& location = {}
) {
    diagnostics.push_back({message, location.line, location.column});
}

struct ConstValue {
    TypeInfo type = makeErrorType();
    bool hasIntegerValue = false;
    int integerValue = 0;
};

ConstValue evaluateConstant(
    SymbolTable& symbols,
    ASTNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    if (!node) {
        addDiagnostic(errors, "missing constant value");
        return {};
    }

    if (auto* value = dynamic_cast<IntLiteralNode*>(node)) {
        return {makePrimitiveType(TYPE_INTEGER, "Integer"), true, value->value};
    }

    if (dynamic_cast<RealLiteralNode*>(node)) {
        return {makePrimitiveType(TYPE_REAL, "Real"), false, 0};
    }

    if (auto* value = dynamic_cast<CharLiteralNode*>(node)) {
        return {makePrimitiveType(TYPE_CHAR, "Char"), true, static_cast<int>(value->value)};
    }

    if (auto* value = dynamic_cast<StringLiteralNode*>(node)) {
        TypeInfo type = makePrimitiveType(TYPE_STRING, "String");
        type.stringLength = static_cast<int>(value->value.size());
        return {type, false, 0};
    }

    if (auto* value = dynamic_cast<BoolLiteralNode*>(node)) {
        return {makePrimitiveType(TYPE_BOOLEAN, "Boolean"), true, value->value ? 1 : 0};
    }

    if (auto* var = dynamic_cast<VarNode*>(node)) {
        const int index = symbols.lookupTab(var->name);
        if (index == 0) {
            addDiagnostic(errors, "undeclared constant identifier '" + var->name + "'", var->location);
            return {};
        }

        const TabEntry& entry = symbols.tabAt(index);
        if (entry.obj != OBJ_CONSTANT) {
            addDiagnostic(errors, "identifier '" + var->name + "' is not a constant", var->location);
            return {};
        }

        return {entry.typeInfo, true, entry.adr};
    }

    if (auto* unary = dynamic_cast<UnaryOpNode*>(node)) {
        ConstValue operand = evaluateConstant(symbols, unary->operand.get(), errors);
        if (operand.type.code == TYPE_ERROR) {
            return operand;
        }

        if (unary->op == "-") {
            if (operand.type.code != TYPE_INTEGER && operand.type.code != TYPE_REAL) {
                addDiagnostic(errors, "unary minus requires numeric constant", unary->location);
                return {};
            }

            if (operand.hasIntegerValue) {
                operand.integerValue = -operand.integerValue;
            }
        } else if (unary->op != "+") {
            addDiagnostic(errors, "unsupported unary operator in constant declaration", unary->location);
            return {};
        }

        return operand;
    }

    addDiagnostic(errors, "unsupported constant expression in declaration", node->location);
    return {};
}

void annotateNode(ASTNode* node, const TypeInfo& type, int tabIndex, const SymbolTable& symbols) {
    if (!node) {
        return;
    }

    node->inferredType = type.code;
    node->tabIndex = tabIndex;
    node->lexLevel = symbols.currentLevel();
}

void registerEnumConstants(
    SymbolTable& symbols,
    EnumTypeNode* enumNode,
    const TypeInfo& enumType,
    std::vector<SemanticDiagnostic>& errors
) {
    if (!enumNode) {
        return;
    }

    for (size_t i = 0; i < enumNode->values.size(); ++i) {
        const std::string& name = enumNode->values[i];
        if (symbols.lookupCurrentScope(name) != 0) {
            addDiagnostic(errors, formatDuplicateMessage(name), enumNode->location);
            continue;
        }

        TabEntry entry;
        entry.identifier = name;
        entry.obj = OBJ_CONSTANT;
        entry.type = enumType.code;
        entry.ref = enumType.ref;
        entry.typeInfo = enumType;
        entry.adr = static_cast<int>(i);
        symbols.enterTab(entry);
    }
}

}

void SemanticAnalyzer::visitProgram(ProgramNode* node) {
    symbolTable.init();

    if (!node) {
        semanticError("missing program node");
        return;
    }

    if (symbolTable.lookupCurrentScope(node->name) != 0) {
        semanticError(formatDuplicateMessage(node->name), node->location);
    } else {
        TabEntry programEntry;
        programEntry.identifier = node->name;
        programEntry.obj = OBJ_PROGRAM;
        programEntry.type = TYPE_VOID;
        programEntry.typeInfo = makeVoidType();

        const int index = symbolTable.enterTab(programEntry);
        annotateNode(node, programEntry.typeInfo, index, symbolTable);
    }

    node->blockIndex = 0;

    for (const auto& declaration : node->declarations) {
        visitDeclaration(declaration.get());
    }

    if (node->body) {
        const int mainBlockIndex = symbolTable.enterBTab(BTabEntry{});
        symbolTable.pushScope(mainBlockIndex);
        visitBlock(node->body.get());
        symbolTable.popScope();
    }
}

void SemanticAnalyzer::visitBlock(BlockNode* node) {
    if (!node) {
        return;
    }

    node->blockIndex = symbolTable.currentBlock();
    node->lexLevel = symbolTable.currentLevel();

    for (const auto& statement : node->statements) {
        visitStatement(statement.get());
    }
}

void SemanticAnalyzer::visitDeclaration(DeclarationNode* node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case ASTNodeKind::VarDecl:
            visitVarDecl(static_cast<VarDeclNode*>(node));
            break;
        case ASTNodeKind::ConstDecl:
            visitConstDecl(static_cast<ConstDeclNode*>(node));
            break;
        case ASTNodeKind::TypeDecl:
            visitTypeDecl(static_cast<TypeDeclNode*>(node));
            break;
        case ASTNodeKind::ParamDecl:
            visitParamDecl(static_cast<ParamDeclNode*>(node));
            break;
        case ASTNodeKind::ProcDecl:
            visitProcDecl(static_cast<ProcDeclNode*>(node));
            break;
        case ASTNodeKind::FuncDecl:
            visitFuncDecl(static_cast<FuncDeclNode*>(node));
            break;
        default:
            semanticError("unsupported declaration node", node->location);
            break;
    }
}

void SemanticAnalyzer::visitVarDecl(VarDeclNode* node) {
    if (!node) {
        return;
    }

    TypeInfo type = resolveType(node->type.get());
    int lastIndex = -1;

    for (const std::string& name : node->names) {
        if (symbolTable.lookupCurrentScope(name) != 0) {
            semanticError(formatDuplicateMessage(name), node->location);
            continue;
        }

        TabEntry entry;
        entry.identifier = name;
        entry.obj = OBJ_VARIABLE;
        entry.type = type.code;
        entry.ref = type.ref;
        entry.typeInfo = type;
        entry.adr = symbolTable.btabAt(symbolTable.currentBlock()).vsze;

        const int index = symbolTable.enterTab(entry);
        symbolTable.btabAt(symbolTable.currentBlock()).vsze += symbolTable.sizeOf(type);
        lastIndex = index;
    }

    annotateNode(node, type, lastIndex, symbolTable);
}

void SemanticAnalyzer::visitConstDecl(ConstDeclNode* node) {
    if (!node) {
        return;
    }

    if (symbolTable.lookupCurrentScope(node->name) != 0) {
        semanticError(formatDuplicateMessage(node->name), node->location);
        return;
    }

    ConstValue value = evaluateConstant(symbolTable, node->value.get(), errorList);

    TabEntry entry;
    entry.identifier = node->name;
    entry.obj = OBJ_CONSTANT;
    entry.type = value.type.code;
    entry.ref = value.type.ref;
    entry.typeInfo = value.type;
    entry.adr = value.hasIntegerValue ? value.integerValue : 0;

    const int index = symbolTable.enterTab(entry);
    annotateNode(node, value.type, index, symbolTable);
    annotateNode(node->value.get(), value.type, index, symbolTable);
}

void SemanticAnalyzer::visitTypeDecl(TypeDeclNode* node) {
    if (!node) {
        return;
    }

    if (symbolTable.lookupCurrentScope(node->name) != 0) {
        semanticError(formatDuplicateMessage(node->name), node->location);
        return;
    }

    TypeInfo type = resolveType(node->type.get());
    type.isNamed = true;
    type.name = node->name;

    TabEntry entry;
    entry.identifier = node->name;
    entry.obj = OBJ_TYPE;
    entry.type = type.code;
    entry.ref = type.ref;
    entry.typeInfo = type;

    const int index = symbolTable.enterTab(entry);
    annotateNode(node, type, index, symbolTable);
    annotate(node->type.get(), type, index);

    if (auto* enumNode = dynamic_cast<EnumTypeNode*>(node->type.get())) {
        registerEnumConstants(symbolTable, enumNode, type, errorList);
    }
}

void SemanticAnalyzer::visitParamDecl(ParamDeclNode* node) {
    if (!node) {
        return;
    }

    TypeInfo type = resolveType(node->type.get());
    int lastIndex = -1;

    for (const std::string& name : node->names) {
        if (symbolTable.lookupCurrentScope(name) != 0) {
            semanticError(formatDuplicateMessage(name), node->location);
            continue;
        }

        TabEntry entry;
        entry.identifier = name;
        entry.obj = OBJ_VARIABLE;
        entry.type = type.code;
        entry.ref = type.ref;
        entry.typeInfo = type;
        entry.nrm = node->byReference ? 0 : 1;
        entry.adr = symbolTable.btabAt(symbolTable.currentBlock()).psze;

        const int index = symbolTable.enterTab(entry);
        symbolTable.btabAt(symbolTable.currentBlock()).lpar = index;
        symbolTable.btabAt(symbolTable.currentBlock()).psze += symbolTable.sizeOf(type);
        lastIndex = index;
    }

    annotateNode(node, type, lastIndex, symbolTable);
}

void SemanticAnalyzer::visitProcDecl(ProcDeclNode* node) {
    if (!node) {
        return;
    }

    const int blockIndex = symbolTable.enterBTab(BTabEntry{});
    node->blockIndex = blockIndex;

    if (symbolTable.lookupCurrentScope(node->name) != 0) {
        semanticError(formatDuplicateMessage(node->name), node->location);
    } else {
        TabEntry entry;
        entry.identifier = node->name;
        entry.obj = OBJ_PROCEDURE;
        entry.type = TYPE_VOID;
        entry.ref = blockIndex;
        entry.typeInfo = makeVoidType();

        const int index = symbolTable.enterTab(entry);
        annotateNode(node, entry.typeInfo, index, symbolTable);
    }

    symbolTable.pushScope(blockIndex);

    for (const auto& parameter : node->parameters) {
        visitParamDecl(parameter.get());
    }

    for (const auto& declaration : node->declarations) {
        visitDeclaration(declaration.get());
    }

    if (node->body) {
        visitBlock(node->body.get());
    }

    symbolTable.popScope();
}

void SemanticAnalyzer::visitFuncDecl(FuncDeclNode* node) {
    if (!node) {
        return;
    }

    TypeInfo returnType = resolveType(node->returnType.get());
    const int blockIndex = symbolTable.enterBTab(BTabEntry{});
    node->blockIndex = blockIndex;

    if (symbolTable.lookupCurrentScope(node->name) != 0) {
        semanticError(formatDuplicateMessage(node->name), node->location);
    } else {
        TabEntry entry;
        entry.identifier = node->name;
        entry.obj = OBJ_FUNCTION;
        entry.type = returnType.code;
        entry.ref = blockIndex;
        entry.typeInfo = returnType;

        const int index = symbolTable.enterTab(entry);
        annotateNode(node, returnType, index, symbolTable);
    }

    symbolTable.pushScope(blockIndex);

    for (const auto& parameter : node->parameters) {
        visitParamDecl(parameter.get());
    }

    for (const auto& declaration : node->declarations) {
        visitDeclaration(declaration.get());
    }

    if (node->body) {
        visitBlock(node->body.get());
    }

    symbolTable.popScope();
}

void SemanticAnalyzer::semanticError(const std::string& message, const SourceLocation& location) {
    errorList.push_back({message, location.line, location.column});
}

void SemanticAnalyzer::semanticWarning(const std::string& message, const SourceLocation& location) {
    warningList.push_back({message, location.line, location.column});
}
