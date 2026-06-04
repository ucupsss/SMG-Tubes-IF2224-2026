#include "codegen.hpp"

#include <algorithm>
#include <exception>
#include <string>

bool CodeGenerationResult::success() const {
    return diagnostics.empty();
}

CodeGenerationResult CodeGenerator::generate(const ProgramNode& root, const SymbolTable& symbols) {
    reset(symbols);

    try {
        generateProgram(root);
    } catch (const std::exception& error) {
        diagnostic("internal code generation error: " + std::string(error.what()), root.location);
    } catch (...) {
        diagnostic("internal code generation error: unknown exception", root.location);
    }

    return result;
}

void CodeGenerator::reset(const SymbolTable& symbols) {
    symbolTable = &symbols;
    result = {};
    routineAddressesByTabIndex.assign(symbols.tab().size(), -1);
    currentBlockIndex = 0;
    currentLexLevel = 0;
    currentFunctionTabIndex = 0;
    currentFunctionReturnOffset = 0;
}

void CodeGenerator::generateProgram(const ProgramNode& node) {
    if (!symbolTable) {
        diagnostic("missing symbol table for code generation", node.location);
        return;
    }

    int variableCount = 0;
    if (node.blockIndex >= 0 && node.blockIndex < static_cast<int>(symbolTable->btab().size())) {
        variableCount = symbolTable->btabAt(node.blockIndex).vsze;
    } else {
        diagnostic("program block index is out of range", node.location);
    }

    if (variableCount < 0) {
        diagnostic("program variable size is negative", node.location);
        variableCount = 0;
    }

    int mainJump = -1;
    bool hasRoutineDeclarations = false;
    for (const auto& declaration : node.declarations) {
        if (!declaration) {
            continue;
        }

        if (declaration->kind == ASTNodeKind::ProcDecl ||
            declaration->kind == ASTNodeKind::FuncDecl) {
            hasRoutineDeclarations = true;
            break;
        }
    }

    if (hasRoutineDeclarations) {
        Instruction jump;
        jump.opcode = OpCode::JMP;
        mainJump = emit(jump);
    }

    for (const auto& declaration : node.declarations) {
        if (!declaration) {
            continue;
        }

        generateDeclaration(*declaration);
    }

    if (mainJump >= 0) {
        patchOperand(mainJump, nextAddress());
    }

    currentBlockIndex = node.blockIndex;
    currentLexLevel = 0;
    currentFunctionTabIndex = 0;
    currentFunctionReturnOffset = 0;

    Instruction init;
    init.opcode = OpCode::INT;
    init.operand = frameHeaderSize + variableCount;
    emit(init);

    if (node.body) {
        generateBlock(*node.body);
    } else {
        diagnostic("program body is missing", node.location);
    }

    Instruction ret;
    ret.opcode = OpCode::RET;
    emit(ret);
}

void CodeGenerator::generateBlock(const BlockNode& node) {
    for (const auto& statement : node.statements) {
        if (!statement) {
            continue;
        }

        generateStatement(*statement);
    }
}

int CodeGenerator::emit(Instruction instruction) {
    result.program.instructions.push_back(std::move(instruction));
    return static_cast<int>(result.program.instructions.size()) - 1;
}

void CodeGenerator::patchOperand(int instructionIndex, int operand) {
    if (instructionIndex < 0 ||
        instructionIndex >= static_cast<int>(result.program.instructions.size())) {
        diagnostic("cannot patch invalid instruction address " + std::to_string(instructionIndex));
        return;
    }

    result.program.instructions[static_cast<size_t>(instructionIndex)].operand = operand;
}

int CodeGenerator::nextAddress() const {
    return static_cast<int>(result.program.instructions.size());
}

int CodeGenerator::lexicalLevelOffset(const TabEntry& entry) const {
    if (entry.lev > currentLexLevel) {
        return 0;
    }

    return currentLexLevel - entry.lev;
}

