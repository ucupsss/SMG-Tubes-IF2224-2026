#include "lexer.hpp"
#include <cctype>
#include <algorithm>

Lexer::Lexer(const std::string& source) : content(source), pos(0) {
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

char Lexer::peek() {
    if (pos >= content.length()) {
        return '\0';
    }

    return content[pos];
}

char Lexer::advance() {
    return content[pos++];
}

void Lexer::skipWhiteSpaceAndComments() {
    while (pos < content.length()) {
        char c = peek();

        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();

        } else if (c == '{') {
            advance();

            while (pos < content.length() && peek() != '}') {
                advance();
            }

            if (peek() == '}') {
                advance();
            }

        } else if (c == '(' && pos + 1 < content.length() && content[pos + 1] == '*') {
            advance();
            advance();

            while (pos + 1 < content.length() && !(content[pos] == '*' && content[pos + 1] == ')')) {
                advance();
            }

            if (pos + 1 < content.length()) {
                advance();
                advance();
            }

        } else {
            break;
        }
    }
}

Token Lexer::getNextToken() {
    skipWhiteSpaceAndComments();

    if (pos >= content.length()) {
        return {TokenType::END_OF_FILE, ""}; 
    }

    char c = peek();

    // DFA State: Identifier & Keywords
    if (std::isalpha(static_cast<unsigned char>(c))) {
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

        if (keywords.count(lowerResult)) {
            return {keywords[lowerResult], result};
        }

        return {TokenType::IDENT, result};
    }

    // DFA State: Numbers (Integer & Real) - including negative numbers
    if (std::isdigit(static_cast<unsigned char>(c)) || 
        (c == '-' && pos + 1 < content.length() && 
         std::isdigit(static_cast<unsigned char>(content[pos + 1])))) {
        std::string result;

        // Handle negative sign
        if (c == '-') {
            result += advance();
        }

        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            result += advance();
        }

        if (peek() == '.' && pos + 1 < content.length() &&
            std::isdigit(static_cast<unsigned char>(content[pos + 1]))) {
            result += advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                result += advance();
            }
            
            return {TokenType::REALCON, result};
        }

        return {TokenType::INTCON, result};
    }

    // DFA State: String & Charcon
    if (c == '\'') {
        advance();
        std::string result;

        while (pos < content.length() && peek() != '\'') {
            result += advance();
        }

        advance();

        if (result.length() == 1) {
            return {TokenType::CHARCON, result};
        }

        return {TokenType::STRING, result};
    }

    // DFA State: Operators & Symbols
    advance();
    switch(c) {
        case '+': return {TokenType::PLUS, "+"};
        case '-': return {TokenType::MINUS, "-"};
        case '*': return {TokenType::TIMES, "*"};
        case '/': return {TokenType::RDIV, "/"};
        case ',': return {TokenType::COMMA, ","};
        case ';': return {TokenType::SEMICOLON, ";"};
        case '.': return {TokenType::PERIOD, "."};
        case '(': return {TokenType::LPARENT, "("};
        case ')': return {TokenType::RPARENT, ")"};
        case '[': return {TokenType::LBRACK, "["};
        case ']': return {TokenType::RBRACK, "]"};
        case '=':
            if (peek() == '=') {
                advance();
                return {TokenType::EQL, "=="};
            }

            return {TokenType::UNKNOWN, std::string(1, c)};
        case ':':
            if (peek() == '=') {
                advance();
                return {TokenType::BECOMES, ":="};
            }

            return  {TokenType::COLON, ":"};
        case '>':
            if (peek() == '=') {
                advance();
                return {TokenType::GEQ, ">="};
            }
            
            return {TokenType::GTR, ">"};
        case '<':
            if (peek() == '=') {
                advance();
                return {TokenType::LEQ, "<="};
            }

            if (peek() == '>') {
                advance();
                return {TokenType::NEQ, "<>"};
            }

            return {TokenType::LSS, "<"};
        
    }

    return {TokenType::UNKNOWN, std::string(1, c)};
}