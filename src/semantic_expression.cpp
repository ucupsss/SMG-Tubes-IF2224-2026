#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

TypeInfo makeErrorType() {
    TypeInfo type;
    type.code = TYPE_ERROR;
    type.baseType = TYPE_ERROR;
    type.name = "Error";
    return type;
}

TypeInfo makePrimitiveType(int code, const std::string& name) {
    TypeInfo type;
    type.code = code;
    type.baseType = code;
    type.name = name;
    type.isNamed = true;
    return type;
}

int effectiveCode(const TypeInfo& type) {
    return type.code == TYPE_SUBRANGE ? type.baseType : type.code;
}

bool isStructured(const TypeInfo& type) {
    return type.code == TYPE_ARRAY || type.code == TYPE_RECORD;
}

bool isRelationalOperator(const std::string& op) {
    return op == "=" || op == "<>" || op == "<" || op == "<=" || op == ">" || op == ">=";
}

std::optional<TypeInfo> findNamedType(const SymbolTable& symbols, int code, int ref) {
    const auto& entries = symbols.tab();

    for (int i = static_cast<int>(entries.size()) - 1; i > 0; --i) {
        const TabEntry& entry = entries[static_cast<size_t>(i)];
        if (entry.obj != OBJ_TYPE) {
            continue;
        }

        if (entry.typeInfo.code == code && entry.typeInfo.ref == ref) {
            return entry.typeInfo;
        }
    }

    return std::nullopt;
}

TypeInfo recoverTypeInfo(const SymbolTable& symbols, int code, int ref) {
    if (auto named = findNamedType(symbols, code, ref)) {
        return *named;
    }

    TypeInfo type;
    type.code = code;
    type.baseType = code;
    type.ref = ref;
    type.name = symbols.typeName(type);

    if (code == TYPE_STRING) {
        type.isNamed = true;
    }

    return type;
}

TypeInfo indexTypeFromArrayEntry(const ATabEntry& entry) {
    if (entry.low != 0 || entry.high != 0) {
        TypeInfo type;
        type.code = TYPE_SUBRANGE;
        type.baseType = entry.xtyp;
        type.low = entry.low;
        type.high = entry.high;

        std::ostringstream name;
        name << "Subrange(" << type.low << ".." << type.high << ")";
        type.name = name.str();
        return type;
    }

    switch (entry.xtyp) {
        case TYPE_INTEGER: return makePrimitiveType(TYPE_INTEGER, "Integer");
        case TYPE_REAL: return makePrimitiveType(TYPE_REAL, "Real");
        case TYPE_CHAR: return makePrimitiveType(TYPE_CHAR, "Char");
        case TYPE_BOOLEAN: return makePrimitiveType(TYPE_BOOLEAN, "Boolean");
        case TYPE_STRING: return makePrimitiveType(TYPE_STRING, "String");
        case TYPE_ENUM: return makePrimitiveType(TYPE_ENUM, "Enum");
        default: break;
    }

    TypeInfo type;
    type.code = entry.xtyp;
    type.baseType = entry.xtyp;
    return type;
}

TypeInfo elementTypeFromArrayEntry(const SymbolTable& symbols, const ATabEntry& entry) {
    return recoverTypeInfo(symbols, entry.etyp, entry.eref);
}

TypeInfo typeFromAnnotatedNode(const TypeNode* node) {
    TypeInfo type;
    type.code = node->typeCode;
    type.baseType = node->typeCode == TYPE_SUBRANGE ? TYPE_INTEGER : node->typeCode;
    type.ref = node->ref;
    type.isNamed = node->isNamed;
    type.name = node->typeName;
    return type;
}

bool isValueLikeTarget(const ExpressionNode* node) {
    if (!node) {
        return false;
    }

    return node->kind == ASTNodeKind::Var ||
           node->kind == ASTNodeKind::ArrayAccess ||
           node->kind == ASTNodeKind::RecordAccess;
}

struct ConstantValue {
    TypeInfo type = makeErrorType();
    bool knownOrdinal = false;
    int ordinalValue = 0;
};