int CodeGenerator::blockIndexForEntry(const TabEntry& entry) const {
    if (!symbolTable) {
        return -1;
    }

    const auto& blocks = symbolTable->btab();
    for (int blockIndex = 0; blockIndex < static_cast<int>(blocks.size()); ++blockIndex) {
        int entryIndex = blocks[static_cast<size_t>(blockIndex)].last;
        while (entryIndex != 0) {
            const TabEntry& candidate = symbolTable->tabAt(entryIndex);
            if (&candidate == &entry) {
                return blockIndex;
            }
            entryIndex = candidate.link;
        }
    }

    return -1;
}

bool CodeGenerator::isParameterEntry(const TabEntry& entry, int blockIndex) const {
    if (!symbolTable || blockIndex < 0 || blockIndex >= static_cast<int>(symbolTable->btab().size())) {
        return false;
    }

    int parameterIndex = symbolTable->btabAt(blockIndex).lpar;
    while (parameterIndex != 0) {
        const TabEntry& parameter = symbolTable->tabAt(parameterIndex);
        if (&parameter == &entry) {
            return true;
        }
        parameterIndex = parameter.link;
    }

    return false;
}

bool CodeGenerator::isCurrentFunctionResult(const TabEntry& entry) const {
    return entry.obj == OBJ_FUNCTION &&
           currentFunctionTabIndex > 0 &&
           entry.ref == currentBlockIndex &&
           entry.identifier == symbolTable->tabAt(currentFunctionTabIndex).identifier;
}

int CodeGenerator::variableAddress(const TabEntry& entry) {
    if (!symbolTable) {
        diagnostic("missing symbol table for variable address calculation");
        return 0;
    }

    if (isCurrentFunctionResult(entry)) {
        return currentFunctionReturnOffset;
    }

    if (entry.obj != OBJ_VARIABLE) {
        diagnostic("identifier '" + entry.identifier + "' is not stored as a variable");
        return 0;
    }

    if (entry.adr < 0) {
        diagnostic("variable '" + entry.identifier + "' has an invalid address");
        return 0;
    }

    const int blockIndex = blockIndexForEntry(entry);
    if (blockIndex < 0) {
        diagnostic("cannot resolve declaring block for '" + entry.identifier + "'");
        return 0;
    }

    if (isParameterEntry(entry, blockIndex)) {
        return frameHeaderSize + entry.adr;
    }

    const BTabEntry& block = symbolTable->btabAt(blockIndex);
    return frameHeaderSize + block.psze + entry.adr;
}

void CodeGenerator::registerRoutine(
    int tabIndex,
    int address,
    const BTabEntry& block,
    bool returnsValue
) {
    if (tabIndex <= 0 || tabIndex >= static_cast<int>(routineAddressesByTabIndex.size())) {
        diagnostic("cannot register routine with invalid tab index " + std::to_string(tabIndex));
        return;
    }

    const int returnValueOffset = frameHeaderSize + block.psze + block.vsze;
    RoutineMetadata metadata;
    metadata.address = address;
    metadata.parameterCount = block.psze;
    metadata.frameSize = frameHeaderSize + block.psze + block.vsze + (returnsValue ? 1 : 0);
    metadata.returnsValue = returnsValue;
    metadata.returnValueOffset = returnsValue ? returnValueOffset : 0;

    for (int parameterIndex : routineParameterIndices(symbolTable->tabAt(tabIndex).ref)) {
        metadata.byReferenceParameters.push_back(symbolTable->tabAt(parameterIndex).nrm == 0);
    }

    routineAddressesByTabIndex[static_cast<size_t>(tabIndex)] = address;
    result.program.routines.push_back(metadata);
}

int CodeGenerator::routineAddress(int tabIndex) const {
    if (tabIndex <= 0 || tabIndex >= static_cast<int>(routineAddressesByTabIndex.size())) {
        return -1;
    }

    return routineAddressesByTabIndex[static_cast<size_t>(tabIndex)];
}

