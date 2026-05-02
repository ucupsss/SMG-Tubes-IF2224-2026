#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "filehandler.hpp"
#include "formatter.hpp"
#include "lexer.hpp"

namespace fs = std::filesystem;

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

    const fs::path inputPath = fs::path("test") / "milestone-1" / "input" / inputFileName;
    const fs::path outputPath = fs::path("test") / "milestone-1" / "output" / outputFileName;

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
