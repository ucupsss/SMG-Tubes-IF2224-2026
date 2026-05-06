#include "parser.hpp"

namespace {

bool isSignedConstantBody(TokenType type) {
    return type == TokenType::IDENT ||
           type == TokenType::INTCON ||
           type == TokenType::REALCON;
}

bool isUnsignedConstantBody(TokenType type) {
    return isSignedConstantBody(type) ||
           type == TokenType::CHARCON ||
           type == TokenType::STRING;
}

}

ParseNode Parser::parseConstDeclaration() {
    ParseNode node("<const-declaration>");

    node.addChild(makeTerminalNode(consume(TokenType::CONSTSY, "constsy")));

    if (!check(TokenType::IDENT)) {
        throw error("expected at least one constant declaration");
    }

    do {
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
        node.addChild(makeTerminalNode(consume(TokenType::EQL, "eql")));
        node.addChild(parseConstant());
        node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));
    } while (check(TokenType::IDENT));

    return node;
}

ParseNode Parser::parseConstant() {
    ParseNode node("<constant>");

    if (check(TokenType::CHARCON) || check(TokenType::STRING)) {
        node.addChild(makeTerminalNode(advance()));
        return node;
    }

    if (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        node.addChild(makeTerminalNode(advance()));
    }

    if (isSignedConstantBody(current().type)) {
        node.addChild(makeTerminalNode(advance()));
        return node;
    }

    throw error("expected constant");
}

ParseNode Parser::parseTypeDeclaration() {
    ParseNode node("<type-declaration>");

    node.addChild(makeTerminalNode(consume(TokenType::TYPESY, "typesy")));

    if (!check(TokenType::IDENT)) {
        throw error("expected at least one type declaration");
    }

    do {
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
        node.addChild(makeTerminalNode(consume(TokenType::EQL, "eql")));
        node.addChild(parseType());
        node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));
    } while (check(TokenType::IDENT));

    return node;
}

ParseNode Parser::parseType() {
    ParseNode node("<type>");

    auto tokenAt = [this](size_t offset) -> const Token& {
        return offset == 0 ? current() : peek(offset);
    };

    auto looksLikeRange = [&tokenAt]() {
        size_t offset = 0;

        if (tokenAt(offset).type == TokenType::PLUS ||
            tokenAt(offset).type == TokenType::MINUS) {
            offset++;

            if (!isSignedConstantBody(tokenAt(offset).type)) {
                return false;
            }
        } else if (!isUnsignedConstantBody(tokenAt(offset).type)) {
            return false;
        }

        offset++;
        return tokenAt(offset).type == TokenType::PERIOD &&
               tokenAt(offset + 1).type == TokenType::PERIOD;
    };

    if (check(TokenType::ARRAYSY)) {
        node.addChild(parseArrayType());
    } else if (check(TokenType::LPARENT)) {
        node.addChild(parseEnumerated());
    } else if (check(TokenType::RECORDSY)) {
        node.addChild(parseRecordType());
    } else if (looksLikeRange()) {
        node.addChild(parseRange());
    } else if (check(TokenType::IDENT)) {
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
    } else {
        throw error("expected type");
    }

    return node;
}

ParseNode Parser::parseArrayType() {
    ParseNode node("<array-type>");

    auto tokenAt = [this](size_t offset) -> const Token& {
        return offset == 0 ? current() : peek(offset);
    };

    auto looksLikeRange = [&tokenAt]() {
        size_t offset = 0;

        if (tokenAt(offset).type == TokenType::PLUS ||
            tokenAt(offset).type == TokenType::MINUS) {
            offset++;

            if (!isSignedConstantBody(tokenAt(offset).type)) {
                return false;
            }
        } else if (!isUnsignedConstantBody(tokenAt(offset).type)) {
            return false;
        }

        offset++;
        return tokenAt(offset).type == TokenType::PERIOD &&
               tokenAt(offset + 1).type == TokenType::PERIOD;
    };

    node.addChild(makeTerminalNode(consume(TokenType::ARRAYSY, "arraysy")));
    node.addChild(makeTerminalNode(consume(TokenType::LBRACK, "lbrack")));

    if (looksLikeRange()) {
        node.addChild(parseRange());
    } else {
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident or range")));
    }

    node.addChild(makeTerminalNode(consume(TokenType::RBRACK, "rbrack")));
    node.addChild(makeTerminalNode(consume(TokenType::OFSY, "ofsy")));
    node.addChild(parseType());

    return node;
}

