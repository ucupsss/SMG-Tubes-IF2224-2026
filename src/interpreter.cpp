#include "interpreter.hpp"

#include "symbol_table.hpp"

#include <cctype>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool isValidInstructionAddress(const IntermediateProgram& program, int address) {
    return address >= 0 && address < static_cast<int>(program.instructions.size());
}

bool ordinalValue(const RuntimeValue& value, int& ordinal) {
    switch (value.kind) {
        case RuntimeValueKind::Integer:
            ordinal = value.integerValue;
            return true;
        case RuntimeValueKind::Boolean:
            ordinal = value.booleanValue ? 1 : 0;
            return true;
        case RuntimeValueKind::Char:
            ordinal = static_cast<unsigned char>(value.charValue);
            return true;
        default:
            return false;
    }
}

std::string lowercase(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

}

bool ExecutionResult::success() const {
    return diagnostics.empty();
}



void StackMachineInterpreter::reset() {
    memory.clear();
    stack.clear();
    routineStack.clear();
    inputTokens.clear();
    currentOutput.clear();
    result = {};
    nextInputToken = 0;
    ip = 0;
    base = 0;
}

void StackMachineInterpreter::runtimeError(const std::string& message) {
    result.diagnostics.push_back({
        "Runtime error at instruction " + std::to_string(ip - 1) + ": " + message,
        ip - 1
    });
    result.halted = true;
}

void StackMachineInterpreter::push(RuntimeValue value) {
    if (!canPushOperand(1, "push")) {
        return;
    }

    stack.push_back(std::move(value));
}

bool StackMachineInterpreter::ensureMemorySize(size_t requiredSize, const std::string& context) {
    if (requiredSize > maxMemorySlots) {
        runtimeError(context + ": stack overflow: kebutuhan memori " +
                     std::to_string(requiredSize) +
                     " slot melebihi batas " +
                     std::to_string(maxMemorySlots));
        return false;
    }

    if (memory.size() >= requiredSize) {
        return true;
    }

    try {
        memory.resize(requiredSize);
    } catch (const std::exception& error) {
        runtimeError(context + ": gagal mengalokasikan memori: " + error.what());
        return false;
    }

    return true;
}

bool StackMachineInterpreter::canPushOperand(size_t count, const std::string& context) {
    if (count > maxOperandStackSlots || stack.size() > maxOperandStackSlots - count) {
        runtimeError(context + ": stack overflow: operand stack melebihi batas " +
                     std::to_string(maxOperandStackSlots));
        return false;
    }

    return true;
}

RuntimeValue StackMachineInterpreter::pop() {
    if (stack.empty()) {
        runtimeError("stack underflow: pop dari stack kosong");
        return RuntimeValue::voidValue();
    }
    RuntimeValue value = std::move(stack.back());
    stack.pop_back();
    return value;
}

int StackMachineInterpreter::resolveBase(int level) {
    if (level < 0) {
        runtimeError("negative lexical level offset: " + std::to_string(level));
        return 0;
    }

    int resolvedBase = base;
    for (int i = 0; i < level; ++i) {
        if (resolvedBase < 0 || resolvedBase >= static_cast<int>(memory.size())) {
            runtimeError("invalid static link base " + std::to_string(resolvedBase));
            return 0;
        }

        RuntimeValue link = memory[static_cast<size_t>(resolvedBase)];
        if (link.kind != RuntimeValueKind::Integer) {
            runtimeError("static link is not an integer address");
            return 0;
        }

        resolvedBase = link.integerValue;
    }

    return resolvedBase;
}

