#include "codegen.hpp"
#include "text_utils.hpp"

#include <string>
#include <utility>

namespace {

Instruction makeLiteral(RuntimeValue value) {
    Instruction instruction;
    instruction.opcode = OpCode::LIT;
    instruction.literal = std::move(value);
    instruction.hasLiteral = true;
    return instruction;
}

Instruction makeOperation(OperationCode operation) {
    Instruction instruction;
    instruction.opcode = OpCode::OPR;
    instruction.operand = static_cast<int>(operation);
    return instruction;
}

Instruction makeJump(OpCode opcode) {
    Instruction instruction;
    instruction.opcode = opcode;
    return instruction;
}

bool mapBinaryOperator(const std::string& op, OperationCode& operation) {
    const std::string lowered = text_util::lowercase(op);

    if (lowered == "+") operation = OperationCode::ADD;
    else if (lowered == "-") operation = OperationCode::SUB;
    else if (lowered == "*") operation = OperationCode::MUL;
    else if (lowered == "div") operation = OperationCode::DIV;
    else if (lowered == "mod") operation = OperationCode::MOD;
    else if (lowered == "==") operation = OperationCode::EQL;
    else if (lowered == "<>") operation = OperationCode::NEQ;
    else if (lowered == "<") operation = OperationCode::LSS;
    else if (lowered == ">=") operation = OperationCode::GEQ;
    else if (lowered == ">") operation = OperationCode::GTR;
    else if (lowered == "<=") operation = OperationCode::LEQ;
    else return false;

    return true;
}

const TabEntry* entryAt(const SymbolTable* symbols, int index) {
    if (!symbols || index <= 0 || index >= static_cast<int>(symbols->tab().size())) {
        return nullptr;
    }
    return &symbols->tabAt(index);
}

} // namespace

void CodeGenerator::generateExpression(const ExpressionNode& node) {
    switch (node.kind) {
        case ASTNodeKind::IntLiteral: {
            const auto& literal = static_cast<const IntLiteralNode&>(node);
            emit(makeLiteral(RuntimeValue::integer(literal.value)));
            break;
        }
        case ASTNodeKind::RealLiteral: {
            const auto& literal = static_cast<const RealLiteralNode&>(node);
            emit(makeLiteral(RuntimeValue::real(literal.value)));
            break;
        }
        case ASTNodeKind::CharLiteral: {
            const auto& literal = static_cast<const CharLiteralNode&>(node);
            emit(makeLiteral(RuntimeValue::character(literal.value)));
            break;
        }
        case ASTNodeKind::StringLiteral: {
            const auto& literal = static_cast<const StringLiteralNode&>(node);
            emit(makeLiteral(RuntimeValue::string(literal.value)));
            break;
        }
        case ASTNodeKind::BoolLiteral: {
            const auto& literal = static_cast<const BoolLiteralNode&>(node);
            emit(makeLiteral(RuntimeValue::boolean(literal.value)));
            break;
        }
        case ASTNodeKind::Var:
        case ASTNodeKind::ArrayAccess:
        case ASTNodeKind::RecordAccess:
            emitLoadAddressable(node);
            break;
        case ASTNodeKind::UnaryOp: {
            const auto& unary = static_cast<const UnaryOpNode&>(node);
            if (!unary.operand) {
                diagnostic("missing unary operand", node.location);
                return;
            }

            const std::string op = text_util::lowercase(unary.op);
            if (op == "+") {
                generateExpression(*unary.operand);
            } else if (op == "-") {
                generateExpression(*unary.operand);
                emit(makeOperation(OperationCode::NEG));
            } else if (op == "not") {
                generateExpression(*unary.operand);
                emit(makeLiteral(RuntimeValue::boolean(false)));
                emit(makeOperation(OperationCode::EQL));
            } else {
                diagnostic("unsupported unary operator '" + unary.op + "'", node.location);
            }
            break;
        }
        case ASTNodeKind::BinOp: {
            const auto& binary = static_cast<const BinOpNode&>(node);
            if (!binary.left || !binary.right) {
                diagnostic("missing binary operand", node.location);
                return;
            }

            const std::string op = text_util::lowercase(binary.op);

            if (op == "/") {
                diagnostic(
                    "operator '/' is defined by earlier milestones as real division, "
                    "but milestone 4 does not define a real-division OPR code",
                    node.location
                );
                return;
            }

            if (op == "and") {
                generateExpression(*binary.left);
                int leftFalse = emit(makeJump(OpCode::JPC));
                generateExpression(*binary.right);
                int rightFalse = emit(makeJump(OpCode::JPC));
                emit(makeLiteral(RuntimeValue::boolean(true)));
                int endJump = emit(makeJump(OpCode::JMP));
                int falseAddress = nextAddress();
                patchOperand(leftFalse, falseAddress);
                patchOperand(rightFalse, falseAddress);
                emit(makeLiteral(RuntimeValue::boolean(false)));
                patchOperand(endJump, nextAddress());
                break;
            }

            if (op == "or") {
                generateExpression(*binary.left);
                int evalRight = emit(makeJump(OpCode::JPC));
                emit(makeLiteral(RuntimeValue::boolean(true)));
                int leftEnd = emit(makeJump(OpCode::JMP));
                patchOperand(evalRight, nextAddress());
                generateExpression(*binary.right);
                int falseJump = emit(makeJump(OpCode::JPC));
                emit(makeLiteral(RuntimeValue::boolean(true)));
                int rightEnd = emit(makeJump(OpCode::JMP));
                patchOperand(falseJump, nextAddress());
                emit(makeLiteral(RuntimeValue::boolean(false)));
                int end = nextAddress();
                patchOperand(leftEnd, end);
                patchOperand(rightEnd, end);
                break;
            }

            OperationCode operation;
            if (!mapBinaryOperator(binary.op, operation)) {
                diagnostic("unsupported binary operator '" + binary.op + "'", node.location);
                return;
            }

            generateExpression(*binary.left);
            generateExpression(*binary.right);
            emit(makeOperation(operation));
            break;
        }
        case ASTNodeKind::FuncCall: {
            const auto& call = static_cast<const FuncCallNode&>(node);
            const TabEntry* entry = entryAt(symbolTable, node.tabIndex);
            int functionIndex = node.tabIndex;

            if (!entry && symbolTable) {
                functionIndex = symbolTable->lookupTab(call.name);
                entry = entryAt(symbolTable, functionIndex);
            }

            if (!entry || entry->obj != OBJ_FUNCTION) {
                diagnostic("unknown function '" + call.name + "'", node.location);
                return;
            }

            const int address = routineAddress(functionIndex);
            if (address < 0) {
                diagnostic("function '" + call.name + "' does not have a generated entry address", node.location);
                return;
            }

            const std::vector<int> parameterIndices = routineParameterIndices(entry->ref);
            if (parameterIndices.size() != call.arguments.size()) {
                diagnostic("function '" + call.name + "' argument count does not match metadata", node.location);
                return;
            }

            for (size_t i = 0; i < call.arguments.size(); ++i) {
                const auto& argument = call.arguments[i];
                if (!argument) {
                    diagnostic("function '" + call.name + "' has an empty argument", node.location);
                    return;
                }

                const size_t diagnosticCount = result.diagnostics.size();
                const TabEntry& parameter = symbolTable->tabAt(parameterIndices[i]);
                if (parameter.nrm == 0) {
                    emitAddressAddressable(*argument);
                } else {
                    generateExpression(*argument);
                }

                if (result.diagnostics.size() != diagnosticCount) {
                    return;
                }
            }

            Instruction instruction;
            instruction.opcode = OpCode::CAL;
            instruction.level = lexicalLevelOffset(*entry);
            instruction.operand = address;
            emit(instruction);
            break;
        }
        default:
            diagnostic("unsupported expression node", node.location);
            break;
    }
}