ParseNode Parser::parseRange() {
    ParseNode node("<range>");

    node.addChild(parseConstant());
    node.addChild(makeTerminalNode(consume(TokenType::PERIOD, "period")));
    node.addChild(makeTerminalNode(consume(TokenType::PERIOD, "period")));
    node.addChild(parseConstant());

    return node;
}

ParseNode Parser::parseEnumerated() {
    ParseNode node("<enumerated>");

    node.addChild(makeTerminalNode(consume(TokenType::LPARENT, "lparent")));
    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));

    while (match(TokenType::COMMA)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
    }

    node.addChild(makeTerminalNode(consume(TokenType::RPARENT, "rparent")));

    return node;
}

ParseNode Parser::parseRecordType() {
    ParseNode node("<record-type>");

    node.addChild(makeTerminalNode(consume(TokenType::RECORDSY, "recordsy")));
    node.addChild(parseFieldList());
    node.addChild(makeTerminalNode(consume(TokenType::ENDSY, "endsy")));

    return node;
}

ParseNode Parser::parseFieldList() {
    ParseNode node("<field-list>");

    node.addChild(parseFieldPart());

    while (match(TokenType::SEMICOLON)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));

        if (!check(TokenType::IDENT)) {
            break;
        }

        node.addChild(parseFieldPart());
    }

    return node;
}

ParseNode Parser::parseFieldPart() {
    ParseNode node("<field-part>");

    node.addChild(parseIdentifierList());
    node.addChild(makeTerminalNode(consume(TokenType::COLON, "colon")));
    node.addChild(parseType());

    return node;
}

ParseNode Parser::parseVarDeclaration() {
    ParseNode node("<var-declaration>");

    node.addChild(makeTerminalNode(consume(TokenType::VARSY, "varsy")));

    if (!check(TokenType::IDENT)) {
        throw error("expected at least one variable declaration");
    }

    do {
        node.addChild(parseIdentifierList());
        node.addChild(makeTerminalNode(consume(TokenType::COLON, "colon")));
        node.addChild(parseType());
        node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));
    } while (check(TokenType::IDENT));

    return node;
}

ParseNode Parser::parseIdentifierList() {
    ParseNode node("<identifier-list>");

    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));

    while (match(TokenType::COMMA)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
    }

    return node;
}

ParseNode Parser::parseSubprogramDeclaration() {
    ParseNode node("<subprogram-declaration>");

    if (check(TokenType::PROCEDURESY)) {
        node.addChild(parseProcedureDeclaration());
    } else if (check(TokenType::FUNCTIONSY)) {
        node.addChild(parseFunctionDeclaration());
    } else {
        throw error("expected procedure or function declaration");
    }

    return node;
}

ParseNode Parser::parseProcedureDeclaration() {
    ParseNode node("<procedure-declaration>");

    node.addChild(makeTerminalNode(consume(TokenType::PROCEDURESY, "proceduresy")));
    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));

    if (check(TokenType::LPARENT)) {
        node.addChild(parseFormalParameterList());
    }

    node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));
    node.addChild(parseBlock());
    node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));

    return node;
}

ParseNode Parser::parseFunctionDeclaration() {
    ParseNode node("<function-declaration>");

    node.addChild(makeTerminalNode(consume(TokenType::FUNCTIONSY, "functionsy")));
    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));

    if (check(TokenType::LPARENT)) {
        node.addChild(parseFormalParameterList());
    }

    node.addChild(makeTerminalNode(consume(TokenType::COLON, "colon")));
    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
    node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));
    node.addChild(parseBlock());
    node.addChild(makeTerminalNode(consume(TokenType::SEMICOLON, "semicolon")));

    return node;
}

ParseNode Parser::parseBlock() {
    ParseNode node("<block>");

    node.addChild(parseDeclarationPart());
    node.addChild(parseCompoundStatement());

    return node;
}

ParseNode Parser::parseFormalParameterList() {
    ParseNode node("<formal-parameter-list>");

    node.addChild(makeTerminalNode(consume(TokenType::LPARENT, "lparent")));
    node.addChild(parseParameterGroup());

    while (match(TokenType::SEMICOLON)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseParameterGroup());
    }

    node.addChild(makeTerminalNode(consume(TokenType::RPARENT, "rparent")));

    return node;
}

ParseNode Parser::parseParameterGroup() {
    ParseNode node("<parameter-group>");

    node.addChild(parseIdentifierList());
    node.addChild(makeTerminalNode(consume(TokenType::COLON, "colon")));

    if (check(TokenType::ARRAYSY)) {
        node.addChild(parseArrayType());
    } else {
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident or array-type")));
    }

    return node;
}
