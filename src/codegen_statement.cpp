#include "codegen.hpp"
#include "text_utils.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {

Instruction makeJump(OpCode opcode, int operand = 0) {
    Instruction instruction;
    instruction.opcode = opcode;
    instruction.operand = operand;
    return instruction;
}

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

bool isBuiltinOutput(const std::string& name) {
    const std::string lowered = text_util::lowercase(name);
    return lowered == "write" || lowered == "writeln";
}

bool isBuiltinInput(const std::string& name) {
    const std::string lowered = text_util::lowercase(name);
    return lowered == "read" || lowered == "readln";
}

const TabEntry* resolveVariableEntry(const SymbolTable* symbols, const VarNode& variable, int tabIndex) {
    if (!symbols) {
        return nullptr;
    }

    if (tabIndex > 0 && tabIndex < static_cast<int>(symbols->tab().size())) {
        return &symbols->tabAt(tabIndex);
    }

    const int resolvedIndex = symbols->lookupTab(variable.name);
    if (resolvedIndex == 0) {
        return nullptr;
    }

    return &symbols->tabAt(resolvedIndex);
}

} // namespace

void CodeGenerator::generateStatement(const StatementNode& node) {
    switch (node.kind) {
        case ASTNodeKind::Block:
            generateBlock(static_cast<const BlockNode&>(node));
            break;
        case ASTNodeKind::Assign:
            generateAssignment(static_cast<const AssignNode&>(node));
            break;
        case ASTNodeKind::If:
            generateIf(static_cast<const IfNode&>(node));
            break;
        case ASTNodeKind::While:
            generateWhile(static_cast<const WhileNode&>(node));
            break;
        case ASTNodeKind::Repeat:
            generateRepeat(static_cast<const RepeatNode&>(node));
            break;
        case ASTNodeKind::For:
            generateFor(static_cast<const ForNode&>(node));
            break;
        case ASTNodeKind::Case:
            generateCase(static_cast<const CaseNode&>(node));
            break;
        case ASTNodeKind::ProcCall:
            generateProcedureCall(static_cast<const ProcCallNode&>(node));
            break;
        default:
            diagnostic("unsupported statement node for code generation", node.location);
            break;
    }
}

void CodeGenerator::generateAssignment(const AssignNode& node) {
    if (!node.target || !node.value) {
        diagnostic("assignment is missing its target or value", node.location);
        return;
    }

    if (node.target->kind == ASTNodeKind::ArrayAccess) {
        diagnostic("array assignment code generation is not implemented yet", node.target->location);
        return;
    }

    if (node.target->kind == ASTNodeKind::RecordAccess) {
        diagnostic("record assignment code generation is not implemented yet", node.target->location);
        return;
    }

    const size_t diagnosticCount = result.diagnostics.size();
    generateExpression(*node.value);
    if (result.diagnostics.size() != diagnosticCount) {
        return;
    }

    emitStoreAddressable(*node.target);
}

bool CodeGenerator::emitStoreAddressable(const ExpressionNode& node) {
    if (node.kind == ASTNodeKind::ArrayAccess) {
        diagnostic("array access code generation is not implemented yet", node.location);
        return false;
    }

    if (node.kind == ASTNodeKind::RecordAccess) {
        diagnostic("record access code generation is not implemented yet", node.location);
        return false;
    }

    if (node.kind != ASTNodeKind::Var) {
        diagnostic("assignment target is not addressable", node.location);
        return false;
    }

    const auto& variable = static_cast<const VarNode&>(node);
    const TabEntry* entry = resolveVariableEntry(symbolTable, variable, node.tabIndex);
    if (!entry) {
        diagnostic("unknown identifier '" + variable.name + "'", node.location);
        return false;
    }

    if (entry->obj != OBJ_VARIABLE) {
        diagnostic("identifier '" + variable.name + "' is not a variable", node.location);
        return false;
    }

    Instruction instruction;
    instruction.opcode = OpCode::STO;
    instruction.operand = variableAddress(*entry);
    emit(instruction);
    return true;
}

void CodeGenerator::generateIf(const IfNode& node) {
    if (!node.condition || !node.thenBranch) {
        diagnostic("if statement is missing its condition or then-branch", node.location);
        return;
    }

    const size_t diagnosticCount = result.diagnostics.size();
    generateExpression(*node.condition);
    if (result.diagnostics.size() != diagnosticCount) {
        return;
    }

    const int falseJump = emit(makeJump(OpCode::JPC));
    generateStatement(*node.thenBranch);

    if (node.elseBranch) {
        const int endJump = emit(makeJump(OpCode::JMP));
        patchOperand(falseJump, nextAddress());
        generateStatement(*node.elseBranch);
        patchOperand(endJump, nextAddress());
    } else {
        patchOperand(falseJump, nextAddress());
    }
}

void CodeGenerator::generateWhile(const WhileNode& node) {
    if (!node.condition || !node.body) {
        diagnostic("while statement is missing its condition or body", node.location);
        return;
    }

    const int loopStart = nextAddress();
    const size_t diagnosticCount = result.diagnostics.size();
    generateExpression(*node.condition);
    if (result.diagnostics.size() != diagnosticCount) {
        return;
    }

    const int exitJump = emit(makeJump(OpCode::JPC));
    generateBlock(*node.body);
    emit(makeJump(OpCode::JMP, loopStart));
    patchOperand(exitJump, nextAddress());
}