int StackMachineInterpreter::resolveAddress(int level, int address, bool indirect) {
    const int resolvedBase = resolveBase(level);
    if (result.halted) return 0;

    const int absoluteAddress = resolvedBase + address;
    if (absoluteAddress < 0 || absoluteAddress >= static_cast<int>(memory.size())) {
        runtimeError("out-of-bounds access di address " + std::to_string(absoluteAddress) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return 0;
    }

    if (!indirect) {
        return absoluteAddress;
    }

    const RuntimeValue& pointer = memory[static_cast<size_t>(absoluteAddress)];
    if (pointer.kind != RuntimeValueKind::Integer) {
        runtimeError("indirect address cell does not contain an integer address");
        return 0;
    }

    if (pointer.integerValue < 0 || pointer.integerValue >= static_cast<int>(memory.size())) {
        runtimeError("out-of-bounds indirect access di address " +
                     std::to_string(pointer.integerValue) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return 0;
    }

    return pointer.integerValue;
}

RuntimeValue StackMachineInterpreter::load(int level, int address, bool indirect) {
    const int absoluteAddress = resolveAddress(level, address, indirect);
    if (result.halted) {
        return RuntimeValue::voidValue();
    }
    return memory[static_cast<size_t>(absoluteAddress)];
}

void StackMachineInterpreter::store(int level, int address, RuntimeValue value, bool indirect) {
    const int absoluteAddress = resolveAddress(level, address, indirect);
    if (result.halted) return;

    memory[static_cast<size_t>(absoluteAddress)] = std::move(value);
}

int StackMachineInterpreter::popAddress(const std::string& instructionName) {
    RuntimeValue address = pop();
    if (result.halted) return 0;

    if (address.kind != RuntimeValueKind::Integer) {
        runtimeError(instructionName + ": address on stack is not an integer");
        return 0;
    }

    if (address.integerValue < 0 || address.integerValue >= static_cast<int>(memory.size())) {
        runtimeError(instructionName + ": out-of-bounds address " +
                     std::to_string(address.integerValue) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return 0;
    }

    return address.integerValue;
}

bool StackMachineInterpreter::readInputToken(std::string& token) {
    while (nextInputToken >= inputTokens.size()) {
        inputTokens.clear();
        nextInputToken = 0;

        std::string line;
        if (!std::getline(std::cin, line)) {
            runtimeError("input exhausted while reading value");
            return false;
        }

        std::istringstream tokenizer(line);
        std::string value;
        while (tokenizer >> value) {
            inputTokens.push_back(value);
        }
    }

    token = inputTokens[nextInputToken++];
    return true;
}

void StackMachineInterpreter::discardInputLine(bool consumeWhenNoBuffered) {
    const bool hasBufferedTokens = nextInputToken < inputTokens.size();
    inputTokens.clear();
    nextInputToken = 0;

    if (!hasBufferedTokens && consumeWhenNoBuffered) {
        std::string ignored;
        if (!std::getline(std::cin, ignored)) {
            runtimeError("input exhausted while reading line");
        }
    }
}

RuntimeValue StackMachineInterpreter::parseInputValue(const std::string& token, int typeCode) {
    try {
        switch (typeCode) {
            case TYPE_INTEGER:
            case TYPE_SUBRANGE:
            case TYPE_ENUM: {
                size_t used = 0;
                const int value = std::stoi(token, &used);
                if (used != token.size()) {
                    runtimeError("invalid integer input '" + token + "'");
                    return RuntimeValue::voidValue();
                }
                return RuntimeValue::integer(value);
            }
            case TYPE_REAL: {
                size_t used = 0;
                const double value = std::stod(token, &used);
                if (used != token.size()) {
                    runtimeError("invalid real input '" + token + "'");
                    return RuntimeValue::voidValue();
                }
                return RuntimeValue::real(value);
            }
            case TYPE_BOOLEAN: {
                const std::string normalized = lowercase(token);
                if (normalized == "true" || normalized == "1") {
                    return RuntimeValue::boolean(true);
                }
                if (normalized == "false" || normalized == "0") {
                    return RuntimeValue::boolean(false);
                }
                runtimeError("invalid boolean input '" + token + "'");
                return RuntimeValue::voidValue();
            }
            case TYPE_CHAR:
                if (token.size() == 1) {
                    return RuntimeValue::character(token[0]);
                }
                if (token.size() == 3 && token.front() == '\'' && token.back() == '\'') {
                    return RuntimeValue::character(token[1]);
                }
                runtimeError("invalid char input '" + token + "'");
                return RuntimeValue::voidValue();
            case TYPE_STRING:
                return RuntimeValue::string(token);
            default:
                runtimeError("unsupported input target type code " + std::to_string(typeCode));
                return RuntimeValue::voidValue();
        }
    } catch (const std::exception&) {
        runtimeError("invalid input token '" + token + "'");
        return RuntimeValue::voidValue();
    }
}

const RoutineMetadata* StackMachineInterpreter::findRoutine(
    const IntermediateProgram& program,
    int address
) const {
    for (const RoutineMetadata& routine : program.routines) {
        if (routine.address == address) {
            return &routine;
        }
    }

    return nullptr;
}

bool StackMachineInterpreter::executeInstruction(
    const IntermediateProgram& program,
    const Instruction& instruction) {
    switch (instruction.opcode) {
        // INT m
        case OpCode::INT: {
            const int size = instruction.operand;
            if (size < 0) {
                runtimeError("INT: ukuran memori tidak valid: " + std::to_string(size));
                return false;
            }

            if (base < 0) {
                runtimeError("INT: base pointer negatif");
                return false;
            }

            const size_t requiredSize = static_cast<size_t>(base) + static_cast<size_t>(size);
            if (requiredSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
                runtimeError("INT: ukuran memori terlalu besar");
                return false;
            }

            if (!ensureMemorySize(requiredSize, "INT")) {
                return false;
            }
            break;
        }
        // LIT v
        case OpCode::LIT: {
            if (instruction.hasLiteral) {
                push(instruction.literal);
            } else {
                // Fallback: jika tidak ada literal, perlakukan operand sebagai integer.
                push(RuntimeValue::integer(instruction.operand));
            }
            break;
        }
        // LOD a
        case OpCode::LOD: {
            RuntimeValue value = load(instruction.level, instruction.operand, instruction.indirect);
            if (result.halted) return false;
            push(std::move(value));
            break;
        }
        // LDA a
        case OpCode::LDA: {
            const int absoluteAddress = resolveAddress(instruction.level, instruction.operand, false);
            if (result.halted) return false;
            push(RuntimeValue::integer(absoluteAddress));
            break;
        }
        // LDI
        case OpCode::LDI: {
            const int absoluteAddress = popAddress("LDI");
            if (result.halted) return false;
            push(memory[static_cast<size_t>(absoluteAddress)]);
            break;
        }
        // LDR n
        case OpCode::LDR: {
            const int size = instruction.operand;
            if (size < 0) {
                runtimeError("LDR: ukuran load tidak valid: " + std::to_string(size));
                return false;
            }

            if (!canPushOperand(static_cast<size_t>(size), "LDR")) {
                return false;
            }

            const int sourceAddress = popAddress("LDR");
            if (result.halted) return false;

            if (sourceAddress + size > static_cast<int>(memory.size())) {
                runtimeError("LDR: out-of-bounds aggregate load");
                return false;
            }

            for (int offset = 0; offset < size; ++offset) {
                push(memory[static_cast<size_t>(sourceAddress + offset)]);
            }
            break;
        }
        // BND low high
        case OpCode::BND: {
            if (stack.empty()) {
                runtimeError("BND: stack underflow: indeks array tidak tersedia");
                return false;
            }

            int indexValue = 0;
            if (!ordinalValue(stack.back(), indexValue)) {
                runtimeError("BND: indeks array harus bertipe ordinal");
                return false;
            }

            const int low = instruction.level;
            const int high = instruction.operand;
            if (indexValue < low || indexValue > high) {
                runtimeError("BND: array index out of bounds " +
                             std::to_string(indexValue) +
                             " (rentang valid " +
                             std::to_string(low) +
                             ".." +
                             std::to_string(high) +
                             ")");
                return false;
            }
            break;
        }
        // STO a
        case OpCode::STO: {
            RuntimeValue value = pop();
            if (result.halted) return false;
            store(instruction.level, instruction.operand, std::move(value), instruction.indirect);
            break;
        }
        // STI
        case OpCode::STI: {
            const int absoluteAddress = popAddress("STI");
            if (result.halted) return false;
            RuntimeValue value = pop();
            if (result.halted) return false;
            memory[static_cast<size_t>(absoluteAddress)] = std::move(value);
            break;
        }
        // CPY n
        case OpCode::CPY: {
            const int size = instruction.operand;
            if (size < 0) {
                runtimeError("CPY: ukuran copy tidak valid: " + std::to_string(size));
                return false;
            }

            const int destinationAddress = popAddress("CPY");
            if (result.halted) return false;
            const int sourceAddress = popAddress("CPY");
            if (result.halted) return false;

            if (sourceAddress + size > static_cast<int>(memory.size()) ||
                destinationAddress + size > static_cast<int>(memory.size())) {
                runtimeError("CPY: out-of-bounds aggregate copy");
                return false;
            }

            std::vector<RuntimeValue> copiedValues;
            copiedValues.reserve(static_cast<size_t>(size));
            for (int offset = 0; offset < size; ++offset) {
                copiedValues.push_back(memory[static_cast<size_t>(sourceAddress + offset)]);
            }

            for (int offset = 0; offset < size; ++offset) {
                memory[static_cast<size_t>(destinationAddress + offset)] =
                    std::move(copiedValues[static_cast<size_t>(offset)]);
            }
            break;
        }
        // INP type
        case OpCode::INP: {
            const int absoluteAddress = popAddress("INP");
            if (result.halted) return false;

            std::string token;
            if (!readInputToken(token)) {
                return false;
            }

            RuntimeValue value = parseInputValue(token, instruction.operand);
            if (result.halted) return false;
            memory[static_cast<size_t>(absoluteAddress)] = std::move(value);
            break;
        }
        // INL
        case OpCode::INL: {
            discardInputLine(instruction.operand != 0);
            if (result.halted) return false;
            break;
        }
        // JMP l
        case OpCode::JMP: {
            if (!isValidInstructionAddress(program, instruction.operand)) {
                runtimeError("JMP: invalid jump target " + std::to_string(instruction.operand));
                return false;
            }
            ip = instruction.operand;
            break;
        }
        case OpCode::JPC: {
            RuntimeValue condition = pop();
            if (result.halted) return false;
            if (!condition.isTruthy()) {
                if (!isValidInstructionAddress(program, instruction.operand)) {
                    runtimeError("JPC: invalid jump target " + std::to_string(instruction.operand));
                    return false;
                }
                ip = instruction.operand;
            }
            break;
        }
        case OpCode::OPR: {
            const auto operation = static_cast<OperationCode>(instruction.operand);
            executeOperation(operation);
            break;
        }
        case OpCode::RET: {
            if (base == 0) {
                if (!currentOutput.empty()) {
                    result.outputLines.push_back(currentOutput);
                    currentOutput.clear();
                }
                result.halted = true;
                break;
            }

            if (routineStack.empty()) {
                runtimeError("RET: routine stack is empty");
                return false;
            }

            const int routineAddress = routineStack.back();
            routineStack.pop_back();

            const RoutineMetadata* routine = findRoutine(program, routineAddress);
            if (!routine) {
                runtimeError("RET: missing metadata for routine at " + std::to_string(routineAddress));
                return false;
            }

            RuntimeValue returnValue = RuntimeValue::voidValue();
            if (routine->returnsValue) {
                const int returnAddress = base + routine->returnValueOffset;
                if (returnAddress < 0 || returnAddress >= static_cast<int>(memory.size())) {
                    runtimeError("RET: function return slot is out of bounds");
                    return false;
                }
                returnValue = memory[static_cast<size_t>(returnAddress)];
            }

            const int dynamicLinkAddress = base + 1;
            const int returnAddressSlot = base + 2;
            if (returnAddressSlot >= static_cast<int>(memory.size()) ||
                dynamicLinkAddress >= static_cast<int>(memory.size())) {
                runtimeError("RET: frame header is out of bounds");
                return false;
            }

            RuntimeValue dynamicLink = memory[static_cast<size_t>(dynamicLinkAddress)];
            RuntimeValue returnAddressValue = memory[static_cast<size_t>(returnAddressSlot)];
            if (dynamicLink.kind != RuntimeValueKind::Integer ||
                returnAddressValue.kind != RuntimeValueKind::Integer) {
                runtimeError("RET: frame header contains invalid link values");
                return false;
            }

            const int oldBase = base;
            if (dynamicLink.integerValue < 0 || dynamicLink.integerValue > oldBase) {
                runtimeError("RET: dynamic link points outside the active stack frame");
                return false;
            }

            if (!isValidInstructionAddress(program, returnAddressValue.integerValue)) {
                runtimeError("RET: return address is not a valid instruction");
                return false;
            }

            base = dynamicLink.integerValue;
            ip = returnAddressValue.integerValue;
            memory.resize(static_cast<size_t>(oldBase));

            if (routine->returnsValue) {
                push(std::move(returnValue));
            }
            break;
        }
        case OpCode::CAL: {
            if (!isValidInstructionAddress(program, instruction.operand)) {
                runtimeError("CAL: invalid routine target " + std::to_string(instruction.operand));
                return false;
            }

            const RoutineMetadata* routine = findRoutine(program, instruction.operand);
            if (!routine) {
                runtimeError("CAL: missing metadata for routine target " + std::to_string(instruction.operand));
                return false;
            }

            if (routine->parameterCount < 0) {
                runtimeError("CAL: invalid routine parameter count " +
                             std::to_string(routine->parameterCount));
                return false;
            }

            if (static_cast<int>(stack.size()) < routine->parameterCount) {
                runtimeError("CAL: not enough argument values on stack");
                return false;
            }

            if (routineStack.size() >= maxCallDepth) {
                runtimeError("CAL: stack overflow: call depth melebihi batas " +
                             std::to_string(maxCallDepth));
                return false;
            }

            if (routine->frameSize < 3) {
                runtimeError("CAL: invalid routine frame size " +
                             std::to_string(routine->frameSize));
                return false;
            }

            if (routine->frameSize < 3 + routine->parameterCount) {
                runtimeError("CAL: routine frame is too small for its parameters");
                return false;
            }

            const int staticBase = resolveBase(instruction.level);
            if (result.halted) return false;

            const int frameBase = static_cast<int>(memory.size());
            if (routine->frameSize > std::numeric_limits<int>::max() - frameBase) {
                runtimeError("CAL: ukuran frame terlalu besar");
                return false;
            }

            const size_t requiredSize =
                static_cast<size_t>(frameBase) + static_cast<size_t>(routine->frameSize);
            if (!ensureMemorySize(requiredSize, "CAL")) {
                return false;
            }

            memory[static_cast<size_t>(frameBase)] = RuntimeValue::integer(staticBase);
            memory[static_cast<size_t>(frameBase + 1)] = RuntimeValue::integer(base);
            memory[static_cast<size_t>(frameBase + 2)] = RuntimeValue::integer(ip);

            for (int i = routine->parameterCount - 1; i >= 0; --i) {
                RuntimeValue argument = pop();
                if (result.halted) return false;
                memory[static_cast<size_t>(frameBase + 3 + i)] = std::move(argument);
            }

            routineStack.push_back(instruction.operand);
            base = frameBase;
            ip = instruction.operand;
            break;
        }
    }
    return !result.halted;
}

ExecutionResult StackMachineInterpreter::execute(const IntermediateProgram& program) {
    reset();
    const int programSize = static_cast<int>(program.instructions.size());
    if (programSize == 0) {
        result.diagnostics.push_back({"program kosong: tidak ada instruksi", -1});
        result.halted = true;
        return result;
    }
    while (ip >= 0 && ip < programSize && !result.halted) {
        // Fetch
        const Instruction& instruction = program.instructions[static_cast<size_t>(ip)];
        ++ip;
        // Decode + Execute
        executeInstruction(program, instruction);
    }
    if (!result.halted) {
        result.diagnostics.push_back({
            "Runtime error: instruction pointer left program without RET at " + std::to_string(ip),
            ip
        });
        result.halted = true;
    }
    return result;
}
