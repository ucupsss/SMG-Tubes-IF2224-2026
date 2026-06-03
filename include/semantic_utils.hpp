#ifndef SEMANTIC_UTILS_HPP
#define SEMANTIC_UTILS_HPP

#include "ast.hpp"
#include "symbol_table.hpp"
#include "text_utils.hpp"

#include <optional>
#include <string>

namespace semantic_util {

inline TypeInfo makeErrorType() {
    TypeInfo type;
    type.code = TYPE_ERROR;
    type.baseType = TYPE_ERROR;
    type.name = "Error";
    return type;
}

inline TypeInfo makeVoidType() {
    TypeInfo type;
    type.code = TYPE_VOID;
    type.baseType = TYPE_VOID;
    type.name = "Void";
    return type;
}

inline TypeInfo makePrimitiveType(int code, const std::string& name) {
    TypeInfo type;
    type.code = code;
    type.baseType = code;
    type.name = name;
    type.isNamed = true;
    return type;
}

inline int effectiveCode(const TypeInfo& type) {
    return type.code == TYPE_SUBRANGE ? type.baseType : type.code;
}

inline bool isValueLikeTarget(const ExpressionNode* node) {
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
    bool knownString = false;
    std::string stringValue;
};

inline std::optional<ConstantValue> tryEvaluateConstant(
    const ExpressionNode* node,
    const SymbolTable& symbols
) {
    if (!node) {
        return std::nullopt;
    }

    if (const auto* literal = dynamic_cast<const IntLiteralNode*>(node)) {
        return ConstantValue{makePrimitiveType(TYPE_INTEGER, "Integer"), true, literal->value, false, ""};
    }

    if (dynamic_cast<const RealLiteralNode*>(node)) {
        return ConstantValue{makePrimitiveType(TYPE_REAL, "Real"), false, 0, false, ""};
    }

    if (const auto* literal = dynamic_cast<const CharLiteralNode*>(node)) {
        return ConstantValue{
            makePrimitiveType(TYPE_CHAR, "Char"),
            true,
            static_cast<int>(literal->value),
            false,
            ""
        };
    }

    if (const auto* literal = dynamic_cast<const BoolLiteralNode*>(node)) {
        return ConstantValue{
            makePrimitiveType(TYPE_BOOLEAN, "Boolean"),
            true,
            literal->value ? 1 : 0,
            false,
            ""
        };
    }

    if (const auto* literal = dynamic_cast<const StringLiteralNode*>(node)) {
        ConstantValue value;
        value.type = makePrimitiveType(TYPE_STRING, "String");
        value.type.stringLength = static_cast<int>(literal->value.size());
        value.knownString = true;
        value.stringValue = literal->value;
        return value;
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

        const std::string op = text_util::lowercase(unary->op);
        if ((op == "+" || op == "-") &&
            (operand->type.code == TYPE_INTEGER ||
             operand->type.code == TYPE_REAL ||
             operand->type.code == TYPE_SUBRANGE)) {
            if (operand->knownOrdinal && op == "-") {
                operand->ordinalValue = -operand->ordinalValue;
            }
            return operand;
        }
    }

    return std::nullopt;
}

inline bool constantFitsTarget(
    const TypeInfo& target,
    const ExpressionNode* node,
    const SymbolTable& symbols
) {
    const auto value = tryEvaluateConstant(node, symbols);
    if (!value.has_value()) {
        return true;
    }

    if (target.code == TYPE_SUBRANGE && value->knownOrdinal) {
        return value->ordinalValue >= target.low && value->ordinalValue <= target.high;
    }

    if (target.code == TYPE_STRING &&
        target.stringLength >= 0 &&
        value->type.code == TYPE_STRING &&
        value->type.stringLength >= 0) {
        return target.stringLength == value->type.stringLength;
    }

    return true;
}

} // namespace semantic_util

#endif
