#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>
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
    UNKNOWN, END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
};

class Lexer {
    public:
        Lexer(const std::string& source);
        Token getNextToken();

    private:
        std::string content;
        size_t pos;
        std::map<std::string, TokenType> keywords;
        bool hasPendingToken;
        Token pendingToken;

        void initKeywords();
        char peek();
        char advance();
        void skipWhiteSpaceAndComments();
};

#endif
