#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
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

TypeInfo makeVoidType() {
    TypeInfo type;
    type.code = TYPE_VOID;
    type.baseType = TYPE_VOID;
    type.name = "Void";
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

bool isValueLikeTarget(const ExpressionNode* node) {
    if (!node) {
        return false;
    }

    return node->kind == ASTNodeKind::Var ||
           node->kind == ASTNodeKind::ArrayAccess ||
           node->kind == ASTNodeKind::RecordAccess;
}

bool isBuiltinOutput(const std::string& name) {
    const std::string lowered = lower(name);
    return lowered == "write" || lowered == "writeln";
}

bool isBuiltinInput(const std::string& name) {
    const std::string lowered = lower(name);
    return lowered == "read" || lowered == "readln";
}

std::vector<int> collectRoutineParameterIndices(const SymbolTable& symbols, int blockIndex) {
    std::vector<int> indices;
    if (blockIndex <= 0) {
        return indices;
    }

    int parameterIndex = symbols.btabAt(blockIndex).lpar;
    while (parameterIndex != 0) {
        indices.push_back(parameterIndex);
        parameterIndex = symbols.tabAt(parameterIndex).link;
    }

    std::reverse(indices.begin(), indices.end());
    return indices;
}

struct ConstantValue {
    TypeInfo type = makeErrorType();
    bool knownOrdinal = false;
    int ordinalValue = 0;
    bool knownString = false;
    std::string stringValue;
};

std::optional<ConstantValue> tryEvaluateConstant(const ExpressionNode* node, const SymbolTable& symbols) {
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

    if (target.code == TYPE_STRING &&
        target.stringLength >= 0 &&
        value->type.code == TYPE_STRING &&
        value->type.stringLength >= 0) {
        return target.stringLength == value->type.stringLength;
    }

    return true;
}

}

void SemanticAnalyzer::visitStatement(StatementNode* node) {
    if (!node) {
        return;
    }

    switch (node->kind) {
        case ASTNodeKind::Block:
            visitBlock(static_cast<BlockNode*>(node));
            annotate(node, makeVoidType());
            break;
        case ASTNodeKind::Assign:
            visitAssign(static_cast<AssignNode*>(node));
            break;
        case ASTNodeKind::If:
            visitIf(static_cast<IfNode*>(node));
            break;
        case ASTNodeKind::While:
            visitWhile(static_cast<WhileNode*>(node));
            break;
        case ASTNodeKind::For:
            visitFor(static_cast<ForNode*>(node));
            break;
        case ASTNodeKind::Repeat:
            visitRepeat(static_cast<RepeatNode*>(node));
            break;
        case ASTNodeKind::Case:
            visitCase(static_cast<CaseNode*>(node));
            break;
        case ASTNodeKind::ProcCall:
            visitProcCall(static_cast<ProcCallNode*>(node));
            break;
        default:
            semanticError("unsupported statement node", node->location);
            annotate(node, makeErrorType());
            break;
    }
}

void SemanticAnalyzer::visitAssign(AssignNode* node) {
    if (!node) {
        semanticError("missing assignment statement");
        return;
    }

    TypeInfo targetType = visitExpression(node->target.get());
    TypeInfo valueType = visitExpression(node->value.get());

    if (!isValueLikeTarget(node->target.get())) {
        semanticError("left-hand side of assignment must be assignable", node->location);
    } else if (node->target && node->target->tabIndex > 0) {
        const TabEntry& entry = symbolTable.tabAt(node->target->tabIndex);
        if (!isAssignableEntry(entry)) {
            semanticError("left-hand side of assignment is not assignable", node->target->location);
        }
    }

    if (!isAssignmentCompatible(targetType, valueType)) {
        semanticError(
            "cannot assign " + symbolTable.typeName(valueType) +
            " to " + symbolTable.typeName(targetType),
            node->location
        );
    } else if (!constantFitsTarget(targetType, node->value.get(), symbolTable)) {
        semanticError("assigned constant is outside the target type range", node->value ? node->value->location : node->location);
    }

    annotate(node, makeVoidType());
}

