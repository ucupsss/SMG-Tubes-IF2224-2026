#include "driver.hpp"

#include "formatter.hpp"
#include "lexer.hpp"

#include <iostream>

std::vector<std::string> runLexer(const std::string& source) {
    Lexer lexer(source);
    std::vector<std::string> outputLines;

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }

        if (isLexerWarning(token)) {
            std::cerr << "Warning lexer baris " << token.line
                      << ", kolom " << token.column
                      << ": " << token.value << "\n";
        }

        outputLines.push_back(formatToken(token));
    }

    return outputLines;
}
