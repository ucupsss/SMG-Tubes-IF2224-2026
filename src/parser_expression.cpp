#include "parser.hpp"

ParseNode Parser::parseExpression() {
    ParseNode node("<expression>");

    node.addChild(parseSimpleExpression());

    if (isRelationalOperator(current().type)) {
        node.addChild(parseRelationalOperator());
        node.addChild(parseSimpleExpression());
    }

    return node;
}

ParseNode Parser::parseSimpleExpression() {
    ParseNode node("<simple-expression>");

    if (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        node.addChild(makeTerminalNode(advance()));
    }

    node.addChild(parseTerm());

    while (isAdditiveOperator(current().type)) {
        node.addChild(parseAdditiveOperator());
        node.addChild(parseTerm());
    }

    return node;
}

ParseNode Parser::parseTerm() {
    ParseNode node("<term>");

    node.addChild(parseFactor());

    while (isMultiplicativeOperator(current().type)) {
        node.addChild(parseMultiplicativeOperator());
        node.addChild(parseFactor());
    }

    return node;
}

ParseNode Parser::parseFactor() {
    ParseNode node("<factor>");

    if (check(TokenType::IDENT)) {
        if (checkNext(TokenType::LPARENT)) {
            node.addChild(parseProcedureOrFunctionCall());
        } else {
            node.addChild(parseVariable());
        }

        return node;
    }

    if (check(TokenType::INTCON) ||
        check(TokenType::REALCON) ||
        check(TokenType::CHARCON) ||
        check(TokenType::STRING)) {
        node.addChild(makeTerminalNode(advance()));
        return node;
    }

    if (match(TokenType::LPARENT)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseExpression());
        node.addChild(makeTerminalNode(consume(TokenType::RPARENT, "rparent")));
        return node;
    }

    if (match(TokenType::NOTSY)) {
        node.addChild(makeTerminalNode(tokens[pos - 1]));
        node.addChild(parseFactor());
        return node;
    }

    throw error("expected factor");
}

ParseNode Parser::parseRelationalOperator() {
    ParseNode node("<relational-operator>");

    if (!isRelationalOperator(current().type)) {
        throw error("expected relational-operator");
    }

    node.addChild(makeTerminalNode(advance()));
    return node;
}

ParseNode Parser::parseAdditiveOperator() {
    ParseNode node("<additive-operator>");

    if (!isAdditiveOperator(current().type)) {
        throw error("expected additive-operator");
    }

    node.addChild(makeTerminalNode(advance()));
    return node;
}

ParseNode Parser::parseMultiplicativeOperator() {
    ParseNode node("<multiplicative-operator>");

    if (!isMultiplicativeOperator(current().type)) {
        throw error("expected multiplicative-operator");
    }

    node.addChild(makeTerminalNode(advance()));
    return node;
}