bool CodeGenerator::emitLoadAddressable(const ExpressionNode& node) {
    if (node.kind == ASTNodeKind::ArrayAccess ||
        node.kind == ASTNodeKind::RecordAccess) {
        const TypeInfo valueType = expressionTypeInfo(node);
        if (isStructuredType(valueType)) {
            diagnostic("structured value load is not implemented yet", node.location);
            return false;
        }

        const size_t diagnosticCount = result.diagnostics.size();
        emitAddressAddressable(node);
        if (result.diagnostics.size() != diagnosticCount) {
            return false;
        }

        Instruction instruction;
        instruction.opcode = OpCode::LDI;
        emit(instruction);
        return true;
    }

    if (node.kind != ASTNodeKind::Var) {
        diagnostic("expression is not addressable", node.location);
        return false;
    }

    const auto& variable = static_cast<const VarNode&>(node);
    const TabEntry* entry = entryAt(symbolTable, node.tabIndex);

    if (!entry && symbolTable) {
        entry = entryAt(symbolTable, symbolTable->lookupTab(variable.name));
    }

    if (!entry) {
        diagnostic("unknown identifier '" + variable.name + "'", node.location);
        return false;
    }

    if (entry->obj == OBJ_CONSTANT) {
        return emitConstant(*entry);
    }

    if (isCurrentFunctionResult(*entry)) {
        if (isStructuredType(entry->typeInfo)) {
            diagnostic("structured function result load is not implemented yet", node.location);
            return false;
        }

        Instruction instruction;
        instruction.opcode = OpCode::LOD;
        instruction.level = 0;
        instruction.operand = variableAddress(*entry);
        emit(instruction);
        return true;
    }

    if (entry->obj != OBJ_VARIABLE) {
        diagnostic("identifier '" + variable.name + "' is not a value", node.location);
        return false;
    }

    if (isStructuredType(entry->typeInfo)) {
        diagnostic("structured value load is not implemented yet", node.location);
        return false;
    }

    Instruction instruction;
    instruction.opcode = OpCode::LOD;
    instruction.level = lexicalLevelOffset(*entry);
    instruction.operand = variableAddress(*entry);
    instruction.indirect = entry->nrm == 0;
    emit(instruction);
    return true;
}