void SemanticAnalyzer::visitIf(IfNode* node) {
    if (!node) {
        semanticError("missing if statement");
        return;
    }

    TypeInfo conditionType = visitExpression(node->condition.get());
    if (!isBoolean(conditionType)) {
        semanticError("if condition must have Boolean type", node->condition ? node->condition->location : node->location);
    }

    visitStatement(node->thenBranch.get());
    visitStatement(node->elseBranch.get());
    annotate(node, makeVoidType());
}

void SemanticAnalyzer::visitWhile(WhileNode* node) {
    if (!node) {
        semanticError("missing while statement");
        return;
    }

    TypeInfo conditionType = visitExpression(node->condition.get());
    if (!isBoolean(conditionType)) {
        semanticError("while condition must have Boolean type", node->condition ? node->condition->location : node->location);
    }

    visitStatement(node->body.get());

    annotate(node, makeVoidType());
}

void SemanticAnalyzer::visitFor(ForNode* node) {
    if (!node) {
        semanticError("missing for statement");
        return;
    }

    const int index = symbolTable.lookupTab(node->controlVariable);
    TypeInfo controlType = makeErrorType();

    if (index == 0) {
        semanticError("undeclared identifier '" + node->controlVariable + "'", node->location);
    } else {
        const TabEntry& entry = symbolTable.tabAt(index);
        controlType = entry.typeInfo;

        if (entry.obj != OBJ_VARIABLE) {
            semanticError("for control variable must be a variable", node->location);
        }

        if (!isOrdinal(controlType)) {
            semanticError("for control variable must have ordinal type", node->location);
        }
    }

    TypeInfo startType = visitExpression(node->startValue.get());
    TypeInfo endType = visitExpression(node->endValue.get());

    if (!isAssignmentCompatible(controlType, startType)) {
        semanticError("for initial value is incompatible with control variable type", node->startValue ? node->startValue->location : node->location);
    } else if (!constantFitsTarget(controlType, node->startValue.get(), symbolTable)) {
        semanticError("for initial value is outside the control variable range", node->startValue ? node->startValue->location : node->location);
    }

    if (!isAssignmentCompatible(controlType, endType)) {
        semanticError("for final value is incompatible with control variable type", node->endValue ? node->endValue->location : node->location);
    } else if (!constantFitsTarget(controlType, node->endValue.get(), symbolTable)) {
        semanticError("for final value is outside the control variable range", node->endValue ? node->endValue->location : node->location);
    }

    visitStatement(node->body.get());

    annotate(node, makeVoidType());
}

void SemanticAnalyzer::visitRepeat(RepeatNode* node) {
    if (!node) {
        semanticError("missing repeat statement");
        return;
    }

    for (const auto& statement : node->body) {
        visitStatement(statement.get());
    }

    TypeInfo conditionType = visitExpression(node->condition.get());
    if (!isBoolean(conditionType)) {
        semanticError("repeat-until condition must have Boolean type", node->condition ? node->condition->location : node->location);
    }

    annotate(node, makeVoidType());
}

void SemanticAnalyzer::visitCase(CaseNode* node) {
    if (!node) {
        semanticError("missing case statement");
        return;
    }

    TypeInfo selectorType = visitExpression(node->selector.get());
    if (selectorType.code == TYPE_ARRAY || selectorType.code == TYPE_RECORD) {
        semanticError("case selector cannot have a structured type", node->selector ? node->selector->location : node->location);
    }

    for (const CaseBranchNode& branch : node->branches) {
        for (const auto& label : branch.labels) {
            TypeInfo labelType = visitExpression(label.get());
            if (!tryEvaluateConstant(label.get(), symbolTable).has_value()) {
                semanticError("case label must be a constant expression", label ? label->location : branch.location);
            }

            if (!isCompatible(selectorType, labelType)) {
                semanticError(
                    "case label type " + symbolTable.typeName(labelType) +
                    " is incompatible with selector type " + symbolTable.typeName(selectorType),
                    label ? label->location : branch.location
                );
            }
        }

        visitStatement(branch.statement.get());
    }

    annotate(node, makeVoidType());
}

