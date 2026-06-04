#include "interpreter.hpp"

#include <limits>
#include <string>

namespace {

bool isNumericKind(RuntimeValueKind kind) {
    return kind == RuntimeValueKind::Integer ||
           kind == RuntimeValueKind::Real ||
           kind == RuntimeValueKind::Boolean ||
           kind == RuntimeValueKind::Char;
}

bool isIntegerLikeKind(RuntimeValueKind kind) {
    return kind == RuntimeValueKind::Integer ||
           kind == RuntimeValueKind::Boolean ||
           kind == RuntimeValueKind::Char;
}

int integerLikeValue(const RuntimeValue& value) {
    switch (value.kind) {
        case RuntimeValueKind::Integer:
            return value.integerValue;
        case RuntimeValueKind::Boolean:
            return value.booleanValue ? 1 : 0;
        case RuntimeValueKind::Char:
            return static_cast<unsigned char>(value.charValue);
        default:
            return 0;
    }
}

double numericValue(const RuntimeValue& value) {
    return value.kind == RuntimeValueKind::Real
        ? value.realValue
        : static_cast<double>(integerLikeValue(value));
}

bool fitsInteger(long long value) {
    return value >= std::numeric_limits<int>::min() &&
           value <= std::numeric_limits<int>::max();
}

}

bool StackMachineInterpreter::executeOperation(OperationCode operation) {
    switch (operation) {
        // NEG (1)
        case OperationCode::NEG: {
            RuntimeValue a = pop();
            if (result.halted) return false;
            if (a.kind == RuntimeValueKind::Integer) {
                if (a.integerValue == std::numeric_limits<int>::min()) {
                    runtimeError("NEG: integer overflow");
                    return false;
                }
                push(RuntimeValue::integer(-a.integerValue));
            } else if (a.kind == RuntimeValueKind::Real) {
                push(RuntimeValue::real(-a.realValue));
            } else {
                runtimeError("NEG: tipe data tidak valid untuk negasi");
                return false;
            }
            break;
        }
        // ADD (2), SUB (3), MUL (4), DIV (5), MOD (6)
        case OperationCode::ADD:
        case OperationCode::SUB:
        case OperationCode::MUL:
        case OperationCode::DIV:
        case OperationCode::MOD:
            return executeBinaryNumeric(operation);
        // EQL (7), NEQ (8), LSS (9), GEQ (10), GTR (11), LEQ (12)
        case OperationCode::EQL:
        case OperationCode::NEQ:
        case OperationCode::LSS:
        case OperationCode::GEQ:
        case OperationCode::GTR:
        case OperationCode::LEQ:
            return executeComparison(operation);
        // WRT (13) cetak nilai
        case OperationCode::WRT: {
            RuntimeValue value = pop();
            if (result.halted) return false;
            currentOutput += value.display();
            break;
        }
        // WRTLN (14) — cetak nilai ke output lalu newline (simpan sebagai baris)
        case OperationCode::WRTLN: {
            RuntimeValue value = pop();
            if (result.halted) return false;
            currentOutput += value.display();
            result.outputLines.push_back(currentOutput);
            currentOutput.clear();
            break;
        }
    }
    return !result.halted;
}

bool StackMachineInterpreter::executeBinaryNumeric(OperationCode operation) {
    RuntimeValue b = pop();
    if (result.halted) return false;
    RuntimeValue a = pop();
    if (result.halted) return false;

    if (!isNumericKind(a.kind) || !isNumericKind(b.kind)) {
        runtimeError("numeric operation requires integer or real operands");
        return false;
    }

    const bool bothInt = isIntegerLikeKind(a.kind) && isIntegerLikeKind(b.kind);
    const double va = numericValue(a);
    const double vb = numericValue(b);
    const int ia = integerLikeValue(a);
    const int ib = integerLikeValue(b);

    switch (operation) {
        case OperationCode::ADD: {
            if (bothInt) {
                const long long value =
                    static_cast<long long>(ia) + static_cast<long long>(ib);
                if (!fitsInteger(value)) {
                    runtimeError("ADD: integer overflow");
                    return false;
                }
                push(RuntimeValue::integer(static_cast<int>(value)));
            } else {
                push(RuntimeValue::real(va + vb));
            }
            break;
        }
        case OperationCode::SUB: {
            if (bothInt) {
                const long long value =
                    static_cast<long long>(ia) - static_cast<long long>(ib);
                if (!fitsInteger(value)) {
                    runtimeError("SUB: integer overflow");
                    return false;
                }
                push(RuntimeValue::integer(static_cast<int>(value)));
            } else {
                push(RuntimeValue::real(va - vb));
            }
            break;
        }
        case OperationCode::MUL: {
            if (bothInt) {
                const long long value =
                    static_cast<long long>(ia) * static_cast<long long>(ib);
                if (!fitsInteger(value)) {
                    runtimeError("MUL: integer overflow");
                    return false;
                }
                push(RuntimeValue::integer(static_cast<int>(value)));
            } else {
                push(RuntimeValue::real(va * vb));
            }
            break;
        }
        case OperationCode::DIV: {
            if (bothInt) {
                if (ib == 0) {
                    runtimeError("division by zero (div)");
                    return false;
                }
                if (ia == std::numeric_limits<int>::min() && ib == -1) {
                    runtimeError("DIV: integer overflow");
                    return false;
                }
                push(RuntimeValue::integer(ia / ib));
            } else {
                if (vb == 0.0) {
                    runtimeError("division by zero (/)");
                    return false;
                }
                push(RuntimeValue::real(va / vb));
            }
            break;
        }
        case OperationCode::MOD: {
            if (!bothInt) {
                runtimeError("MOD: kedua operand harus integer");
                return false;
            }
            if (ib == 0) {
                runtimeError("modulo by zero (mod)");
                return false;
            }
            if (ia == std::numeric_limits<int>::min() && ib == -1) {
                push(RuntimeValue::integer(0));
                return true;
            }
            push(RuntimeValue::integer(ia % ib));
            break;
        }
        default:
            break;
    }
    return !result.halted;
}



