#include "semantic.hpp"

#include <sstream>
#include <utility>

namespace {

TypeInfo makeErrorType() {
    TypeInfo type;
    type.code = TYPE_ERROR;
    type.baseType = TYPE_ERROR;
    type.name = "Error";
    return type;
}

TypeInfo makeVoidType() {
    TypeInfo type;
    type.code = TYPE_VOID;
    type.baseType = TYPE_VOID;
    type.name = "Void";
    return type;
}

void addDiagnostic(
    std::vector<SemanticDiagnostic>& diagnostics,
    const std::string& message,
    const SourceLocation& location
) {
    diagnostics.push_back({message, location.line, location.column});
}

std::string formatDuplicateMessage(const std::string& name) {
    return "redeclaration of identifier '" + name + "' in the same scope";
}

struct ConstValue {
    TypeInfo type = makeErrorType();
    bool hasIntegerValue = false;
    int integerValue = 0;
};

TypeInfo typeFromCode(int code, const std::string& name) {
    TypeInfo type;
    type.code = code;
    type.baseType = code;
    type.name = name;
    type.isNamed = true;
    return type;
}

ConstValue evaluateConstant(
    SymbolTable& symbols,
    ASTNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    if (!node) {
        addDiagnostic(errors, "missing constant value", {});
        return {};
    }

    if (auto* value = dynamic_cast<IntLiteralNode*>(node)) {
        return {typeFromCode(TYPE_INTEGER, "Integer"), true, value->value};
    }

    if (dynamic_cast<RealLiteralNode*>(node)) {
        return {typeFromCode(TYPE_REAL, "Real"), false, 0};
    }

    if (auto* value = dynamic_cast<CharLiteralNode*>(node)) {
        return {typeFromCode(TYPE_CHAR, "Char"), true, static_cast<int>(value->value)};
    }

    if (auto* value = dynamic_cast<StringLiteralNode*>(node)) {
        TypeInfo type = typeFromCode(TYPE_STRING, "String");
        type.stringLength = static_cast<int>(value->value.size());
        return {type, false, 0};
    }

    if (auto* value = dynamic_cast<BoolLiteralNode*>(node)) {
        return {typeFromCode(TYPE_BOOLEAN, "Boolean"), true, value->value ? 1 : 0};
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

TypeInfo resolveDeclarationType(
    SymbolTable& symbols,
    TypeNode* node,
    std::vector<SemanticDiagnostic>& errors
);

void annotateTypeNode(TypeNode* node, const TypeInfo& type) {
    if (!node) {
        return;
    }

    node->typeCode = type.code;
    node->ref = type.ref;
    node->isNamed = type.isNamed;
    node->typeName = type.name;
    node->inferredType = type.code;
}

TypeInfo resolveNamedType(
    SymbolTable& symbols,
    NamedTypeNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    const int index = symbols.lookupTab(node->name);
    if (index == 0 || symbols.tabAt(index).obj != OBJ_TYPE) {
        addDiagnostic(errors, "unknown type '" + node->name + "'", node->location);
        return makeErrorType();
    }

    TypeInfo type = symbols.typeOf(index);
    annotateTypeNode(node, type);
    node->tabIndex = index;
    return type;
}

TypeInfo resolveSubrangeType(
    SymbolTable& symbols,
    SubrangeTypeNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    ConstValue lower = evaluateConstant(symbols, node->lowerBound.get(), errors);
    ConstValue upper = evaluateConstant(symbols, node->upperBound.get(), errors);

    if (lower.type.code == TYPE_ERROR || upper.type.code == TYPE_ERROR) {
        annotateTypeNode(node, makeErrorType());
        return makeErrorType();
    }

    if (lower.type.code != upper.type.code) {
        addDiagnostic(errors, "subrange bounds must have the same type", node->location);
        annotateTypeNode(node, makeErrorType());
        return makeErrorType();
    }

    if (lower.type.code == TYPE_REAL) {
        addDiagnostic(errors, "subrange cannot use Real bounds", node->location);
        annotateTypeNode(node, makeErrorType());
        return makeErrorType();
    }

    if (lower.hasIntegerValue && upper.hasIntegerValue && lower.integerValue > upper.integerValue) {
        addDiagnostic(errors, "subrange lower bound cannot be greater than upper bound", node->location);
    }

    TypeInfo type;
    type.code = TYPE_SUBRANGE;
    type.baseType = lower.type.code;
    type.low = lower.hasIntegerValue ? lower.integerValue : 0;
    type.high = upper.hasIntegerValue ? upper.integerValue : 0;

    std::ostringstream name;
    name << "Subrange(" << type.low << ".." << type.high << ")";
    type.name = name.str();

    annotateTypeNode(node, type);
    return type;
}

TypeInfo resolveEnumType(EnumTypeNode* node) {
    TypeInfo type;
    type.code = TYPE_ENUM;
    type.baseType = TYPE_ENUM;
    type.low = 0;
    type.high = node ? static_cast<int>(node->values.size()) - 1 : 0;
    type.name = "Enum";
    annotateTypeNode(node, type);
    return type;
}

TypeInfo resolveArrayType(
    SymbolTable& symbols,
    ArrayTypeNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    TypeInfo indexType = resolveDeclarationType(symbols, node->indexType.get(), errors);
    TypeInfo elementType = resolveDeclarationType(symbols, node->elementType.get(), errors);

    if (indexType.code == TYPE_REAL ||
        indexType.code == TYPE_ARRAY ||
        indexType.code == TYPE_RECORD ||
        indexType.code == TYPE_ERROR) {
        addDiagnostic(errors, "array index type must be a non-Real simple type", node->location);
    }

    ATabEntry arrayEntry;
    arrayEntry.xtyp = indexType.code == TYPE_SUBRANGE ? indexType.baseType : indexType.code;
    arrayEntry.etyp = elementType.code;
    arrayEntry.eref = elementType.ref;
    arrayEntry.low = indexType.code == TYPE_SUBRANGE ? indexType.low : 0;
    arrayEntry.high = indexType.code == TYPE_SUBRANGE ? indexType.high : 0;
    arrayEntry.elsz = symbols.sizeOf(elementType);

    const int ref = symbols.enterATab(arrayEntry);

    TypeInfo type;
    type.code = TYPE_ARRAY;
    type.baseType = TYPE_ARRAY;
    type.ref = ref;
    type.name = "Array";

    annotateTypeNode(node, type);
    return type;
}

TypeInfo resolveRecordType(
    SymbolTable& symbols,
    RecordTypeNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    const int blockIndex = symbols.enterBTab(BTabEntry{});
    symbols.pushScope(blockIndex);

    int fieldOffset = 0;
    for (const RecordFieldNode& field : node->fields) {
        TypeInfo fieldType = resolveDeclarationType(symbols, field.type.get(), errors);

        for (const std::string& name : field.names) {
            if (symbols.lookupCurrentScope(name) != 0) {
                addDiagnostic(errors, formatDuplicateMessage(name), field.location);
                continue;
            }

            TabEntry entry;
            entry.identifier = name;
            entry.obj = OBJ_VARIABLE;
            entry.type = fieldType.code;
            entry.ref = fieldType.ref;
            entry.typeInfo = fieldType;
            entry.adr = fieldOffset;

            symbols.enterTab(entry);
            fieldOffset += symbols.sizeOf(fieldType);
        }
    }

    symbols.btabAt(blockIndex).vsze = fieldOffset;
    symbols.popScope();

    TypeInfo type;
    type.code = TYPE_RECORD;
    type.baseType = TYPE_RECORD;
    type.ref = blockIndex;
    type.name = "Record";

    annotateTypeNode(node, type);
    return type;
}

TypeInfo resolveDeclarationType(
    SymbolTable& symbols,
    TypeNode* node,
    std::vector<SemanticDiagnostic>& errors
) {
    if (!node) {
        addDiagnostic(errors, "missing type information", {});
        return makeErrorType();
    }

    if (auto* named = dynamic_cast<NamedTypeNode*>(node)) {
        return resolveNamedType(symbols, named, errors);
    }

    if (auto* subrange = dynamic_cast<SubrangeTypeNode*>(node)) {
        return resolveSubrangeType(symbols, subrange, errors);
    }

    if (auto* en = dynamic_cast<EnumTypeNode*>(node)) {
        return resolveEnumType(en);
    }

    if (auto* array = dynamic_cast<ArrayTypeNode*>(node)) {
        return resolveArrayType(symbols, array, errors);
    }

    if (auto* record = dynamic_cast<RecordTypeNode*>(node)) {
        return resolveRecordType(symbols, record, errors);
    }

    addDiagnostic(errors, "unsupported type node in declaration", node->location);
    return makeErrorType();
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
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();

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
        visitBlock(node->body.get());
    }
}

void SemanticAnalyzer::visitBlock(BlockNode* node) {
    if (!node) {
        return;
    }

    node->blockIndex = symbolTable.currentBlock();
    node->lexLevel = symbolTable.currentLevel();
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

    TypeInfo type = resolveDeclarationType(symbolTable, node->type.get(), errorList);
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

    TypeInfo type = resolveDeclarationType(symbolTable, node->type.get(), errorList);
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
    annotateTypeNode(node->type.get(), type);

    if (auto* enumNode = dynamic_cast<EnumTypeNode*>(node->type.get())) {
        registerEnumConstants(symbolTable, enumNode, type, errorList);
    }
}

void SemanticAnalyzer::visitParamDecl(ParamDeclNode* node) {
    if (!node) {
        return;
    }

    TypeInfo type = resolveDeclarationType(symbolTable, node->type.get(), errorList);
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
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();

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
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();
}

void SemanticAnalyzer::visitFuncDecl(FuncDeclNode* node) {
    if (!node) {
        return;
    }

    TypeInfo returnType = resolveDeclarationType(symbolTable, node->returnType.get(), errorList);
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
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();

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
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();
}

void SemanticAnalyzer::semanticError(const std::string& message, const SourceLocation& location) {
    errorList.push_back({message, location.line, location.column});
}

void SemanticAnalyzer::semanticWarning(const std::string& message, const SourceLocation& location) {
    warningList.push_back({message, location.line, location.column});
}