void SemanticAnalyzer::visitProcCall(ProcCallNode* node) {
    if (!node) {
        semanticError("missing procedure call");
        return;
    }

    const int index = symbolTable.lookupTab(node->name);
    if (index == 0) {
        semanticError("undeclared procedure '" + node->name + "'", node->location);
        annotate(node, makeErrorType());
        return;
    }

    std::vector<TypeInfo> argumentTypes;
    argumentTypes.reserve(node->arguments.size());
    for (const auto& argument : node->arguments) {
        argumentTypes.push_back(visitExpression(argument.get()));
    }

    const TabEntry& entry = symbolTable.tabAt(index);
    if (isBuiltinOutput(node->name)) {
        annotate(node, makeVoidType(), index);
        return;
    }

    if (isBuiltinInput(node->name)) {
        for (size_t i = 0; i < node->arguments.size(); ++i) {
            ExpressionNode* argument = node->arguments[i].get();
            if (!isValueLikeTarget(argument)) {
                semanticError(
                    "argument " + std::to_string(i + 1) + " of '" + node->name +
                    "' must be assignable",
                    argument ? argument->location : node->location
                );
                continue;
            }

            if (argument && argument->tabIndex > 0) {
                const TabEntry& target = symbolTable.tabAt(argument->tabIndex);
                if (!isAssignableEntry(target)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of '" + node->name +
                        "' is not assignable",
                        argument->location
                    );
                }
            }
        }

        annotate(node, makeVoidType(), index);
        return;
    }

    if (entry.obj != OBJ_PROCEDURE) {
        semanticError("identifier '" + node->name + "' is not a procedure", node->location);
        annotate(node, makeErrorType(), index);
        return;
    }

    const std::vector<int> parameterIndices = collectRoutineParameterIndices(symbolTable, entry.ref);
    if (node->arguments.size() != parameterIndices.size()) {
        semanticError(
            "procedure '" + node->name + "' expects " +
            std::to_string(parameterIndices.size()) + " argument(s), got " +
            std::to_string(node->arguments.size()),
            node->location
        );
    } else {
        for (size_t i = 0; i < parameterIndices.size(); ++i) {
            const TabEntry& parameter = symbolTable.tabAt(parameterIndices[i]);
            ExpressionNode* argumentNode = node->arguments[i].get();
            const TypeInfo argumentType = argumentTypes[i];

            if (parameter.nrm == 0) {
                if (!isValueLikeTarget(argumentNode)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of procedure '" + node->name +
                        "' must be assignable for by-reference parameter",
                        argumentNode ? argumentNode->location : node->location
                    );
                } else if (argumentNode && argumentNode->tabIndex > 0) {
                    const TabEntry& target = symbolTable.tabAt(argumentNode->tabIndex);
                    if (!isAssignableEntry(target)) {
                        semanticError(
                            "argument " + std::to_string(i + 1) + " of procedure '" + node->name +
                            "' is not assignable",
                            argumentNode->location
                        );
                    }
                }

                if (!isCompatible(parameter.typeInfo, argumentType)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of procedure '" + node->name +
                        "' expects " + symbolTable.typeName(parameter.typeInfo) +
                        ", got " + symbolTable.typeName(argumentType),
                        argumentNode ? argumentNode->location : node->location
                    );
                }
            } else {
                if (!isAssignmentCompatible(parameter.typeInfo, argumentType)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of procedure '" + node->name +
                        "' expects " + symbolTable.typeName(parameter.typeInfo) +
                        ", got " + symbolTable.typeName(argumentType),
                        argumentNode ? argumentNode->location : node->location
                    );
                } else if (!constantFitsTarget(parameter.typeInfo, argumentNode, symbolTable)) {
                    semanticError(
                        "argument " + std::to_string(i + 1) + " of procedure '" + node->name +
                        "' is outside the parameter type range",
                        argumentNode ? argumentNode->location : node->location
                    );
                }
            }
        }
    }

    annotate(node, makeVoidType(), index);
}