std::optional<ConstantValue> tryEvaluateConstant(const ExpressionNode* node, const SymbolTable& symbols) {
    if (!node) {
        return std::nullopt;
    }

    if (const auto* literal = dynamic_cast<const IntLiteralNode*>(node)) {
        return ConstantValue{makePrimitiveType(TYPE_INTEGER, "Integer"), true, literal->value};
    }

    if (dynamic_cast<const RealLiteralNode*>(node)) {
        return ConstantValue{makePrimitiveType(TYPE_REAL, "Real"), false, 0};
    }

    if (const auto* literal = dynamic_cast<const CharLiteralNode*>(node)) {
        return ConstantValue{
            makePrimitiveType(TYPE_CHAR, "Char"),
            true,
            static_cast<int>(literal->value)
        };
    }

    if (const auto* literal = dynamic_cast<const BoolLiteralNode*>(node)) {
        return ConstantValue{
            makePrimitiveType(TYPE_BOOLEAN, "Boolean"),
            true,
            literal->value ? 1 : 0
        };
    }

    if (const auto* variable = dynamic_cast<const VarNode*>(node)) {
        const int index = symbols.lookupTab(variable->name);
        if (index == 0) {
            return std::nullopt;
        }

        const TabEntry& entry = symbols.tabAt(index);
        if (entry.obj != OBJ_CONSTANT) {
            return std::nullopt;
        }

        ConstantValue value;
        value.type = entry.typeInfo;
        value.knownOrdinal = entry.typeInfo.code != TYPE_REAL && entry.typeInfo.code != TYPE_STRING;
        value.ordinalValue = entry.adr;
        return value;
    }

    if (const auto* unary = dynamic_cast<const UnaryOpNode*>(node)) {
        auto operand = tryEvaluateConstant(unary->operand.get(), symbols);
        if (!operand.has_value()) {
            return std::nullopt;
        }

        const std::string op = lower(unary->op);
        if ((op == "+" || op == "-") &&
            (operand->type.code == TYPE_INTEGER || operand->type.code == TYPE_REAL || operand->type.code == TYPE_SUBRANGE)) {
            if (operand->knownOrdinal && op == "-") {
                operand->ordinalValue = -operand->ordinalValue;
            }
            return operand;
        }
    }

    return std::nullopt;
}

bool constantFitsTarget(const TypeInfo& target, const ExpressionNode* node, const SymbolTable& symbols) {
    const auto value = tryEvaluateConstant(node, symbols);
    if (!value.has_value()) {
        return true;
    }

    if (target.code == TYPE_SUBRANGE && value->knownOrdinal) {
        return value->ordinalValue >= target.low && value->ordinalValue <= target.high;
    }

    return true;
}

bool isAssignableEntry(const TabEntry& entry, int currentBlock) {
    return entry.obj == OBJ_VARIABLE || (entry.obj == OBJ_FUNCTION && entry.ref == currentBlock);
}

}

void SemanticAnalyzer::annotate(ASTNode* node, const TypeInfo& type, int tabIndex) {
    if (!node) {
        return;
    }

    node->inferredType = type.code;
    if (tabIndex != -1 || node->tabIndex == -1) {
        node->tabIndex = tabIndex;
    }
    node->lexLevel = symbolTable.currentLevel();

    if (auto* typeNode = dynamic_cast<TypeNode*>(node)) {
        typeNode->typeCode = type.code;
        typeNode->ref = type.ref;
        typeNode->isNamed = type.isNamed;
        typeNode->typeName = type.name;
    }
}

