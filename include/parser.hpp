#ifndef PARSER_HPP
#define PARSER_HPP

#include "lexer.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct ParseNode {
    std::string label;
    std::optional<Token> token;
    std::vector<ParseNode> children;

    ParseNode(const std::string& label);
    ParseNode(const std::string& label, const Token& token);

    void addChild(const ParseNode& child);
};

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, const Token& token);

    const Token& getToken() const;

private:
    Token token;
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens);

    ParseNode parse();

private:
    std::vector<Token> tokens;
    size_t pos;

    const Token& current() const;
    const Token& peek(size_t offset = 1) const;

    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    bool match(TokenType type);

    Token advance();
    Token consume(TokenType type, const std::string& expected);

    ParseError error(const std::string& message) const;

    bool isDeclarationStart(TokenType type) const;
    bool isStatementStart(TokenType type) const;
    bool isConstantStart(TokenType type) const;
    bool isTypeStart(TokenType type) const;
    bool isRelationalOperator(TokenType type) const;
    bool isAdditiveOperator(TokenType type) const;
    bool isMultiplicativeOperator(TokenType type) const;

    bool isProcedureOrFunctionCallStart() const;
    bool isAssignmentStatementStart() const;

    ParseNode makeTerminalNode(const Token& token) const;
    
    ParseNode parseProgram();
    ParseNode parseProgramHeader();
    ParseNode parseDeclarationPart();

    ParseNode parseConstDeclaration();
    ParseNode parseConstant();

    ParseNode parseTypeDeclaration();
    ParseNode parseType();
    ParseNode parseArrayType();
    ParseNode parseRange();
    ParseNode parseEnumerated();
    ParseNode parseRecordType();
    ParseNode parseFieldList();
    ParseNode parseFieldPart();

    ParseNode parseVarDeclaration();
    ParseNode parseIdentifierList();

    ParseNode parseSubprogramDeclaration();
    ParseNode parseProcedureDeclaration();
    ParseNode parseFunctionDeclaration();
    ParseNode parseBlock();
    ParseNode parseFormalParameterList();
    ParseNode parseParameterGroup();

    ParseNode parseCompoundStatement();
    ParseNode parseStatementList();
    ParseNode parseStatement();

    ParseNode parseVariable();
    ParseNode parseComponentVariable(ParseNode baseVariable);
    ParseNode parseIndexList();

    ParseNode parseAssignmentStatement();
    ParseNode parseIfStatement();
    ParseNode parseCaseStatement();
    ParseNode parseCaseBlock();
    ParseNode parseWhileStatement();
    ParseNode parseRepeatStatement();
    ParseNode parseForStatement();

    ParseNode parseProcedureOrFunctionCall();
    ParseNode parseParameterList();

    ParseNode parseExpression();
    ParseNode parseSimpleExpression();
    ParseNode parseTerm();
    ParseNode parseFactor();

    ParseNode parseRelationalOperator();
    ParseNode parseAdditiveOperator();
    ParseNode parseMultiplicativeOperator();
};

#endif