bool CodeGenerator::emitAddressAddressable(const ExpressionNode& node) {
    if (node.kind == ASTNodeKind::ArrayAccess) {
        const auto& access = static_cast<const ArrayAccessNode&>(node);
        if (!access.array) {
            diagnostic("array access is missing its base expression", node.location);
            return false;
        }

        TypeInfo currentType = expressionTypeInfo(*access.array);
        const size_t diagnosticCount = result.diagnostics.size();
        emitAddressAddressable(*access.array);
        if (result.diagnostics.size() != diagnosticCount) {
            return false;
        }

        for (const auto& index : access.indices) {
            if (!index) {
                diagnostic("array access has an empty index expression", node.location);
                return false;
            }

            if (currentType.code != TYPE_ARRAY ||
                currentType.ref <= 0 ||
                currentType.ref >= static_cast<int>(symbolTable->atab().size())) {
                diagnostic("subscripted expression is not an array", node.location);
                return false;
            }

            const ATabEntry& arrayEntry = symbolTable->atabAt(currentType.ref);
            generateExpression(*index);
            if (result.diagnostics.size() != diagnosticCount) {
                return false;
            }

            if (arrayEntry.low != 0) {
                emit(makeLiteral(RuntimeValue::integer(arrayEntry.low)));
                emit(makeOperation(OperationCode::SUB));
            }

            if (arrayEntry.elsz != 1) {
                emit(makeLiteral(RuntimeValue::integer(arrayEntry.elsz)));
                emit(makeOperation(OperationCode::MUL));
            }

            emit(makeOperation(OperationCode::ADD));
            currentType = arrayElementType(arrayEntry);
        }

        return true;
    }

    if (node.kind == ASTNodeKind::RecordAccess) {
        const auto& access = static_cast<const RecordAccessNode&>(node);
        if (!access.record) {
            diagnostic("record access is missing its base expression", node.location);
            return false;
        }

        TypeInfo recordType = expressionTypeInfo(*access.record);
        if (recordType.code != TYPE_RECORD || recordType.ref <= 0) {
            diagnostic("field access requires a record operand", node.location);
            return false;
        }

        const int fieldIndex = symbolTable ? symbolTable->lookupTab(access.fieldName, recordType.ref) : 0;
        if (fieldIndex <= 0) {
            diagnostic("record type does not contain field '" + access.fieldName + "'", node.location);
            return false;
        }

        const size_t diagnosticCount = result.diagnostics.size();
        emitAddressAddressable(*access.record);
        if (result.diagnostics.size() != diagnosticCount) {
            return false;
        }

        const TabEntry& field = symbolTable->tabAt(fieldIndex);
        if (field.adr != 0) {
            emit(makeLiteral(RuntimeValue::integer(field.adr)));
            emit(makeOperation(OperationCode::ADD));
        }

        return true;
    }

    if (node.kind != ASTNodeKind::Var) {
        diagnostic("expression is not addressable", node.location);
        return false;
    }

    const auto& variable = static_cast<const VarNode&>(node);
    const TabEntry* entry = entryAt(symbolTable, node.tabIndex);

    if (!entry && symbolTable) {
        entry = entryAt(symbolTable, symbolTable->lookupTab(variable.name));
    }

    if (!entry) {
        diagnostic("unknown identifier '" + variable.name + "'", node.location);
        return false;
    }

    if (entry->obj != OBJ_VARIABLE) {
        diagnostic("identifier '" + variable.name + "' is not addressable", node.location);
        return false;
    }

    Instruction instruction;
    if (entry->nrm == 0) {
        instruction.opcode = OpCode::LOD;
        instruction.level = lexicalLevelOffset(*entry);
        instruction.operand = variableAddress(*entry);
    } else {
        instruction.opcode = OpCode::LDA;
        instruction.level = lexicalLevelOffset(*entry);
        instruction.operand = variableAddress(*entry);
    }

    emit(instruction);
    return true;
}

bool CodeGenerator::emitConstant(const TabEntry& entry) {
    switch (entry.typeInfo.code) {
        case TYPE_INTEGER:
        case TYPE_ENUM:
            emit(makeLiteral(RuntimeValue::integer(
                entry.hasConstantOrdinal ? entry.constantOrdinalValue : entry.adr
            )));
            return true;
        case TYPE_CHAR:
            emit(makeLiteral(RuntimeValue::character(static_cast<char>(
                entry.hasConstantOrdinal ? entry.constantOrdinalValue : entry.adr
            ))));
            return true;
        case TYPE_BOOLEAN:
            emit(makeLiteral(RuntimeValue::boolean(
                (entry.hasConstantOrdinal ? entry.constantOrdinalValue : entry.adr) != 0
            )));
            return true;
        case TYPE_REAL:
            if (!entry.hasConstantReal) return false;
            emit(makeLiteral(RuntimeValue::real(entry.constantRealValue)));
            return true;
        case TYPE_STRING:
            if (!entry.hasConstantString) return false;
            emit(makeLiteral(RuntimeValue::string(entry.constantStringValue)));
            return true;
        default:
            diagnostic("constant '" + entry.identifier + "' has unsupported type");
            return false;
    }
}
