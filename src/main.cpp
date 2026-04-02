#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "lexer.hpp"

namespace fs = std::filesystem;

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
           type == TokenType::STRING ||
           type == TokenType::UNKNOWN;
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

bool hasTxtExtension(const fs::path& filePath) {
    return filePath.extension() == ".txt";
}

bool isLexerWarning(const Token& token) {
    return token.type == TokenType::UNKNOWN && 
            (token.value == "string tidak ditutup sebelum akhir file" || 
                token.value == "komentar tidak ditutup sebelum akhir file");
}

bool readInputFile(const fs::path& inputPath, std::string& source) {
    if (!hasTxtExtension(inputPath)) {
        std::cerr << "Error: file input harus berekstensi .txt\n";
        return false;
    }

    std::ifstream file(inputPath);
    if (!file.is_open()) {
        std::cerr << "Gagal membuka file input: " << inputPath.string() << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    source = buffer.str();
    return true;
}

bool writeOutputFile(const fs::path& outputPath, const std::vector<std::string>& lines) {
    if (!hasTxtExtension(outputPath)) {
        std::cerr << "Error: file output harus berekstensi .txt\n";
        return false;
    }

    fs::create_directories(outputPath.parent_path());

    std::ofstream file(outputPath);
    if (!file.is_open()) {
        std::cerr << "Gagal membuat file output: " << outputPath.string() << "\n";
        return false;
    }

    for (const std::string& line : lines) {
        file << line << "\n";
    }

    return true;
}

int main() {
    std::string inputFileName;
    std::string outputFileName;

    std::cout << "Masukkan nama file input: ";
    std::getline(std::cin, inputFileName);

    std::cout << "Masukkan nama file output: ";
    std::getline(std::cin, outputFileName);

    if (inputFileName.empty() || outputFileName.empty()) {
        std::cerr << "Error: nama file input dan output tidak boleh kosong\n";
        return 1;
    }

    const fs::path inputPath = fs::path("test") / "input" / inputFileName;
    const fs::path outputPath = fs::path("test") / "output" / outputFileName;

    std::string source;
    if (!readInputFile(inputPath, source)) {
        return 1;
    }

    Lexer lexer(source);
    std::vector<std::string> outputLines;

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }

        if (isLexerWarning(token)) {
            std::cerr << "Warning: " << token.value << "\n";
        }

        outputLines.push_back(formatToken(token));
    }

    if (!writeOutputFile(outputPath, outputLines)) {
        return 1;
    }

    std::cout << "Output berhasil ditulis ke " << outputPath.string() << "\n";
    return 0;
}
