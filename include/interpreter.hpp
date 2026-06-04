#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "intermediate.hpp"

#include <string>
#include <vector>

struct RuntimeDiagnostic {
    std::string message;
    int instructionPointer = -1;
};

struct ExecutionResult {
    std::vector<std::string> outputLines;
    std::vector<RuntimeDiagnostic> diagnostics;
    bool halted = false;

    bool success() const;
};

class StackMachineInterpreter {
public:
    ExecutionResult execute(const IntermediateProgram& program);

private:
    void reset();
    bool executeInstruction(const IntermediateProgram& program, const Instruction& instruction);

    RuntimeValue pop();
    void push(RuntimeValue value);
    RuntimeValue load(int level, int address);
    void store(int level, int address, RuntimeValue value);
    int resolveBase(int level);
    const RoutineMetadata* findRoutine(const IntermediateProgram& program, int address) const;

    bool executeOperation(OperationCode operation);
    bool executeBinaryNumeric(OperationCode operation);
    bool executeComparison(OperationCode operation);

    void runtimeError(const std::string& message);

    std::vector<RuntimeValue> memory;
    std::vector<RuntimeValue> stack;
    std::vector<int> routineStack;
    std::string currentOutput;
    ExecutionResult result;
    int ip = 0;
    int base = 0;
};

#endif
