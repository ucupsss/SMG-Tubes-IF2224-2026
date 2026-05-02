#include "lexer.hpp"

#include <cctype>
#include <algorithm>

Lexer::Lexer(const std::string& source) 
    : content(source), 
      pos(0),
      line(1),
      column(1) {
    initKeywords();
}

void Lexer::initKeywords() {
    keywords["not"] = TokenType::NOTSY;
    keywords["div"] = TokenType::IDIV;
    keywords["mod"] = TokenType::IMOD;
    keywords["and"] = TokenType::ANDSY;
    keywords["or"] = TokenType::ORSY;
    keywords["const"] = TokenType::CONSTSY;
    keywords["type"] = TokenType::TYPESY;
    keywords["var"] = TokenType::VARSY;
    keywords["function"] = TokenType::FUNCTIONSY;
    keywords["procedure"] = TokenType::PROCEDURESY; 
    keywords["array"] = TokenType::ARRAYSY;
    keywords["record"] = TokenType::RECORDSY;
    keywords["program"] = TokenType::PROGRAMSY;
    keywords["begin"] = TokenType::BEGINSY;
    keywords["if"] = TokenType::IFSY;
    keywords["case"] = TokenType::CASESY;
    keywords["repeat"] = TokenType::REPEATSY;
    keywords["while"] = TokenType::WHILESY;
    keywords["for"] = TokenType::FORSY;
    keywords["end"] = TokenType::ENDSY;
    keywords["else"] = TokenType::ELSESY;
    keywords["until"] = TokenType::UNTILSY;
    keywords["of"] = TokenType::OFSY;
    keywords["do"] = TokenType::DOSY;
    keywords["to"] = TokenType::TOSY;
    keywords["downto"] = TokenType::DOWNTOSY;
    keywords["then"] = TokenType::THENSY;
}

char Lexer::peek() const {
    if (pos >= content.length()) {
        return '\0';
    }

    return content[pos];
}

char Lexer::peekNext() const {
    if (pos + 1 >= content.length()) {
        return '\0';
    }

    return content[pos + 1];
}

char Lexer::advance() {
    char c = content[pos++];

    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }

    return c;
}

bool Lexer::isSeparator(char c) const {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

bool Lexer::isTokenBoundary(char c) const {
    return c == '\0' || 
            isSeparator(c) ||
            c == '+' || c == '-' || c == '*' || c == '/' ||
            c == ',' || c == ';' || c == ':' ||
            c == '(' || c == ')' || c == '[' || c ==']' ||
            c == '<' || c == '>' || c == '=';
}

void Lexer::skipSeparators() {
    while (pos < content.length() && isSeparator(peek())) {
        advance();
    }
}

Token Lexer::makeToken(TokenType type, const std::string& value, int startLine, int startColumn) const {
    return {type, value, startLine, startColumn};
}

std::string Lexer::consumeUnknownSequence() {
    std::string result;

    while (pos < content.length() && !isSeparator(peek())) {
        result += advance();
    }

    return result;
}

Token Lexer::readBraceComment(int startLine, int startColumn) {
    std::string comment;
    comment += advance();

    while (pos < content.length() && peek() != '}') {
        comment += advance();
    }

    if (peek() == '}') {
        comment += advance();
        return makeToken(TokenType::COMMENT, comment, startLine, startColumn);
    }

    return makeToken(TokenType::UNKNOWN, "komentar tidak ditutup sebelum akhir file", startLine, startColumn);
}

Token Lexer::readParenStarComment(int startLine, int startColumn) {
    std::string comment;
    comment += advance();
    comment += advance();

    while (pos < content.length()) {
        if (peek() == '*' && peekNext() == ')') {
            comment += advance();
            comment += advance();
            return makeToken(TokenType::COMMENT, comment, startLine, startColumn);
        }

        comment += advance();
    }

    return makeToken(TokenType::UNKNOWN, "komentar tidak ditutup sebelum akhir file", startLine, startColumn);
}

Token Lexer::readIdentifierOrKeyword(int startLine, int startColumn) {
    std::string result;

    while (std::isalnum(static_cast<unsigned char>(peek()))) {
        result += advance();
    }

    std::string lowerResult = result;
    std::transform(
        lowerResult.begin(),
        lowerResult.end(),
        lowerResult.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );

    auto keyword = keywords.find(lowerResult);
    if (keyword != keywords.end()) {
        return makeToken(keyword->second, result, startLine, startColumn);
    }

    return makeToken(TokenType::IDENT, result, startLine, startColumn);
}

Token Lexer::readNumber(int startLine, int startColumn) {
    std::string result;

    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        result += advance();
    }

    // Range: 1..10 -> intcon(1), period, period, intcon(10)
    if (peek() == '.' && peekNext() == '.') {
        return makeToken(TokenType::INTCON, result, startLine, startColumn);
    }

    // Real number: 1.23 -> realcon(1.23)
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        result += advance();

        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            result += advance();
        }

        // Real range: 1.23..4.56 -> realcon(1.23), period, period, realcon(4.56)
        if (peek() == '.' && peekNext() == '.') {
            return makeToken(TokenType::REALCON, result, startLine, startColumn);
        }

        // 1.23abc or 1.23.abc -> malformed sequence
        if (!isTokenBoundary(peek()) && peek() != '.') {
            result += consumeUnknownSequence();
            return makeToken(TokenType::UNKNOWN, result, startLine, startColumn);
        }

        // 1.23.abc -> consume as unknown, not realcon(1.23), period, ident(abc)
        if (peek() == '.') {
            result += consumeUnknownSequence();
            return makeToken(TokenType::UNKNOWN, result, startLine, startColumn);
        }

        return makeToken(TokenType::REALCON, result, startLine, startColumn);
    }

    // 1. -> intcon(1), period
    if (peek() == '.') {
        return makeToken(TokenType::INTCON, result, startLine, startColumn);
    }

    // 123abc -> malformed sequence
    if (!isTokenBoundary(peek())) {
        result += consumeUnknownSequence();
        return makeToken(TokenType::UNKNOWN, result, startLine, startColumn);
    }

    return makeToken(TokenType::INTCON, result, startLine, startColumn);
}
    
