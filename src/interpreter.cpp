#include "interpreter.hpp"
#include <string>
#include <vector>

bool ExecutionResult::success() const {
    return diagnostics.empty();
}



void StackMachineInterpreter::reset() {
    memory.clear();
    stack.clear();
    currentOutput.clear();
    result = {};
    ip = 0;
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

RuntimeValue StackMachineInterpreter::load(int address) {
    if (address < 0 || address >= static_cast<int>(memory.size())) {
        runtimeError("out-of-bounds read di address " + std::to_string(address) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return RuntimeValue::voidValue();
    }
    return memory[static_cast<size_t>(address)];
}

void StackMachineInterpreter::store(int address, RuntimeValue value) {
    if (address < 0 || address >= static_cast<int>(memory.size())) {
        runtimeError("out-of-bounds write di address " + std::to_string(address) +
                     " (ukuran memori: " + std::to_string(memory.size()) + ")");
        return;
    }
    memory[static_cast<size_t>(address)] = std::move(value);
}

bool StackMachineInterpreter::executeInstruction(
    const IntermediateProgram& /*program*/,
    const Instruction& instruction) {
    switch (instruction.opcode) {
        // INT m
        case OpCode::INT: {
            const int size = instruction.operand;
            if (size < 0) {
                runtimeError("INT: ukuran memori tidak valid: " + std::to_string(size));
                return false;
            }
            if (static_cast<int>(memory.size()) < size) {
                memory.resize(static_cast<size_t>(size));
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
            RuntimeValue value = load(instruction.operand);
            if (result.halted) return false;
            push(std::move(value));
            break;
        }
        // STO a
        case OpCode::STO: {
            RuntimeValue value = pop();
            if (result.halted) return false;
            store(instruction.operand, std::move(value));
            break;
        }
        // JMP l
        case OpCode::JMP: {
            ip = instruction.operand;
            break;
        }
        case OpCode::JPC: {
            RuntimeValue condition = pop();
            if (result.halted) return false;
            if (!condition.isTruthy()) {
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
            if (!currentOutput.empty()) {
                result.outputLines.push_back(currentOutput);
                currentOutput.clear();
            }
            result.halted = true;
            break;
        }
        case OpCode::CAL: {
            runtimeError("CAL: pemanggilan prosedur/fungsi belum diimplementasikan");
            return false;
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
    return result;
}
