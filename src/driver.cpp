#include "driver.hpp"

#include "formatter.hpp"
#include "lexer.hpp"
#include "parser.hpp"

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

std::vector<std::string> runSyntaxAnalyzer(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens;

    while (true) {
        Token token = lexer.getNextToken();

        if (isLexerWarning(token)) {
            std::cerr << "Warning lexer baris " << token.line
                      << ", kolom " << token.column
                      << ": " << token.value << "\n";
        }

        if (token.type == TokenType::UNKNOWN) {
            return {"Lexical error at line " +
                    std::to_string(token.line) + ", column " +
                    std::to_string(token.column) + ": " + token.value};
        }

        if (token.type != TokenType::COMMENT) {
            tokens.push_back(token);
        }

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }

    try {
        Parser parser(tokens);
        ParseNode parseTree = parser.parse();
        return formatParseTree(parseTree);
    } catch (const ParseError& error) {
        return {error.what()};
    }
}