TypeInfo CodeGenerator::arrayElementType(const ATabEntry& entry) const {
    TypeInfo type;
    type.code = entry.etyp;
    type.baseType = entry.etyp;
    type.ref = entry.eref;
    type.name = symbolTable ? symbolTable->typeName(type) : "";

    return type;
}

bool CodeGenerator::isStructuredType(const TypeInfo& type) const {
    return type.code == TYPE_ARRAY || type.code == TYPE_RECORD;
}

TypeInfo CodeGenerator::expressionTypeInfo(const ExpressionNode& node) const {
    if (!symbolTable) {
        return {};
    }

    switch (node.kind) {
        case ASTNodeKind::Var: {
            if (node.tabIndex > 0 && node.tabIndex < static_cast<int>(symbolTable->tab().size())) {
                return symbolTable->tabAt(node.tabIndex).typeInfo;
            }

            const auto& variable = static_cast<const VarNode&>(node);
            const int index = symbolTable->lookupTab(variable.name);
            if (index > 0) {
                return symbolTable->tabAt(index).typeInfo;
            }
            break;
        }
        case ASTNodeKind::ArrayAccess: {
            const auto& access = static_cast<const ArrayAccessNode&>(node);
            if (!access.array) {
                break;
            }

            TypeInfo currentType = expressionTypeInfo(*access.array);
            for (size_t i = 0; i < access.indices.size(); ++i) {
                if (currentType.code != TYPE_ARRAY ||
                    currentType.ref <= 0 ||
                    currentType.ref >= static_cast<int>(symbolTable->atab().size())) {
                    return {};
                }

                currentType = arrayElementType(symbolTable->atabAt(currentType.ref));
            }

            return currentType;
        }
        case ASTNodeKind::RecordAccess: {
            const auto& access = static_cast<const RecordAccessNode&>(node);
            if (!access.record) {
                break;
            }

            TypeInfo recordType = expressionTypeInfo(*access.record);
            if (recordType.code != TYPE_RECORD || recordType.ref <= 0) {
                break;
            }

            const int fieldIndex = symbolTable->lookupTab(access.fieldName, recordType.ref);
            if (fieldIndex > 0) {
                return symbolTable->tabAt(fieldIndex).typeInfo;
            }
            break;
        }
        case ASTNodeKind::IntLiteral:
            return symbolTable->tabAt(symbolTable->lookupTab("Integer")).typeInfo;
        case ASTNodeKind::RealLiteral:
            return symbolTable->tabAt(symbolTable->lookupTab("Real")).typeInfo;
        case ASTNodeKind::CharLiteral:
            return symbolTable->tabAt(symbolTable->lookupTab("Char")).typeInfo;
        case ASTNodeKind::StringLiteral:
            return symbolTable->tabAt(symbolTable->lookupTab("String")).typeInfo;
        case ASTNodeKind::BoolLiteral:
            return symbolTable->tabAt(symbolTable->lookupTab("Boolean")).typeInfo;
        default:
            break;
    }

    TypeInfo fallback;
    fallback.code = node.inferredType;
    fallback.baseType = node.inferredType;
    fallback.name = symbolTable->typeName(fallback);
    return fallback;
}

std::vector<int> CodeGenerator::routineParameterIndices(int blockIndex) const {
    std::vector<int> indices;
    if (!symbolTable || blockIndex <= 0 || blockIndex >= static_cast<int>(symbolTable->btab().size())) {
        return indices;
    }

    int parameterIndex = symbolTable->btabAt(blockIndex).lpar;
    while (parameterIndex != 0) {
        indices.push_back(parameterIndex);
        const TabEntry& parameter = symbolTable->tabAt(parameterIndex);
        parameterIndex = parameter.link;
    }

    std::reverse(indices.begin(), indices.end());
    return indices;
}

void CodeGenerator::diagnostic(const std::string& message, const SourceLocation& location) {
    result.diagnostics.push_back({message, location.line, location.column});
}
