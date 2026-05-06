#include "formatter.hpp"

#include <vector>

namespace {

bool needsValue(TokenType type) {
    return type == TokenType::IDENT ||
           type == TokenType::INTCON ||
           type == TokenType::REALCON ||
           type == TokenType::CHARCON ||
           type == TokenType::STRING ||
           type == TokenType::COMMENT ||
           type == TokenType::UNKNOWN;
}

bool shouldSkipNode(const ParseNode& node) {
    return node.label == "<statement>" && node.children.empty();
}

bool shouldFlattenNode(const ParseNode& node) {
    return node.label == "<variable>" ||
           node.label == "<additive-operator>";
}

std::vector<ParseNode> normalizeParseTreeNode(const ParseNode& node) {
    std::vector<ParseNode> normalizedChildren;

    for (const ParseNode& child : node.children) {
        std::vector<ParseNode> normalizedChildNodes = normalizeParseTreeNode(child);
        normalizedChildren.insert(
            normalizedChildren.end(),
            normalizedChildNodes.begin(),
            normalizedChildNodes.end()
        );
    }

    if (shouldSkipNode(node)) {
        return {};
    }

    if (shouldFlattenNode(node)) {
        return normalizedChildren;
    }

    ParseNode normalizedNode = node;
    normalizedNode.children = std::move(normalizedChildren);
    return {normalizedNode};
}

void formatParseTreeNode(
    const ParseNode& node,
    const std::string& prefix,
    bool isLast,
    bool isRoot,
    std::vector<std::string>& lines
) {
    if (isRoot) {
        lines.push_back(node.label);
    } else {
        lines.push_back(prefix + (isLast ? "└── " : "├── ") + node.label);
    }

    const std::string childPrefix = isRoot
        ? ""
        : prefix + (isLast ? "    " : "│   ");

    for (size_t i = 0; i < node.children.size(); ++i) {
        formatParseTreeNode(
            node.children[i],
            childPrefix,
            i + 1 == node.children.size(),
            false,
            lines
        );
    }
}

}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INTCON: return "intcon";
        case TokenType::REALCON: return "realcon";
        case TokenType::CHARCON: return "charcon";
        case TokenType::STRING: return "string";
        case TokenType::NOTSY: return "notsy";
        case TokenType::PLUS: return "plus";
        case TokenType::MINUS: return "minus";
        case TokenType::TIMES: return "times";
        case TokenType::IDIV: return "idiv";
        case TokenType::RDIV: return "rdiv";
        case TokenType::IMOD: return "imod";
        case TokenType::ANDSY: return "andsy";
        case TokenType::ORSY: return "orsy";
        case TokenType::EQL: return "eql";
        case TokenType::NEQ: return "neq";
        case TokenType::GTR: return "gtr";
        case TokenType::GEQ: return "geq";
        case TokenType::LSS: return "lss";
        case TokenType::LEQ: return "leq";
        case TokenType::LPARENT: return "lparent";
        case TokenType::RPARENT: return "rparent";
        case TokenType::LBRACK: return "lbrack";
        case TokenType::RBRACK: return "rbrack";
        case TokenType::COMMA: return "comma";
        case TokenType::SEMICOLON: return "semicolon";
        case TokenType::PERIOD: return "period";
        case TokenType::COLON: return "colon";
        case TokenType::BECOMES: return "becomes";
        case TokenType::CONSTSY: return "constsy";
        case TokenType::TYPESY: return "typesy";
        case TokenType::VARSY: return "varsy";
        case TokenType::FUNCTIONSY: return "functionsy";
        case TokenType::PROCEDURESY: return "proceduresy";
        case TokenType::ARRAYSY: return "arraysy";
        case TokenType::RECORDSY: return "recordsy";
        case TokenType::PROGRAMSY: return "programsy";
        case TokenType::IDENT: return "ident";
        case TokenType::BEGINSY: return "beginsy";
        case TokenType::IFSY: return "ifsy";
        case TokenType::CASESY: return "casesy";
        case TokenType::REPEATSY: return "repeatsy";
        case TokenType::WHILESY: return "whilesy";
        case TokenType::FORSY: return "forsy";
        case TokenType::ENDSY: return "endsy";
        case TokenType::ELSESY: return "elsesy";
        case TokenType::UNTILSY: return "untilsy";
        case TokenType::OFSY: return "ofsy";
        case TokenType::DOSY: return "dosy";
        case TokenType::TOSY: return "tosy";
        case TokenType::DOWNTOSY: return "downtosy";
        case TokenType::THENSY: return "thensy";
        case TokenType::COMMENT: return "comment";
        case TokenType::UNKNOWN: return "unknown";
        case TokenType::END_OF_FILE: return "eof";
        default: return "unknown";
    }
}

std::string formatToken(const Token& token) {
    std::string result = tokenTypeToString(token.type);

    if (needsValue(token.type)) {
        if (token.type == TokenType::STRING || token.type == TokenType::CHARCON) {
            result += "('" + token.value + "')";
        } else {
            result += "(" + token.value + ")";
        }
    }

    return result;
}

std::vector<std::string> formatParseTree(const ParseNode& root) {
    std::vector<ParseNode> normalizedRoots = normalizeParseTreeNode(root);
    std::vector<std::string> lines;

    if (!normalizedRoots.empty()) {
        formatParseTreeNode(normalizedRoots.front(), "", true, true, lines);
    }

    return lines;
}

bool isLexerWarning(const Token& token) {
    return token.type == TokenType::UNKNOWN &&
           (token.value == "string tidak ditutup sebelum akhir file" ||
            token.value == "komentar tidak ditutup sebelum akhir file");
}