void CodeGenerator::generateRepeat(const RepeatNode& node) {
    if (!node.condition) {
        diagnostic("repeat-until statement is missing its condition", node.location);
        return;
    }

    const int loopStart = nextAddress();
    for (const auto& statement : node.body) {
        if (!statement) {
            continue;
        }

        generateStatement(*statement);
    }

    const size_t diagnosticCount = result.diagnostics.size();
    generateExpression(*node.condition);
    if (result.diagnostics.size() != diagnosticCount) {
        return;
    }

    emit(makeJump(OpCode::JPC, loopStart));
}

void CodeGenerator::generateFor(const ForNode& node) {
    if (!symbolTable) {
        diagnostic("missing symbol table for for-statement code generation", node.location);
        return;
    }

    if (!node.startValue || !node.endValue || !node.body) {
        diagnostic("for statement is missing its bounds or body", node.location);
        return;
    }

    const int index = symbolTable->lookupTab(node.controlVariable);
    if (index == 0) {
        diagnostic("unknown control variable '" + node.controlVariable + "'", node.location);
        return;
    }

    const TabEntry& control = symbolTable->tabAt(index);
    if (control.obj != OBJ_VARIABLE) {
        diagnostic("for control identifier '" + node.controlVariable + "' is not a variable", node.location);
        return;
    }

    size_t diagnosticCount = result.diagnostics.size();
    generateExpression(*node.startValue);
    if (result.diagnostics.size() != diagnosticCount) {
        return;
    }

    Instruction initStore;
    initStore.opcode = OpCode::STO;
    initStore.operand = variableAddress(control);
    emit(initStore);

    const int loopStart = nextAddress();

    Instruction loadControl;
    loadControl.opcode = OpCode::LOD;
    loadControl.operand = variableAddress(control);
    emit(loadControl);

    diagnosticCount = result.diagnostics.size();
    generateExpression(*node.endValue);
    if (result.diagnostics.size() != diagnosticCount) {
        return;
    }

    emit(makeOperation(
        node.direction == ForDirection::Downto ? OperationCode::GEQ : OperationCode::LEQ
    ));

    const int exitJump = emit(makeJump(OpCode::JPC));
    generateBlock(*node.body);

    loadControl.operand = variableAddress(control);
    emit(loadControl);
    emit(makeLiteral(RuntimeValue::integer(1)));
    emit(makeOperation(
        node.direction == ForDirection::Downto ? OperationCode::SUB : OperationCode::ADD
    ));
    emit(initStore);
    emit(makeJump(OpCode::JMP, loopStart));

    patchOperand(exitJump, nextAddress());
}

void CodeGenerator::generateCase(const CaseNode& node) {
    if (!node.selector) {
        diagnostic("case statement is missing its selector", node.location);
        return;
    }

    std::vector<int> endJumps;

    for (const CaseBranchNode& branch : node.branches) {
        if (!branch.statement) {
            diagnostic("case branch is missing its statement", branch.location);
            continue;
        }

        if (branch.labels.empty()) {
            diagnostic("case branch has no labels", branch.location);
            continue;
        }

        for (const auto& label : branch.labels) {
            if (!label) {
                diagnostic("case branch has an empty label", branch.location);
                continue;
            }

            const size_t diagnosticCount = result.diagnostics.size();
            generateExpression(*node.selector);
            generateExpression(*label);
            if (result.diagnostics.size() != diagnosticCount) {
                return;
            }

            emit(makeOperation(OperationCode::EQL));
            const int nextCheck = emit(makeJump(OpCode::JPC));
            generateStatement(*branch.statement);
            endJumps.push_back(emit(makeJump(OpCode::JMP)));
            patchOperand(nextCheck, nextAddress());
        }
    }

    const int endAddress = nextAddress();
    for (int jumpIndex : endJumps) {
        patchOperand(jumpIndex, endAddress);
    }
}

void CodeGenerator::generateProcedureCall(const ProcCallNode& node) {
    const std::string lowered = text_util::lowercase(node.name);

    if (isBuiltinOutput(lowered)) {
        if (node.arguments.empty()) {
            if (lowered == "writeln") {
                emit(makeOperation(OperationCode::WRTLN));
            }
            return;
        }

        for (size_t i = 0; i < node.arguments.size(); ++i) {
            const auto& argument = node.arguments[i];
            if (!argument) {
                diagnostic("procedure '" + node.name + "' has an empty argument", node.location);
                return;
            }

            const size_t diagnosticCount = result.diagnostics.size();
            generateExpression(*argument);
            if (result.diagnostics.size() != diagnosticCount) {
                return;
            }

            const bool isLast = i + 1 == node.arguments.size();
            emit(makeOperation(
                lowered == "writeln" && isLast ? OperationCode::WRTLN : OperationCode::WRT
            ));
        }
        return;
    }

    if (isBuiltinInput(lowered)) {
        diagnostic("input procedures are not implemented yet", node.location);
        return;
    }

    diagnostic("procedure calls for user-defined routines are not implemented yet", node.location);
}
