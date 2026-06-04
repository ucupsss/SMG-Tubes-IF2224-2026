#include "interpreter.hpp"
#include <string>
#include <vector>

namespace {

bool isValidInstructionAddress(const IntermediateProgram& program, int address) {
    return address >= 0 && address < static_cast<int>(program.instructions.size());
}

}

bool ExecutionResult::success() const {
    return diagnostics.empty();
}



void StackMachineInterpreter::reset() {
    memory.clear();
    stack.clear();
    routineStack.clear();
    currentOutput.clear();
    result = {};
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
    stack.push_back(std::move(value));
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

RuntimeValue StackMachineInterpreter::load(int level, int address) {
    const int resolvedBase = resolveBase(level);
    if (result.halted) return RuntimeValue::voidValue();

    const int absoluteAddress = resolvedBase + address;
    if (absoluteAddress < 0 || absoluteAddress >= static_cast<int>(memory.size())) {
        runtimeError("out-of-bounds read di address " + std::to_string(absoluteAddress) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return RuntimeValue::voidValue();
    }
    return memory[static_cast<size_t>(absoluteAddress)];
}

void StackMachineInterpreter::store(int level, int address, RuntimeValue value) {
    const int resolvedBase = resolveBase(level);
    if (result.halted) return;

    const int absoluteAddress = resolvedBase + address;
    if (absoluteAddress < 0 || absoluteAddress >= static_cast<int>(memory.size())) {
        runtimeError("out-of-bounds write di address " + std::to_string(absoluteAddress) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return;
    }
    memory[static_cast<size_t>(absoluteAddress)] = std::move(value);
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
            const int requiredSize = base + size;
            if (static_cast<int>(memory.size()) < requiredSize) {
                memory.resize(static_cast<size_t>(requiredSize));
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
            RuntimeValue value = load(instruction.level, instruction.operand);
            if (result.halted) return false;
            push(std::move(value));
            break;
        }
        // STO a
        case OpCode::STO: {
            RuntimeValue value = pop();
            if (result.halted) return false;
            store(instruction.level, instruction.operand, std::move(value));
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

            if (static_cast<int>(stack.size()) < routine->parameterCount) {
                runtimeError("CAL: not enough argument values on stack");
                return false;
            }

            const int staticBase = resolveBase(instruction.level);
            if (result.halted) return false;

            const int frameBase = static_cast<int>(memory.size());
            memory.resize(static_cast<size_t>(frameBase + routine->frameSize));
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
