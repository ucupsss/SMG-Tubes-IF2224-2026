#include "driver.hpp"

#include "formatter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

namespace {

std::string formatLexicalError(const Token& token) {
    return "Lexical error at line " +
           std::to_string(token.line) +
           ", column " +
           std::to_string(token.column) +
           ": " +
           token.value;
}

}

std::vector<std::string> runLexer(const std::string& source) {
    Lexer lexer(source);
    std::vector<std::string> outputLines;

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::END_OF_FILE) {
            break;
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

        if (token.type == TokenType::UNKNOWN) {
            return {formatLexicalError(token)};
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

std::vector<std::string> runSemanticAnalyzer(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens;

    while(true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::UNKNOWN) {
            return {formatLexicalError(token)};
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
        SemanticAnalyzer analyzer;
        analyzer.analyze(parseTree);
        return analyzer.formatOutput();
    } catch (const ParseError& error) {
        return {error.what()};
    }

}
