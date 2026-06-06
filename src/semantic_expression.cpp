#include "semantic.hpp"
#include "semantic_utils.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using semantic_util::constantFitsTarget;
using semantic_util::effectiveCode;
using semantic_util::isValueLikeTarget;
using semantic_util::makeErrorType;
using semantic_util::makePrimitiveType;
using semantic_util::tryEvaluateConstant;
using text_util::lowercase;

bool isStructured(const TypeInfo& type) {
    return type.code == TYPE_ARRAY || type.code == TYPE_RECORD;
}

bool isRelationalOperator(const std::string& op) {
    return op == "==" || op == "<>" || op == "<" || op == "<=" || op == ">" || op == ">=";
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
    const std::string op = lowercase(node->op);

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
    const std::string op = lowercase(node->op);

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
        } else if (expectedIndexType.code == TYPE_SUBRANGE) {
            const auto value = tryEvaluateConstant(indexExpr.get(), symbolTable);
            if (value.has_value() &&
                value->knownOrdinal &&
                (value->ordinalValue < expectedIndexType.low || value->ordinalValue > expectedIndexType.high)) {
                semanticError(
                    "array index is outside the declared subrange",
                    indexExpr ? indexExpr->location : node->location
                );
            }
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
                    if (!isAssignableEntry(target)) {
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