Token Lexer::readStringOrChar(int startLine, int startColumn) {
    advance();

    std::string result;
    bool hasEscapedQuote = false;

    while (pos < content.length()) {
        if (peek() == '\'') {
            if (peekNext() == '\'') {
                result += '\'';
                hasEscapedQuote = true;
                advance();
                advance();
                continue;
            }

            break;
        }

        result += advance();
    } 

    if (peek() == '\'') {
        advance();
    } else {
        return makeToken(TokenType::UNKNOWN, "string tidak ditutup sebelum akhir file", startLine, startColumn);
    }

    if (!hasEscapedQuote && result.length() == 1) {
        return makeToken(TokenType::CHARCON, result, startLine, startColumn);
    }

    return makeToken(TokenType::STRING, result, startLine, startColumn);
}

Token Lexer::readDotStartedToken(int startLine, int startColumn) {
    if (pos > 0 && content[pos - 1] == '.') {
        advance();
        return makeToken(TokenType::PERIOD, ".", startLine, startColumn);
    }

    if (std::isdigit(static_cast<unsigned char>(peekNext()))) {
        std::string result;
        result += advance();
        result += consumeUnknownSequence();
        return makeToken(TokenType::UNKNOWN, result, startLine, startColumn);
    }

    advance();
    return makeToken(TokenType::PERIOD, ".", startLine, startColumn);
}

Token Lexer::getNextToken() {
    skipSeparators();

    if (pos >= content.length()) {
        return makeToken(TokenType::END_OF_FILE, "", line, column);
    }

    int startLine = line;
    int startColumn = column;
    char c = peek();

    if (c == '{') {
        return readBraceComment(startLine, startColumn);
    }

    if (c == '(' && peekNext() == '*') {
        return readParenStarComment(startLine, startColumn);
    }

    if (std::isalpha(static_cast<unsigned char>(c))) {
        return readIdentifierOrKeyword(startLine, startColumn);
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
        return readNumber(startLine, startColumn);
    }

    if (c == '\'') {
        return readStringOrChar(startLine, startColumn);
    }

    if (c == '.') {
        return readDotStartedToken(startLine, startColumn);
    }

    advance();

    // DFA State: Operators & Symbols
    switch(c) {
        case '+':
            return makeToken(TokenType::PLUS, "+", startLine, startColumn);

        case '-':
            return makeToken(TokenType::MINUS, "-", startLine, startColumn);

        case '*':
            return makeToken(TokenType::TIMES, "*", startLine, startColumn);
            
        case '/':
            return makeToken(TokenType::RDIV, "/", startLine, startColumn);

        case ',':
            return makeToken(TokenType::COMMA, ",", startLine, startColumn);

        case ';':
            return makeToken(TokenType::SEMICOLON, ";", startLine, startColumn);

        case '(':
            return makeToken(TokenType::LPARENT, "(", startLine, startColumn);

        case ')':
            return makeToken(TokenType::RPARENT, ")", startLine, startColumn);

        case '[':
            return makeToken(TokenType::LBRACK, "[", startLine, startColumn);

        case ']':
            return makeToken(TokenType::RBRACK, "]", startLine, startColumn);

        case '=':
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::EQL, "==", startLine, startColumn);
            }

            return makeToken(TokenType::UNKNOWN, std::string(1, c), startLine, startColumn);

        case ':':
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::BECOMES, ":=", startLine, startColumn);
            }

            return makeToken(TokenType::COLON, ":", startLine, startColumn);

        case '>':
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::GEQ, ">=", startLine, startColumn);
            }
            
            return makeToken(TokenType::GTR, ">", startLine, startColumn);
        case '<':
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::LEQ, "<=", startLine, startColumn);
            }

            if (peek() == '>') {
                advance();
                return makeToken(TokenType::NEQ, "<>", startLine, startColumn);
            }

            return makeToken(TokenType::LSS, "<", startLine, startColumn);
        
    }

    return makeToken(TokenType::UNKNOWN, std::string(1, c), startLine, startColumn);
}