bool StackMachineInterpreter::executeComparison(OperationCode operation) {
    RuntimeValue b = pop();
    if (result.halted) return false;
    RuntimeValue a = pop();
    if (result.halted) return false;
    bool resultBool = false;
    // Perbandingan integer dengan integer
    if (a.kind == RuntimeValueKind::Integer && b.kind == RuntimeValueKind::Integer) {
        const int va = a.integerValue;
        const int vb = b.integerValue;
        switch (operation) {
            case OperationCode::EQL: resultBool = (va == vb); break;
            case OperationCode::NEQ: resultBool = (va != vb); break;
            case OperationCode::LSS: resultBool = (va <  vb); break;
            case OperationCode::GEQ: resultBool = (va >= vb); break;
            case OperationCode::GTR: resultBool = (va >  vb); break;
            case OperationCode::LEQ: resultBool = (va <= vb); break;
            default: break;
        }
    }
    // Perbandingan real
    else if ((a.kind == RuntimeValueKind::Integer || a.kind == RuntimeValueKind::Real) && (b.kind == RuntimeValueKind::Integer || b.kind == RuntimeValueKind::Real)) {
        const double va = (a.kind == RuntimeValueKind::Integer) ? static_cast<double>(a.integerValue) : a.realValue;
        const double vb = (b.kind == RuntimeValueKind::Integer) ? static_cast<double>(b.integerValue) : b.realValue;
        switch (operation) {
            case OperationCode::EQL: resultBool = (va == vb); break;
            case OperationCode::NEQ: resultBool = (va != vb); break;
            case OperationCode::LSS: resultBool = (va <  vb); break;
            case OperationCode::GEQ: resultBool = (va >= vb); break;
            case OperationCode::GTR: resultBool = (va >  vb); break;
            case OperationCode::LEQ: resultBool = (va <= vb); break;
            default: break;
        }
    }
    // Perbandingan boolean
    else if (a.kind == RuntimeValueKind::Boolean && b.kind == RuntimeValueKind::Boolean) {
        const bool va = a.booleanValue;
        const bool vb = b.booleanValue;
        switch (operation) {
            case OperationCode::EQL: resultBool = (va == vb); break;
            case OperationCode::NEQ: resultBool = (va != vb); break;
            default:
                runtimeError("perbandingan < > <= >= tidak valid untuk boolean");
                return false;
        }
    }
    // Perbandingan char
    else if (a.kind == RuntimeValueKind::Char && b.kind == RuntimeValueKind::Char) {
        const char va = a.charValue;
        const char vb = b.charValue;
        switch (operation) {
            case OperationCode::EQL: resultBool = (va == vb); break;
            case OperationCode::NEQ: resultBool = (va != vb); break;
            case OperationCode::LSS: resultBool = (va <  vb); break;
            case OperationCode::GEQ: resultBool = (va >= vb); break;
            case OperationCode::GTR: resultBool = (va >  vb); break;
            case OperationCode::LEQ: resultBool = (va <= vb); break;
            default: break;
        }
    }
    // Perbandingan string (hanya == dan <>)
    else if (a.kind == RuntimeValueKind::String && b.kind == RuntimeValueKind::String) {
        switch (operation) {
            case OperationCode::EQL: resultBool = (a.stringValue == b.stringValue); break;
            case OperationCode::NEQ: resultBool = (a.stringValue != b.stringValue); break;
            default:
                runtimeError("perbandingan < > <= >= tidak valid untuk string");
                return false;
        }
    }
    else {
        runtimeError("perbandingan antara tipe data yang tidak kompatibel");
        return false;
    }
    push(RuntimeValue::boolean(resultBool));
    return !result.halted;
}
