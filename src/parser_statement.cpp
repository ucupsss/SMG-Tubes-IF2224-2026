#include "parser.hpp"

namespace {

bool isIndexAtom(TokenType type) {
    return type == TokenType::INTCON ||
           type == TokenType::CHARCON ||
           type == TokenType::IDENT;
}

}

ParseNode Parser::parseCompoundStatement() {
    ParseNode node("<compound-statement>");

    node.addChild(makeTerminalNode(consume(TokenType::BEGINSY, "beginsy")));
    node.addChild(parseStatementList());
    node.addChild(makeTerminalNode(consume(TokenType::ENDSY, "endsy")));

    return node;
}

ParseNode Parser::parseStatementList() {
    ParseNode node("<statement-list>");

    node.addChild(parseStatement());

    while (match(TokenType::SEMICOLON)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseStatement());
    }

    return node;
}

ParseNode Parser::parseStatement() {
    if (isAssignmentStatementStart()) {
        return parseAssignmentStatement();
    }

    if (check(TokenType::IDENT)) {
        return parseProcedureOrFunctionCall();
    }

    if (check(TokenType::IFSY)) {
        return parseIfStatement();
    }

    if (check(TokenType::CASESY)) {
        return parseCaseStatement();
    }

    if (check(TokenType::WHILESY)) {
        return parseWhileStatement();
    }

    if (check(TokenType::REPEATSY)) {
        return parseRepeatStatement();
    }

    if (check(TokenType::FORSY)) {
        return parseForStatement();
    }

    if (check(TokenType::BEGINSY)) {
        return parseCompoundStatement();
    }

    return ParseNode("<statement>");
}

ParseNode Parser::parseVariable() {
    ParseNode node("<variable>");

    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));

    while (check(TokenType::LBRACK) || check(TokenType::PERIOD)) {
        node.addChild(parseComponentVariable(node));
    }

    return node;
}

ParseNode Parser::parseComponentVariable(ParseNode baseVariable) {
    (void)baseVariable;

    ParseNode node("<component-variable>");

    if (check(TokenType::LBRACK)) {
        node.addChild(makeTerminalNode(consume(TokenType::LBRACK, "lbrack")));
        node.addChild(parseIndexList());
        node.addChild(makeTerminalNode(consume(TokenType::RBRACK, "rbrack")));
        return node;
    }

    if (check(TokenType::PERIOD)) {
        node.addChild(makeTerminalNode(consume(TokenType::PERIOD, "period")));
        node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
        return node;
    }

    throw error("expected component-variable");
}

ParseNode Parser::parseIndexList() {
    ParseNode node("<index-list>");

    if (!isIndexAtom(current().type)) {
        throw error("expected intcon, charcon, or ident in index-list");
    }

    node.addChild(makeTerminalNode(advance()));

    while (match(TokenType::COMMA)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));

        if (!isIndexAtom(current().type)) {
            throw error("expected intcon, charcon, or ident after comma in index-list");
        }

        node.addChild(makeTerminalNode(advance()));
    }

    return node;
}

ParseNode Parser::parseAssignmentStatement() {
    ParseNode node("<assignment-statement>");

    node.addChild(parseVariable());
    node.addChild(makeTerminalNode(consume(TokenType::BECOMES, "becomes")));
    node.addChild(parseExpression());

    return node;
}

ParseNode Parser::parseIfStatement() {
    ParseNode node("<if-statement>");

    node.addChild(makeTerminalNode(consume(TokenType::IFSY, "ifsy")));
    node.addChild(parseExpression());
    node.addChild(makeTerminalNode(consume(TokenType::THENSY, "thensy")));
    node.addChild(parseStatement());

    if (match(TokenType::ELSESY)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseStatement());
    }

    return node;
}

ParseNode Parser::parseCaseStatement() {
    ParseNode node("<case-statement>");

    node.addChild(makeTerminalNode(consume(TokenType::CASESY, "casesy")));
    node.addChild(parseExpression());
    node.addChild(makeTerminalNode(consume(TokenType::OFSY, "ofsy")));
    node.addChild(parseCaseBlock());
    node.addChild(makeTerminalNode(consume(TokenType::ENDSY, "endsy")));

    return node;
}

ParseNode Parser::parseCaseBlock() {
    ParseNode node("<case-block>");

    node.addChild(parseConstant());

    while (match(TokenType::COMMA)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseConstant());
    }

    node.addChild(makeTerminalNode(consume(TokenType::COLON, "colon")));
    node.addChild(parseStatement());

    if (match(TokenType::SEMICOLON)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));

        if (isConstantStart(current().type)) {
            node.addChild(parseCaseBlock());
        }
    }

    return node;
}

ParseNode Parser::parseWhileStatement() {
    ParseNode node("<while-statement>");

    node.addChild(makeTerminalNode(consume(TokenType::WHILESY, "whilesy")));
    node.addChild(parseExpression());
    node.addChild(makeTerminalNode(consume(TokenType::DOSY, "dosy")));
    node.addChild(parseStatement());

    return node;
}

ParseNode Parser::parseRepeatStatement() {
    ParseNode node("<repeat-statement>");

    node.addChild(makeTerminalNode(consume(TokenType::REPEATSY, "repeatsy")));
    node.addChild(parseStatementList());
    node.addChild(makeTerminalNode(consume(TokenType::UNTILSY, "untilsy")));
    node.addChild(parseExpression());

    return node;
}

ParseNode Parser::parseForStatement() {
    ParseNode node("<for-statement>");

    node.addChild(makeTerminalNode(consume(TokenType::FORSY, "forsy")));
    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));
    node.addChild(makeTerminalNode(consume(TokenType::BECOMES, "becomes")));
    node.addChild(parseExpression());

    if (check(TokenType::TOSY) || check(TokenType::DOWNTOSY)) {
        node.addChild(makeTerminalNode(advance()));
    } else {
        throw error("expected tosy or downtosy");
    }

    node.addChild(parseExpression());
    node.addChild(makeTerminalNode(consume(TokenType::DOSY, "dosy")));
    node.addChild(parseStatement());

    return node;
}

ParseNode Parser::parseProcedureOrFunctionCall() {
    ParseNode node("<procedure/function-call>");

    node.addChild(makeTerminalNode(consume(TokenType::IDENT, "ident")));

    if (match(TokenType::LPARENT)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));

        if (!check(TokenType::RPARENT)) {
            node.addChild(parseParameterList());
        }

        node.addChild(makeTerminalNode(consume(TokenType::RPARENT, "rparent")));
    }

    return node;
}

ParseNode Parser::parseParameterList() {
    ParseNode node("<parameter-list>");

    node.addChild(parseExpression());

    while (match(TokenType::COMMA)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseExpression());
    }

    return node;
}
