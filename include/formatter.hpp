#ifndef FORMATTER_HPP
#define FORMATTER_HPP

#include <string>

#include "lexer.hpp"

std::string tokenTypeToString(TokenType type);
std::string formatToken(const Token& token);
bool isLexerWarning(const Token& token);

#endif
