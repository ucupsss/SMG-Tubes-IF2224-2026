#include "lexer.hpp"
#include <cctype>
#include <algorithm>

Lexer::Lexer(const std::string& source) : content(source), pos(0) {
    initKeywords();
}

void Lexer::initKeywords() {
    keywords["NOT"] = TokenType::NOTSY;
    keywords["div"] = TokenType::IDIV;
    keywords["MOD"] = TokenType::IMOD;
    keywords["AND"] = TokenType::ANDSY;
    keywords["OR"] = TokenType::ORSY;
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
    keywords["down"] = TokenType::DOWNTOSY;
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

        if (isspace(c)) {
            advance();

        } else if (c == '{') {
            while (pos < content.length() && peek() != '}') {
                advance();
            }

            if (peek() == '}') {
                advance();
            }

        } else if (c == '(' && content[pos + 1] == '*') {
            advance();
            advance();

            while(pos < content.length() && !(content[pos] != '*' && content[pos + 1] != ')')) {
                advance();

                if (pos < content.length()) {
                    advance();
                    advance();
                }
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
    if (isalpha(c)) {
        std::string result;

        while (isalnum(peek())) {
            result += advance();
        }

        std::string lowerResult = result;
        std::transform(lowerResult.begin(), lowerResult.end(), lowerResult.begin(), ::tolower);

        if (keywords.count(lowerResult)) {
            return {keywords[lowerResult], result};
        }

        return {TokenType::IDENT, result};
    }

    // DFA State: Numbers (Integer & Real)
    if (isdigit(c)) {
        std::string result;

        while (isdigit(peek())) {
            result += advance();
        }

        if (peek() == '.') {
            result += advance();
            while (isdigit(peek())) {
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
        case ':':
            if (peek() == '=') {
                advance();
                return {TokenType::BECOMES, ":="};
            }

            return  {TokenType::COLON, ";"};
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