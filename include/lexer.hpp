#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <map>

// Daftat kategori token sesuai spesifikasi Arion
enum class TokenType {
    INTCON, REALCON, CHARCON, STRING,
    NOTSY, PLUS, MINUS, TIMES, IDIV, RDIV, IMOD, ANDSY, ORSY,
    EQL, NEQ, GTR, GEQ, LSS, LEQ, 
    LPARENT, RPARENT, LBRACK, RBRACK,
    COMMA, SEMICOLON, PERIOD, COLON, BECOMES,
    CONSTSY, TYPESY, VARSY, FUNCTIONSY, PROCEDURESY, ARRAYSY, RECORDSY, PROGRAMSY,
    IDENT, BEGINSY, IFSY, CASESY, REPEATSY, WHILESY, FORSY, ENDSY, ELSESY, UNTILSY,
    OFSY, DOSY, TOSY, DOWNTOSY, THENSY,
    COMMENT, UNKNOWN, END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

class Lexer {
public:
    Lexer(const std::string& source);
    Token getNextToken();

private:
    std::string content;
    size_t pos;
    int line;
    int column;
    std::map<std::string, TokenType> keywords;

    void initKeywords();

    char peek() const;
    char peekNext() const;
    char advance();

    bool isSeparator(char c) const;
    bool isTokenBoundary(char c) const;
    void skipSeparators();

    Token makeToken(TokenType type, const std::string& value, int startLine, int startColumn) const;

    Token readBraceComment(int startLine, int startColumn);
    Token readParenStarComment(int startLine, int startColumn);
    Token readIdentifierOrKeyword(int startLine, int startColumn);
    Token readNumber(int startLine, int startColumn);
    Token readStringOrChar(int startLine, int startColumn);
    Token readDotStartedToken(int startLine, int startColumn);

    std::string consumeUnknownSequence();
};

#endif
