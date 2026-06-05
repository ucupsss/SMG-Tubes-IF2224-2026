#ifndef INTERMEDIATE_HPP
#define INTERMEDIATE_HPP

#include <string>
#include <vector>

enum class RuntimeValueKind {
    Void,
    Integer,
    Real,
    Boolean,
    Char,
    String
};

struct RuntimeValue {
    RuntimeValueKind kind = RuntimeValueKind::Void;
    int integerValue = 0;
    double realValue = 0.0;
    bool booleanValue = false;
    char charValue = '\0';
    std::string stringValue;

    static RuntimeValue voidValue();
    static RuntimeValue integer(int value);
    static RuntimeValue real(double value);
    static RuntimeValue boolean(bool value);
    static RuntimeValue character(char value);
    static RuntimeValue string(std::string value);

    bool isTruthy() const;
    std::string display() const;
};

enum class OpCode {
    LIT,
    LOD,
    LDA,
    LDI,
    STO,
    STI,
    CPY,
    INP,
    INL,
    CAL,
    INT,
    JMP,
    JPC,
    OPR,
    RET
};

enum class OperationCode {
    NEG = 1,
    ADD = 2,
    SUB = 3,
    MUL = 4,
    DIV = 5,
    MOD = 6,
    EQL = 7,
    NEQ = 8,
    LSS = 9,
    GEQ = 10,
    GTR = 11,
    LEQ = 12,
    WRT = 13,
    WRTLN = 14,
    RDIV = 15
};

struct Instruction {
    OpCode opcode = OpCode::RET;
    int level = 0;
    int operand = 0;
    RuntimeValue literal;
    bool hasLiteral = false;
    bool indirect = false;
    std::string comment;
};

struct RoutineMetadata {
    int address = 0;
    int parameterCount = 0;
    int frameSize = 0;
    bool returnsValue = false;
    int returnValueOffset = 0;
    std::vector<bool> byReferenceParameters;
};

struct IntermediateProgram {
    std::vector<Instruction> instructions;
    std::vector<RoutineMetadata> routines;
};

std::string opcodeName(OpCode opcode);
std::string operationName(OperationCode operation);
std::string formatInstruction(const Instruction& instruction, int address);
std::vector<std::string> formatProgram(const IntermediateProgram& program);

#endif
