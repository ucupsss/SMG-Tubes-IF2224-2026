#include "parser.hpp"

#include "formatter.hpp"

#include <sstream>

namespace {

std::string describeToken(const Token& token) {
    std::string result = tokenTypeToString(token.type);

    if (!token.value.empty()) {
        result += " (" + token.value + ")";
    }

    return result;
}

}

ParseNode::ParseNode(const std::string& label)
    : label(label),
      token(std::nullopt),
      children() {}

ParseNode::ParseNode(const std::string& label, const Token& token)
    : label(label),
      token(token),
      children() {}

void ParseNode::addChild(const ParseNode& child) {
    children.push_back(child);
}

ParseError::ParseError(const std::string& message, const Token& token)
    : std::runtime_error(message),
      token(token) {}

const Token& ParseError::getToken() const {
    return token;
}

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens),
      pos(0) {
    if (this->tokens.empty() || this->tokens.back().type != TokenType::END_OF_FILE) {
        this->tokens.push_back({TokenType::END_OF_FILE, "", -1, -1});
    }
}

ParseNode Parser::parse() {
    ParseNode root = parseProgram();
    consume(TokenType::END_OF_FILE, "end of file");
    return root;
}

const Token& Parser::current() const {
    if (pos >= tokens.size()) {
        return tokens.back();
    }

    return tokens[pos];
}

const Token& Parser::peek(size_t offset) const {
    const size_t target = pos + offset;
    if (target >= tokens.size()) {
        return tokens.back();
    }

    return tokens[target];
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::checkNext(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

Token Parser::advance() {
    Token token = current();

    if (!isAtEnd()) {
        pos++;
    }

    return token;
}

Token Parser::consume(TokenType type, const std::string& expected) {
    if (check(type)) {
        return advance();
    }

    std::ostringstream message;
    message << "Syntax error at line " << current().line
            << ", column " << current().column
            << ": unexpected token " << describeToken(current())
            << ", expected " << expected;

    throw ParseError(message.str(), current());
}

ParseError Parser::error(const std::string& message) const {
    std::ostringstream fullMessage;
    fullMessage << "Syntax error at line " << current().line
                << ", column " << current().column
                << ": " << message
                << ", found " << describeToken(current());

    return ParseError(fullMessage.str(), current());
}

bool Parser::isDeclarationStart(TokenType type) const {
    return type == TokenType::CONSTSY ||
           type == TokenType::TYPESY ||
           type == TokenType::VARSY ||
           type == TokenType::PROCEDURESY ||
           type == TokenType::FUNCTIONSY;
}

bool Parser::isStatementStart(TokenType type) const {
    return type == TokenType::IDENT ||
           type == TokenType::IFSY ||
           type == TokenType::CASESY ||
           type == TokenType::WHILESY ||
           type == TokenType::REPEATSY ||
           type == TokenType::FORSY;
}

bool Parser::isConstantStart(TokenType type) const {
    return type == TokenType::CHARCON ||
           type == TokenType::STRING ||
           type == TokenType::PLUS ||
           type == TokenType::MINUS ||
           type == TokenType::IDENT ||
           type == TokenType::INTCON ||
           type == TokenType::REALCON;
}

bool Parser::isTypeStart(TokenType type) const {
    return type == TokenType::IDENT ||
           type == TokenType::ARRAYSY ||
           type == TokenType::LPARENT ||
           type == TokenType::RECORDSY ||
           isConstantStart(type);
}

bool Parser::isRelationalOperator(TokenType type) const {
    return type == TokenType::EQL ||
           type == TokenType::NEQ ||
           type == TokenType::GTR ||
           type == TokenType::GEQ ||
           type == TokenType::LSS ||
           type == TokenType::LEQ;
}

bool Parser::isAdditiveOperator(TokenType type) const {
    return type == TokenType::PLUS ||
           type == TokenType::MINUS ||
           type == TokenType::ORSY;
}

bool Parser::isMultiplicativeOperator(TokenType type) const {
    return type == TokenType::TIMES ||
           type == TokenType::RDIV ||
           type == TokenType::IDIV ||
           type == TokenType::IMOD ||
           type == TokenType::ANDSY;
}

bool Parser::isProcedureOrFunctionCallStart() const {
    return check(TokenType::IDENT);
}

bool Parser::isAssignmentStatementStart() const {
    if (!check(TokenType::IDENT)) {
        return false;
    }

    size_t offset = 1;

    while (true) {
        if (peek(offset).type == TokenType::LBRACK) {
            int bracketDepth = 1;
            offset++;

            while (bracketDepth > 0 && peek(offset).type != TokenType::END_OF_FILE) {
                if (peek(offset).type == TokenType::LBRACK) {
                    bracketDepth++;
                } else if (peek(offset).type == TokenType::RBRACK) {
                    bracketDepth--;
                }

                offset++;
            }

            continue;
        }

        if (peek(offset).type == TokenType::PERIOD &&
            peek(offset + 1).type == TokenType::IDENT) {
            offset += 2;
            continue;
        }

        break;
    }

    return peek(offset).type == TokenType::BECOMES;
}

ParseNode Parser::makeTerminalNode(const Token& token) const {
    return ParseNode(formatToken(token), token);
}

ParseNode Parser::parseProgram() {
    ParseNode node("<program>");

    node.addChild(parseProgramHeader());
    node.addChild(parseDeclarationPart());
    node.addChild(parseCompoundStatement());
    node.addChild(makeTerminalNode(consume(TokenType::PERIOD, "period")));

    return node;
}

ParseNode Parser::parseProgramHeader() {
    ParseNode node("<program-header>");

    node.addChild(makeTerminalNode(consume(TokenType::PROGRAMSY, "programsy")));
    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
    node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));

    return node;
}

ParseNode Parser::parseDeclarationPart() {
    ParseNode node("<declaration-part>");

    while (isDeclarationStart(current().type)) {
        if (check(TokenType::CONSTSY)) {
            node.addChild(parseConstDeclaration());
        } else if (check(TokenType::TYPESY)) {
            node.addChild(parseTypeDeclaration());
        } else if (check(TokenType::VARSY)) {
            node.addChild(parseVarDeclaration());
        } else if (check(TokenType::PROCEDURESY) || check(TokenType::FUNCTIONSY)) {
            node.addChild(parseSubprogramDeclaration());
        } else {
            throw error("expected declaration");
        }
    }

    return node;
}
