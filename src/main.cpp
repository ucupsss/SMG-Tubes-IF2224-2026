#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>

#include "lexer.hpp"

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INTCON: return "intcon";
        case TokenType::REALCON: return "realcon";
        case TokenType::CHARCON: return "charcon";
        case TokenType::STRING: return "string";
        case TokenType::NOTSY: return "notsy";
        case TokenType::PLUS: return "plus";
        case TokenType::MINUS: return "minus";
        case TokenType::TIMES: return "times";
        case TokenType::IDIV: return "idiv";
        case TokenType::RDIV: return "rdiv";
        case TokenType::IMOD: return "imod";
        case TokenType::ANDSY: return "andsy";
        case TokenType::ORSY: return "orsy";
        case TokenType::EQL: return "eql";
        case TokenType::NEQ: return "neq";
        case TokenType::GTR: return "gtr";
        case TokenType::GEQ: return "geq";
        case TokenType::LSS: return "lss";
        case TokenType::LEQ: return "leq";
        case TokenType::LPARENT: return "lparent";
        case TokenType::RPARENT: return "rparent";
        case TokenType::LBRACK: return "lbrack";
        case TokenType::RBRACK: return "rbrack";
        case TokenType::COMMA: return "comma";
        case TokenType::SEMICOLON: return "semicolon";
        case TokenType::PERIOD: return "period";
        case TokenType::COLON: return "colon";
        case TokenType::BECOMES: return "becomes";
        case TokenType::CONSTSY: return "constsy";
        case TokenType::TYPESY: return "typesy";
        case TokenType::VARSY: return "varsy";
        case TokenType::FUNCTIONSY: return "functionsy";
        case TokenType::PROCEDURESY: return "proceduresy";
        case TokenType::ARRAYSY: return "arraysy";
        case TokenType::RECORDSY: return "recordsy";
        case TokenType::PROGRAMSY: return "programsy";
        case TokenType::IDENT: return "ident";
        case TokenType::BEGINSY: return "beginsy";
        case TokenType::IFSY: return "ifsy";
        case TokenType::CASESY: return "casesy";
        case TokenType::REPEATSY: return "repeatsy";
        case TokenType::WHILESY: return "whilesy";
        case TokenType::FORSY: return "forsy";
        case TokenType::ENDSY: return "endsy";
        case TokenType::ELSESY: return "elsesy";
        case TokenType::UNTILSY: return "untilsy";
        case TokenType::OFSY: return "ofsy";
        case TokenType::DOSY: return "dosy";
        case TokenType::TOSY: return "tosy";
        case TokenType::DOWNTOSY: return "downtosy";
        case TokenType::THENSY: return "thensy";
        case TokenType::UNKNOWN: return "unknown";
        case TokenType::END_OF_FILE: return "eof";
        default: return "unknown";
    }
}

bool needsValue(TokenType type) {
    return type == TokenType::IDENT ||
           type == TokenType::INTCON ||
           type == TokenType::REALCON ||
           type == TokenType::CHARCON ||
           type == TokenType::STRING;
}

std::string formatToken(const Token& token) {
    std::string result = tokenTypeToString(token.type);

    if (needsValue(token.type)) {
        if (token.type == TokenType::STRING || token.type == TokenType::CHARCON) {
            result += " ('" + token.value + "')";
        } else {
            result += " (" + token.value + ")";
        }
    }

    return result;
}

std::string readInput(int argc, char* argv[]) {
    if (argc > 1) {
        const std::filesystem::path inputPath(argv[1]);

        if (inputPath.extension() != ".txt") {
            std::cerr << "Error: file input harus berekstensi .txt\n";
            return "";
        }

        std::ifstream file(inputPath);
        if (!file.is_open()) {
            std::cerr << "Gagal membuka file input!\n";
            return "";
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    std::string source = readInput(argc, argv);
    if (source.empty()) {
        return 0;
    }

    Lexer lexer(source);

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }

        if (token.type == TokenType::UNKNOWN) {
            std::cerr << "Error: token tidak dikenali: " << token.value << "\n";
            return 1;
        }

        std::cout << formatToken(token) << "\n";
    }

    return 0;
}