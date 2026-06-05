#include "intermediate.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace {

std::string escapeCharacter(char value) {
    switch (value) {
        case '\0': return "\\0";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\\': return "\\\\";
        case '\'': return "\\'";
        default: return std::string(1, value);
    }
}

std::string formatLiteralOperand(const RuntimeValue& value) {
    switch (value.kind) {
        case RuntimeValueKind::Void:
            return "void";
        case RuntimeValueKind::Integer:
            return std::to_string(value.integerValue);
        case RuntimeValueKind::Real: {
            std::ostringstream output;
            output << std::setprecision(15) << value.realValue;
            return output.str();
        }
        case RuntimeValueKind::Boolean:
            return value.booleanValue ? "true" : "false";
        case RuntimeValueKind::Char:
            return "'" + escapeCharacter(value.charValue) + "'";
        case RuntimeValueKind::String: {
            std::ostringstream output;
            output << std::quoted(value.stringValue);
            return output.str();
        }
    }

    return "void";
}

} // namespace

RuntimeValue RuntimeValue::voidValue() {
    return {};
}

RuntimeValue RuntimeValue::integer(int value) {
    RuntimeValue result;
    result.kind = RuntimeValueKind::Integer;
    result.integerValue = value;
    return result;
}

RuntimeValue RuntimeValue::real(double value) {
    RuntimeValue result;
    result.kind = RuntimeValueKind::Real;
    result.realValue = value;
    return result;
}

RuntimeValue RuntimeValue::boolean(bool value) {
    RuntimeValue result;
    result.kind = RuntimeValueKind::Boolean;
    result.booleanValue = value;
    return result;
}

RuntimeValue RuntimeValue::character(char value) {
    RuntimeValue result;
    result.kind = RuntimeValueKind::Char;
    result.charValue = value;
    return result;
}

RuntimeValue RuntimeValue::string(std::string value) {
    RuntimeValue result;
    result.kind = RuntimeValueKind::String;
    result.stringValue = std::move(value);
    return result;
}

bool RuntimeValue::isTruthy() const {
    switch (kind) {
        case RuntimeValueKind::Void:
            return false;
        case RuntimeValueKind::Integer:
            return integerValue != 0;
        case RuntimeValueKind::Real:
            return realValue != 0.0;
        case RuntimeValueKind::Boolean:
            return booleanValue;
        case RuntimeValueKind::Char:
            return charValue != '\0';
        case RuntimeValueKind::String:
            return !stringValue.empty();
    }

    return false;
}

std::string RuntimeValue::display() const {
    switch (kind) {
        case RuntimeValueKind::Void:
            return "";
        case RuntimeValueKind::Integer:
            return std::to_string(integerValue);
        case RuntimeValueKind::Real: {
            std::ostringstream output;
            output << std::setprecision(15) << realValue;
            return output.str();
        }
        case RuntimeValueKind::Boolean:
            return booleanValue ? "true" : "false";
        case RuntimeValueKind::Char:
            return std::string(1, charValue);
        case RuntimeValueKind::String:
            return stringValue;
    }

    return "";
}

std::string opcodeName(OpCode opcode) {
    switch (opcode) {
        case OpCode::LIT: return "LIT";
        case OpCode::LOD: return "LOD";
        case OpCode::LDA: return "LDA";
        case OpCode::LDI: return "LDI";
        case OpCode::LDR: return "LDR";
        case OpCode::STO: return "STO";
        case OpCode::STI: return "STI";
        case OpCode::CPY: return "CPY";
        case OpCode::INP: return "INP";
        case OpCode::INL: return "INL";
        case OpCode::CAL: return "CAL";
        case OpCode::INT: return "INT";
        case OpCode::JMP: return "JMP";
        case OpCode::JPC: return "JPC";
        case OpCode::OPR: return "OPR";
        case OpCode::RET: return "RET";
    }

    return "UNKNOWN";
}

std::string operationName(OperationCode operation) {
    switch (operation) {
        case OperationCode::NEG: return "NEG";
        case OperationCode::ADD: return "ADD";
        case OperationCode::SUB: return "SUB";
        case OperationCode::MUL: return "MUL";
        case OperationCode::DIV: return "DIV";
        case OperationCode::MOD: return "MOD";
        case OperationCode::EQL: return "EQL";
        case OperationCode::NEQ: return "NEQ";
        case OperationCode::LSS: return "LSS";
        case OperationCode::GEQ: return "GEQ";
        case OperationCode::GTR: return "GTR";
        case OperationCode::LEQ: return "LEQ";
        case OperationCode::WRT: return "WRT";
        case OperationCode::WRTLN: return "WRTLN";
        case OperationCode::RDIV: return "RDIV";
    }

    return "UNKNOWN";
}

std::string formatInstruction(const Instruction& instruction, int address) {
    std::ostringstream output;
    output << address << ' '
           << opcodeName(instruction.opcode) << ' '
           << instruction.level << ' ';

    if (instruction.opcode == OpCode::LIT && instruction.hasLiteral) {
        output << formatLiteralOperand(instruction.literal);
    } else {
        output << instruction.operand;
    }

    if (!instruction.comment.empty()) {
        output << " ; " << instruction.comment;
    } else if (instruction.indirect) {
        output << " ; indirect";
    }

    return output.str();
}

std::vector<std::string> formatProgram(const IntermediateProgram& program) {
    std::vector<std::string> lines;
    lines.reserve(program.instructions.size());

    for (size_t i = 0; i < program.instructions.size(); ++i) {
        lines.push_back(formatInstruction(program.instructions[i], static_cast<int>(i)));
    }

    return lines;
}
