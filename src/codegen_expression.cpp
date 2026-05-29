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
        case ASTNodeKind::FuncCall:
            diagnostic("function call expressions are not implemented yet", node.location);
            break;
        default:
            diagnostic("unsupported expression node", node.location);
            break;
    }
}

bool CodeGenerator::emitLoadAddressable(const ExpressionNode& node) {
    if (node.kind == ASTNodeKind::ArrayAccess) {
        diagnostic("array access code generation is not implemented yet", node.location);
        return false;
    }

    if (node.kind == ASTNodeKind::RecordAccess) {
        diagnostic("record access code generation is not implemented yet", node.location);
        return false;
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

    if (entry->obj != OBJ_VARIABLE) {
        diagnostic("identifier '" + variable.name + "' is not a value", node.location);
        return false;
    }

    Instruction instruction;
    instruction.opcode = OpCode::LOD;
    instruction.operand = variableAddress(*entry);
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