TypeInfo SemanticAnalyzer::visitExpression(ExpressionNode* node) {
    if (!node) {
        semanticError("missing expression");
        return makeErrorType();
    }

    switch (node->kind) {
        case ASTNodeKind::IntLiteral: {
            TypeInfo type = makePrimitiveType(TYPE_INTEGER, "Integer");
            annotate(node, type);
            return type;
        }
        case ASTNodeKind::RealLiteral: {
            TypeInfo type = makePrimitiveType(TYPE_REAL, "Real");
            annotate(node, type);
            return type;
        }
        case ASTNodeKind::CharLiteral: {
            TypeInfo type = makePrimitiveType(TYPE_CHAR, "Char");
            annotate(node, type);
            return type;
        }
        case ASTNodeKind::StringLiteral: {
            TypeInfo type = makePrimitiveType(TYPE_STRING, "String");
            auto* literal = static_cast<StringLiteralNode*>(node);
            type.stringLength = static_cast<int>(literal->value.size());
            annotate(node, type);
            return type;
        }
        case ASTNodeKind::BoolLiteral: {
            TypeInfo type = makePrimitiveType(TYPE_BOOLEAN, "Boolean");
            annotate(node, type);
            return type;
        }
        case ASTNodeKind::Var:
            return visitVar(static_cast<VarNode*>(node));
        case ASTNodeKind::ArrayAccess:
            return visitArrayAccess(static_cast<ArrayAccessNode*>(node));
        case ASTNodeKind::RecordAccess:
            return visitRecordAccess(static_cast<RecordAccessNode*>(node));
        case ASTNodeKind::FuncCall:
            return visitFuncCall(static_cast<FuncCallNode*>(node));
        case ASTNodeKind::UnaryOp:
            return visitUnaryOp(static_cast<UnaryOpNode*>(node));
        case ASTNodeKind::BinOp:
            return visitBinOp(static_cast<BinOpNode*>(node));
        default:
            semanticError("unsupported expression node", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
    }
}

TypeInfo SemanticAnalyzer::visitBinOp(BinOpNode* node) {
    if (!node) {
        semanticError("missing binary operator expression");
        return makeErrorType();
    }

    TypeInfo left = visitExpression(node->left.get());
    TypeInfo right = visitExpression(node->right.get());
    const std::string op = lower(node->op);

    if (left.code == TYPE_ERROR || right.code == TYPE_ERROR) {
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    if (op == "+" || op == "-" || op == "*") {
        if (!isNumeric(left) || !isNumeric(right)) {
            semanticError("operator '" + node->op + "' requires numeric operands", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = (effectiveCode(left) == TYPE_REAL || effectiveCode(right) == TYPE_REAL)
            ? makePrimitiveType(TYPE_REAL, "Real")
            : makePrimitiveType(TYPE_INTEGER, "Integer");

        annotate(node, result);
        return result;
    }

    if (op == "/") {
        if (!isNumeric(left) || !isNumeric(right)) {
            semanticError("operator '/' requires numeric operands", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = makePrimitiveType(TYPE_REAL, "Real");
        annotate(node, result);
        return result;
    }

    if (op == "div" || op == "mod") {
        if (effectiveCode(left) != TYPE_INTEGER || effectiveCode(right) != TYPE_INTEGER) {
            semanticError("operator '" + node->op + "' requires Integer operands", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = makePrimitiveType(TYPE_INTEGER, "Integer");
        annotate(node, result);
        return result;
    }

    if (op == "and" || op == "or") {
        if (!isBoolean(left) || !isBoolean(right)) {
            semanticError("operator '" + node->op + "' requires Boolean operands", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = makePrimitiveType(TYPE_BOOLEAN, "Boolean");
        annotate(node, result);
        return result;
    }

    if (isRelationalOperator(op)) {
        if (!isCompatible(left, right)) {
            semanticError(
                "incompatible operand types for operator '" + node->op + "': " +
                symbolTable.typeName(left) + " and " + symbolTable.typeName(right),
                node->location
            );
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        if (isStructured(left) || isStructured(right)) {
            semanticError("relational operators do not support structured operands", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = makePrimitiveType(TYPE_BOOLEAN, "Boolean");
        annotate(node, result);
        return result;
    }

    semanticError("unsupported operator '" + node->op + "'", node->location);
    annotate(node, makeErrorType());
    return makeErrorType();
}

TypeInfo SemanticAnalyzer::visitUnaryOp(UnaryOpNode* node) {
    if (!node) {
        semanticError("missing unary operator expression");
        return makeErrorType();
    }

    TypeInfo operand = visitExpression(node->operand.get());
    const std::string op = lower(node->op);

    if (operand.code == TYPE_ERROR) {
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    if (op == "not") {
        if (!isBoolean(operand)) {
            semanticError("operator 'not' requires a Boolean operand", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = makePrimitiveType(TYPE_BOOLEAN, "Boolean");
        annotate(node, result);
        return result;
    }

    if (op == "+" || op == "-") {
        if (!isNumeric(operand)) {
            semanticError("unary operator '" + node->op + "' requires a numeric operand", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
        }

        TypeInfo result = effectiveCode(operand) == TYPE_REAL
            ? makePrimitiveType(TYPE_REAL, "Real")
            : makePrimitiveType(TYPE_INTEGER, "Integer");

        annotate(node, result);
        return result;
    }

    semanticError("unsupported unary operator '" + node->op + "'", node->location);
    annotate(node, makeErrorType());
    return makeErrorType();
}

TypeInfo SemanticAnalyzer::visitVar(VarNode* node) {
    if (!node) {
        semanticError("missing variable node");
        return makeErrorType();
    }

    const int index = symbolTable.lookupTab(node->name);
    if (index == 0) {
        semanticError("undeclared identifier '" + node->name + "'", node->location);
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    const TabEntry& entry = symbolTable.tabAt(index);
    if (entry.obj == OBJ_TYPE || entry.obj == OBJ_PROCEDURE || entry.obj == OBJ_PROGRAM || entry.obj == OBJ_RESERVED) {
        semanticError("identifier '" + node->name + "' cannot be used as a value", node->location);
        annotate(node, makeErrorType(), index);
        return makeErrorType();
    }

    if (entry.obj == OBJ_FUNCTION && entry.ref != symbolTable.currentBlock()) {
        semanticError("function '" + node->name + "' must be called to produce a value", node->location);
        annotate(node, makeErrorType(), index);
        return makeErrorType();
    }

    annotate(node, entry.typeInfo, index);
    return entry.typeInfo;
}

TypeInfo SemanticAnalyzer::visitArrayAccess(ArrayAccessNode* node) {
    if (!node) {
        semanticError("missing array access node");
        return makeErrorType();
    }

    TypeInfo currentType = visitExpression(node->array.get());
    int rootTabIndex = node->array ? node->array->tabIndex : -1;

    for (const auto& indexExpr : node->indices) {
        TypeInfo indexType = visitExpression(indexExpr.get());

        if (currentType.code != TYPE_ARRAY || currentType.ref <= 0) {
            semanticError("subscripted expression is not an array", node->location);
            annotate(node, makeErrorType(), rootTabIndex);
            return makeErrorType();
        }

        const ATabEntry& arrayEntry = symbolTable.atabAt(currentType.ref);
        TypeInfo expectedIndexType = indexTypeFromArrayEntry(arrayEntry);

        if (!isCompatible(expectedIndexType, indexType)) {
            semanticError(
                "array index type mismatch: expected " + symbolTable.typeName(expectedIndexType) +
                ", got " + symbolTable.typeName(indexType),
                indexExpr ? indexExpr->location : node->location
            );
        }

        currentType = elementTypeFromArrayEntry(symbolTable, arrayEntry);
    }

    annotate(node, currentType, rootTabIndex);
    return currentType;
}

TypeInfo SemanticAnalyzer::visitRecordAccess(RecordAccessNode* node) {
    if (!node) {
        semanticError("missing record access node");
        return makeErrorType();
    }

    TypeInfo recordType = visitExpression(node->record.get());
    if (recordType.code != TYPE_RECORD || recordType.ref <= 0) {
        semanticError("field access requires a record operand", node->location);
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    const int fieldIndex = symbolTable.lookupTab(node->fieldName, recordType.ref);
    if (fieldIndex == 0) {
        semanticError(
            "record type does not contain field '" + node->fieldName + "'",
            node->location
        );
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    const TabEntry& field = symbolTable.tabAt(fieldIndex);
    annotate(node, field.typeInfo, fieldIndex);
    return field.typeInfo;
}

TypeInfo SemanticAnalyzer::visitFuncCall(FuncCallNode* node) {
    if (!node) {
        semanticError("missing function call node");
        return makeErrorType();
    }

    const int index = symbolTable.lookupTab(node->name);
    if (index == 0) {
        semanticError("undeclared function '" + node->name + "'", node->location);
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    const TabEntry& entry = symbolTable.tabAt(index);
    if (entry.obj != OBJ_FUNCTION) {
        semanticError("identifier '" + node->name + "' is not a function", node->location);
        annotate(node, makeErrorType(), index);
        return makeErrorType();
    }

    std::vector<int> parameterIndices;
    if (entry.ref > 0) {
        int parameterIndex = symbolTable.btabAt(entry.ref).lpar;
        while (parameterIndex != 0) {
            parameterIndices.push_back(parameterIndex);
            parameterIndex = symbolTable.tabAt(parameterIndex).link;
        }
        std::reverse(parameterIndices.begin(), parameterIndices.end());
    }

    std::vector<TypeInfo> argumentTypes;
    argumentTypes.reserve(node->arguments.size());
    for (const auto& argument : node->arguments) {
        argumentTypes.push_back(visitExpression(argument.get()));
    }

    if (node->arguments.size() != parameterIndices.size()) {
        semanticError(
            "function '" + node->name + "' expects " +
            std::to_string(parameterIndices.size()) + " argument(s), got " +
            std::to_string(node->arguments.size()),
            node->location
        );
    } else {
        for (size_t i = 0; i < node->arguments.size(); ++i) {
            const TabEntry& parameter = symbolTable.tabAt(parameterIndices[i]);
            TypeInfo argumentType = argumentTypes[i];

            ExpressionNode* argumentNode = node->arguments[i].get();

            if (parameter.nrm == 0) {
                if (!isValueLikeTarget(argumentNode)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of function '" + node->name +
                        "' must be assignable for by-reference parameter",
                        argumentNode ? argumentNode->location : node->location
                    );
                } else if (argumentNode && argumentNode->tabIndex > 0) {
                    const TabEntry& target = symbolTable.tabAt(argumentNode->tabIndex);
                    if (!isAssignableEntry(target, symbolTable.currentBlock())) {
                        semanticError(
                            "argument " + std::to_string(i + 1) + " of function '" + node->name +
                            "' is not assignable",
                            argumentNode->location
                        );
                    }
                }

                if (!isCompatible(parameter.typeInfo, argumentType)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of function '" + node->name +
                        "' expects " + symbolTable.typeName(parameter.typeInfo) +
                        ", got " + symbolTable.typeName(argumentType),
                        argumentNode ? argumentNode->location : node->location
                    );
                }
            } else {
                if (!isAssignmentCompatible(parameter.typeInfo, argumentType)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of function '" + node->name +
                        "' expects " + symbolTable.typeName(parameter.typeInfo) +
                        ", got " + symbolTable.typeName(argumentType),
                        argumentNode ? argumentNode->location : node->location
                    );
                } else if (!constantFitsTarget(parameter.typeInfo, argumentNode, symbolTable)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of function '" + node->name +
                        "' is outside the parameter type range",
                        argumentNode ? argumentNode->location : node->location
                    );
                }
            }
        }
    }

    annotate(node, entry.typeInfo, index);
    return entry.typeInfo;
}

TypeInfo SemanticAnalyzer::resolveType(TypeNode* node) {
    if (!node) {
        semanticError("missing type information");
        return makeErrorType();
    }

    if (node->kind != ASTNodeKind::SubrangeType &&
        (node->typeCode != TYPE_VOID || node->ref != 0 || node->tabIndex != -1)) {
        TypeInfo type = typeFromAnnotatedNode(node);
        if (type.code == TYPE_VOID && node->kind != ASTNodeKind::NamedType) {
            return type;
        }
        if (type.code != TYPE_VOID) {
            return type;
        }
    }

    switch (node->kind) {
        case ASTNodeKind::NamedType:
            return resolveNamedType(static_cast<NamedTypeNode*>(node));
        case ASTNodeKind::ArrayType:
            return resolveArrayType(static_cast<ArrayTypeNode*>(node));
        case ASTNodeKind::RecordType:
            return resolveRecordType(static_cast<RecordTypeNode*>(node));
        case ASTNodeKind::SubrangeType:
            return resolveSubrangeType(static_cast<SubrangeTypeNode*>(node));
        case ASTNodeKind::EnumType:
            return resolveEnumType(static_cast<EnumTypeNode*>(node));
        default:
            semanticError("unsupported type node", node->location);
            annotate(node, makeErrorType());
            return makeErrorType();
    }
}

TypeInfo SemanticAnalyzer::resolveNamedType(NamedTypeNode* node) {
    if (!node) {
        semanticError("missing named type");
        return makeErrorType();
    }

    const int index = symbolTable.lookupTab(node->name);
    if (index == 0 || symbolTable.tabAt(index).obj != OBJ_TYPE) {
        semanticError("unknown type '" + node->name + "'", node->location);
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    TypeInfo type = symbolTable.typeOf(index);
    annotate(node, type, index);
    return type;
}

TypeInfo SemanticAnalyzer::resolveArrayType(ArrayTypeNode* node) {
    if (!node) {
        semanticError("missing array type");
        return makeErrorType();
    }

    if (node->typeCode == TYPE_ARRAY && node->ref != 0) {
        return typeFromAnnotatedNode(node);
    }

    TypeInfo indexType = resolveType(node->indexType.get());
    TypeInfo elementType = resolveType(node->elementType.get());

    if (indexType.code == TYPE_ERROR || elementType.code == TYPE_ERROR) {
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    if (indexType.code == TYPE_REAL || indexType.code == TYPE_ARRAY || indexType.code == TYPE_RECORD) {
        semanticError("array index type must be a non-Real simple type", node->location);
    }

    ATabEntry entry;
    entry.xtyp = indexType.code == TYPE_SUBRANGE ? indexType.baseType : indexType.code;
    entry.etyp = elementType.code;
    entry.eref = elementType.ref;
    entry.low = indexType.code == TYPE_SUBRANGE ? indexType.low : 0;
    entry.high = indexType.code == TYPE_SUBRANGE ? indexType.high : 0;
    entry.elsz = symbolTable.sizeOf(elementType);

    TypeInfo type;
    type.code = TYPE_ARRAY;
    type.baseType = TYPE_ARRAY;
    type.ref = symbolTable.enterATab(entry);
    type.name = "Array";

    annotate(node, type);
    return type;
}

TypeInfo SemanticAnalyzer::resolveRecordType(RecordTypeNode* node) {
    if (!node) {
        semanticError("missing record type");
        return makeErrorType();
    }

    if (node->typeCode == TYPE_RECORD && node->ref != 0) {
        return typeFromAnnotatedNode(node);
    }

    const int blockIndex = symbolTable.enterBTab(BTabEntry{});
    symbolTable.pushScope(blockIndex);
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();

    int fieldOffset = 0;
    for (const RecordFieldNode& field : node->fields) {
        TypeInfo fieldType = resolveType(field.type.get());

        for (const std::string& name : field.names) {
            if (symbolTable.lookupCurrentScope(name) != 0) {
                semanticError("redeclaration of identifier '" + name + "' in the same scope", field.location);
                continue;
            }

            TabEntry entry;
            entry.identifier = name;
            entry.obj = OBJ_VARIABLE;
            entry.type = fieldType.code;
            entry.ref = fieldType.ref;
            entry.typeInfo = fieldType;
            entry.adr = fieldOffset;

            symbolTable.enterTab(entry);
            fieldOffset += symbolTable.sizeOf(fieldType);
        }
    }

    symbolTable.btabAt(blockIndex).vsze = fieldOffset;
    symbolTable.popScope();
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();

    TypeInfo type;
    type.code = TYPE_RECORD;
    type.baseType = TYPE_RECORD;
    type.ref = blockIndex;
    type.name = "Record";

    annotate(node, type);
    return type;
}

TypeInfo SemanticAnalyzer::resolveSubrangeType(SubrangeTypeNode* node) {
    if (!node) {
        semanticError("missing subrange type");
        return makeErrorType();
    }

    if (node->typeCode == TYPE_SUBRANGE) {
        return typeFromAnnotatedNode(node);
    }

    std::function<std::pair<TypeInfo, std::optional<int>>(ASTNode*, const SourceLocation&)> evaluateBound;
    evaluateBound = [this, &evaluateBound](ASTNode* bound, const SourceLocation& location)
        -> std::pair<TypeInfo, std::optional<int>> {
        if (!bound) {
            semanticError("missing subrange bound", location);
            return {makeErrorType(), std::nullopt};
        }

        if (auto* literal = dynamic_cast<IntLiteralNode*>(bound)) {
            return {makePrimitiveType(TYPE_INTEGER, "Integer"), literal->value};
        }

        if (dynamic_cast<RealLiteralNode*>(bound)) {
            return {makePrimitiveType(TYPE_REAL, "Real"), std::nullopt};
        }

        if (auto* literal = dynamic_cast<CharLiteralNode*>(bound)) {
            return {makePrimitiveType(TYPE_CHAR, "Char"), static_cast<int>(literal->value)};
        }

        if (auto* literal = dynamic_cast<BoolLiteralNode*>(bound)) {
            return {makePrimitiveType(TYPE_BOOLEAN, "Boolean"), literal->value ? 1 : 0};
        }

        if (auto* variable = dynamic_cast<VarNode*>(bound)) {
            const int index = symbolTable.lookupTab(variable->name);
            if (index == 0) {
                semanticError("undeclared constant identifier '" + variable->name + "'", variable->location);
                return {makeErrorType(), std::nullopt};
            }

            const TabEntry& entry = symbolTable.tabAt(index);
            if (entry.obj != OBJ_CONSTANT) {
                semanticError("identifier '" + variable->name + "' is not a constant", variable->location);
                return {makeErrorType(), std::nullopt};
            }

            return {entry.typeInfo, entry.adr};
        }

        if (auto* unary = dynamic_cast<UnaryOpNode*>(bound)) {
            auto [operandType, operandValue] = evaluateBound(unary->operand.get(), unary->location);
            if (operandType.code == TYPE_ERROR) {
                return {operandType, operandValue};
            }

            if ((lower(unary->op) == "-" || lower(unary->op) == "+") && operandValue.has_value()) {
                if (effectiveCode(operandType) != TYPE_INTEGER && effectiveCode(operandType) != TYPE_REAL) {
                    semanticError("unary sign requires a numeric constant", unary->location);
                    return {makeErrorType(), std::nullopt};
                }

                if (lower(unary->op) == "-") {
                    operandValue = -operandValue.value();
                }
                return {operandType, operandValue};
            }
        }

        semanticError("unsupported constant bound in subrange type", bound->location);
        return {makeErrorType(), std::nullopt};
    };

    auto [lowerType, lowerValue] = evaluateBound(node->lowerBound.get(), node->location);
    auto [upperType, upperValue] = evaluateBound(node->upperBound.get(), node->location);

    if (lowerType.code == TYPE_ERROR || upperType.code == TYPE_ERROR) {
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    if (lowerType.code != upperType.code) {
        semanticError("subrange bounds must have the same type", node->location);
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    if (lowerType.code == TYPE_REAL) {
        semanticError("subrange cannot use Real bounds", node->location);
        annotate(node, makeErrorType());
        return makeErrorType();
    }

    if (lowerValue.has_value() && upperValue.has_value() && lowerValue.value() > upperValue.value()) {
        semanticError("subrange lower bound cannot be greater than upper bound", node->location);
    }

    TypeInfo type;
    type.code = TYPE_SUBRANGE;
    type.baseType = lowerType.code;
    type.low = lowerValue.value_or(0);
    type.high = upperValue.value_or(0);

    std::ostringstream name;
    name << "Subrange(" << type.low << ".." << type.high << ")";
    type.name = name.str();

    annotate(node, type);
    return type;
}

TypeInfo SemanticAnalyzer::resolveEnumType(EnumTypeNode* node) {
    if (!node) {
        semanticError("missing enumerated type");
        return makeErrorType();
    }

    if (node->typeCode == TYPE_ENUM) {
        return typeFromAnnotatedNode(node);
    }

    TypeInfo type;
    type.code = TYPE_ENUM;
    type.baseType = TYPE_ENUM;
    type.low = 0;
    type.high = node->values.empty() ? 0 : static_cast<int>(node->values.size()) - 1;
    type.name = "Enum";

    annotate(node, type);
    return type;
}

bool SemanticAnalyzer::isCompatible(const TypeInfo& lhs, const TypeInfo& rhs) const {
    if (lhs.code == TYPE_ERROR || rhs.code == TYPE_ERROR) {
        return true;
    }

    if (lhs.code == TYPE_STRING && rhs.code == TYPE_STRING) {
        if (lhs.stringLength >= 0 && rhs.stringLength >= 0) {
            return lhs.stringLength == rhs.stringLength;
        }
        return true;
    }

    if (lhs.code == TYPE_SUBRANGE && rhs.code == TYPE_SUBRANGE) {
        return lhs.baseType == rhs.baseType;
    }

    if (lhs.code == TYPE_SUBRANGE) {
        return lhs.baseType == effectiveCode(rhs);
    }

    if (rhs.code == TYPE_SUBRANGE) {
        return rhs.baseType == effectiveCode(lhs);
    }

    if (lhs.code != rhs.code) {
        return false;
    }

    if (lhs.code == TYPE_ENUM && (lhs.isNamed || rhs.isNamed)) {
        return lhs.isNamed && rhs.isNamed && !lhs.name.empty() && lhs.name == rhs.name;
    }

    if (lhs.code == TYPE_ARRAY || lhs.code == TYPE_RECORD) {
        if (lhs.isNamed || rhs.isNamed) {
            return lhs.isNamed && rhs.isNamed && !lhs.name.empty() && lhs.name == rhs.name;
        }

        return lhs.ref != 0 && lhs.ref == rhs.ref;
    }

    return true;
}

bool SemanticAnalyzer::isAssignmentCompatible(const TypeInfo& target, const TypeInfo& value) const {
    if (target.code == TYPE_ERROR || value.code == TYPE_ERROR) {
        return true;
    }

    if (target.code == TYPE_REAL && effectiveCode(value) == TYPE_INTEGER) {
        return true;
    }

    if (!isCompatible(target, value)) {
        return false;
    }

    if (target.code == TYPE_STRING && value.code == TYPE_STRING) {
        return true;
    }

    if (target.code == TYPE_ARRAY || target.code == TYPE_RECORD) {
        return true;
    }

    const int targetCode = effectiveCode(target);
    if (targetCode == TYPE_INTEGER ||
        targetCode == TYPE_BOOLEAN ||
        targetCode == TYPE_CHAR ||
        targetCode == TYPE_ENUM) {
        return true;
    }

    return target.code == value.code || target.code == TYPE_SUBRANGE || value.code == TYPE_SUBRANGE;
}

bool SemanticAnalyzer::isOrdinal(const TypeInfo& type) const {
    const int code = effectiveCode(type);
    return code == TYPE_INTEGER || code == TYPE_CHAR || code == TYPE_BOOLEAN || code == TYPE_ENUM;
}

bool SemanticAnalyzer::isNumeric(const TypeInfo& type) const {
    const int code = effectiveCode(type);
    return code == TYPE_INTEGER || code == TYPE_REAL;
}

bool SemanticAnalyzer::isBoolean(const TypeInfo& type) const {
    return effectiveCode(type) == TYPE_BOOLEAN;
}
