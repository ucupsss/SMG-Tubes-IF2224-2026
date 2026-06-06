#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "intermediate.hpp"

#include <cstddef>
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
    static constexpr size_t maxCallDepth = 1000;
    static constexpr size_t maxMemorySlots = 1000000;
    static constexpr size_t maxOperandStackSlots = 1000000;

    void reset();
    bool executeInstruction(const IntermediateProgram& program, const Instruction& instruction);

    RuntimeValue pop();
    void push(RuntimeValue value);
    bool ensureMemorySize(size_t requiredSize, const std::string& context);
    bool canPushOperand(size_t count, const std::string& context);
    RuntimeValue load(int level, int address, bool indirect);
    void store(int level, int address, RuntimeValue value, bool indirect);
    int resolveBase(int level);
    int resolveAddress(int level, int address, bool indirect);
    int popAddress(const std::string& instructionName);
    const RoutineMetadata* findRoutine(const IntermediateProgram& program, int address) const;
    bool readInputToken(std::string& token);
    void discardInputLine(bool consumeWhenNoBuffered);
    RuntimeValue parseInputValue(const std::string& token, int typeCode);

    bool executeOperation(OperationCode operation);
    bool executeBinaryNumeric(OperationCode operation);
    bool executeComparison(OperationCode operation);

    void runtimeError(const std::string& message);

    std::vector<RuntimeValue> memory;
    std::vector<RuntimeValue> stack;
    std::vector<int> routineStack;
    std::vector<std::string> inputTokens;
    std::string currentOutput;
    ExecutionResult result;
    size_t nextInputToken = 0;
    int ip = 0;
    int base = 0;
};

#endif
