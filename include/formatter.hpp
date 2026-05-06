#ifndef FORMATTER_HPP
#define FORMATTER_HPP

#include <string>
#include <vector>

#include "lexer.hpp"
#include "parser.hpp"

std::string tokenTypeToString(TokenType type);
std::string formatToken(const Token& token);
std::vector<std::string> formatParseTree(const ParseNode& root);

#endif
