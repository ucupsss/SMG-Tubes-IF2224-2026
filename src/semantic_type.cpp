#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <sstream>
#include <utility>

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

TypeInfo typeFromAnnotatedNode(const TypeNode* node) {
    TypeInfo type;
    type.code = node->typeCode;
    type.baseType = node->typeCode == TYPE_SUBRANGE ? TYPE_INTEGER : node->typeCode;
    type.ref = node->ref;
    type.isNamed = node->isNamed;
    type.name = node->typeName;
    return type;
}

} // namespace

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

bool SemanticAnalyzer::isAssignableEntry(const TabEntry& entry) const {
    return entry.obj == OBJ_VARIABLE ||
           (entry.obj == OBJ_FUNCTION && entry.ref == symbolTable.currentBlock());
}
