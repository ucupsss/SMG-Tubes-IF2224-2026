#include "ast.hpp"
#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::string indent(int n) {
    return std::string(static_cast<size_t>(std::max(0, n)), ' ');
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string join(const std::vector<std::string>& values, const std::string& sep = ", ") {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << sep;
        out << values[i];
    }
    return out.str();
}

std::string ann(const ASTNode& node) {
    std::ostringstream out;
    out << " [type:" << node.inferredType
        << ", tab:" << node.tabIndex
        << ", lev:" << node.lexLevel << "]";
    return out.str();
}

void appendNode(std::string& text, const std::string& label, const ASTNode* node, int i) {
    if (!node) return;
    text += "\n" + indent(i) + label + ":";
    text += "\n" + node->toString(i + 2);
}

void appendSection(std::string& text, const std::string& label, int i) {
    text += "\n" + indent(i) + label + ":";
}

SourceLocation locOf(const ParseNode& node) {
    if (node.token.has_value()) {
        return {node.token->line, node.token->column};
    }

    for (const ParseNode& child : node.children) {
        SourceLocation loc = locOf(child);
        if (loc.line != -1) return loc;
    }

    return {};
}

bool isToken(const ParseNode& node, TokenType type) {
    return node.token.has_value() && node.token->type == type;
}

std::string tokenValue(const ParseNode& node) {
    return node.token.has_value() ? node.token->value : "";
}

const ParseNode* firstChildLabel(const ParseNode& node, const std::string& label) {
    for (const ParseNode& child : node.children) {
        if (child.label == label) return &child;
    }
    return nullptr;
}

const ParseNode* firstToken(const ParseNode& node, TokenType type) {
    for (const ParseNode& child : node.children) {
        if (isToken(child, type)) return &child;
    }
    return nullptr;
}

std::vector<const ParseNode*> childrenLabel(const ParseNode& node, const std::string& label) {
    std::vector<const ParseNode*> result;
    for (const ParseNode& child : node.children) {
        if (child.label == label) result.push_back(&child);
    }
    return result;
}

std::vector<std::string> identifierList(const ParseNode& node) {
    std::vector<std::string> ids;
    for (const ParseNode& child : node.children) {
        if (isToken(child, TokenType::IDENT)) ids.push_back(child.token->value);
    }
    return ids;
}

std::string opFrom(const ParseNode& node) {
    if (node.token.has_value()) return lower(node.token->value);

    for (const ParseNode& child : node.children) {
        if (child.token.has_value()) return lower(child.token->value);
    }

    return "";
}

bool isStatementLabel(const std::string& label) {
    return label == "<statement>" ||
           label == "<assignment-statement>" ||
           label == "<if-statement>" ||
           label == "<while-statement>" ||
           label == "<for-statement>" ||
           label == "<repeat-statement>" ||
           label == "<case-statement>" ||
           label == "<procedure/function-call>";
}

std::unique_ptr<TypeNode> buildType(const ParseNode& node);
std::unique_ptr<ExpressionNode> buildExpression(const ParseNode& node);
std::unique_ptr<StatementNode> buildStatement(const ParseNode& node);
std::unique_ptr<BlockNode> buildCompoundStatement(const ParseNode& node);
std::vector<std::unique_ptr<DeclarationNode>> buildDeclarationPart(const ParseNode& node);

std::unique_ptr<ExpressionNode> literalFromToken(const ParseNode& node) {
    if (isToken(node, TokenType::INTCON)) {
        auto lit = std::make_unique<IntLiteralNode>();
        lit->value = std::stoi(node.token->value);
        lit->location = locOf(node);
        return lit;
    }

    if (isToken(node, TokenType::REALCON)) {
        auto lit = std::make_unique<RealLiteralNode>();
        lit->value = std::stod(node.token->value);
        lit->location = locOf(node);
        return lit;
    }

    if (isToken(node, TokenType::CHARCON)) {
        auto lit = std::make_unique<CharLiteralNode>();
        lit->value = node.token->value.empty() ? '\0' : node.token->value[0];
        lit->location = locOf(node);
        return lit;
    }

    if (isToken(node, TokenType::STRING)) {
        auto lit = std::make_unique<StringLiteralNode>();
        lit->value = node.token->value;
        lit->location = locOf(node);
        return lit;
    }

    if (isToken(node, TokenType::IDENT)) {
        const std::string name = node.token->value;
        const std::string lowered = lower(name);

        if (lowered == "true" || lowered == "false") {
            auto lit = std::make_unique<BoolLiteralNode>();
            lit->value = (lowered == "true");
            lit->location = locOf(node);
            return lit;
        }

        auto var = std::make_unique<VarNode>();
        var->name = name;
        var->location = locOf(node);
        return var;
    }

    throw std::runtime_error("Unsupported literal token in AST builder");
}

std::unique_ptr<ExpressionNode> buildConstant(const ParseNode& node) {
    bool neg = false;
    bool pos = false;
    const ParseNode* valueNode = nullptr;

    for (const ParseNode& child : node.children) {
        if (isToken(child, TokenType::MINUS)) {
            neg = true;
        } else if (isToken(child, TokenType::PLUS)) {
            pos = true;
        } else if (child.token.has_value()) {
            valueNode = &child;
        }
    }

    if (!valueNode) throw std::runtime_error("Invalid <constant> node");

    auto result = literalFromToken(*valueNode);

    if (neg) {
        if (auto* lit = dynamic_cast<IntLiteralNode*>(result.get())) {
            lit->value = -lit->value;
            return result;
        }
        if (auto* lit = dynamic_cast<RealLiteralNode*>(result.get())) {
            lit->value = -lit->value;
            return result;
        }
    }

    if ((neg || pos) && result->kind != ASTNodeKind::IntLiteral && result->kind != ASTNodeKind::RealLiteral) {
        auto unary = std::make_unique<UnaryOpNode>();
        unary->op = neg ? "-" : "+";
        unary->location = locOf(node);
        unary->operand = std::move(result);
        return unary;
    }

    return result;
}

std::unique_ptr<TypeNode> namedTypeFromToken(const ParseNode& node) {
    auto type = std::make_unique<NamedTypeNode>();
    type->name = tokenValue(node);
    type->typeName = type->name;
    type->isNamed = true;
    type->location = locOf(node);
    return type;
}

std::unique_ptr<TypeNode> buildRange(const ParseNode& node) {
    auto range = std::make_unique<SubrangeTypeNode>();
    auto constants = childrenLabel(node, "<constant>");
    if (constants.size() >= 2) {
        range->lowerBound = buildConstant(*constants[0]);
        range->upperBound = buildConstant(*constants[1]);
    }
    range->location = locOf(node);
    return range;
}

std::unique_ptr<TypeNode> buildEnumerated(const ParseNode& node) {
    auto en = std::make_unique<EnumTypeNode>();
    for (const ParseNode& child : node.children) {
        if (isToken(child, TokenType::IDENT)) en->values.push_back(child.token->value);
    }
    en->location = locOf(node);
    return en;
}

std::unique_ptr<TypeNode> buildArrayType(const ParseNode& node) {
    auto arr = std::make_unique<ArrayTypeNode>();
    bool indexSet = false;

    for (const ParseNode& child : node.children) {
        if (!indexSet && child.label == "<range>") {
            arr->indexType = buildRange(child);
            indexSet = true;
        } else if (!indexSet && isToken(child, TokenType::IDENT)) {
            arr->indexType = namedTypeFromToken(child);
            indexSet = true;
        } else if (child.label == "<type>") {
            arr->elementType = buildType(child);
        }
    }

    arr->location = locOf(node);
    return arr;
}

RecordFieldNode buildFieldPart(const ParseNode& node) {
    RecordFieldNode field;
    field.location = locOf(node);

    if (const ParseNode* ids = firstChildLabel(node, "<identifier-list>")) {
        field.names = identifierList(*ids);
    }

    if (const ParseNode* type = firstChildLabel(node, "<type>")) {
        field.type = buildType(*type);
    }

    return field;
}

void collectFieldParts(const ParseNode& node, std::vector<RecordFieldNode>& fields) {
    if (node.label == "<field-part>") {
        fields.push_back(buildFieldPart(node));
        return;
    }

    for (const ParseNode& child : node.children) collectFieldParts(child, fields);
}

std::unique_ptr<TypeNode> buildRecordType(const ParseNode& node) {
    auto rec = std::make_unique<RecordTypeNode>();
    collectFieldParts(node, rec->fields);
    rec->location = locOf(node);
    return rec;
}

std::unique_ptr<TypeNode> buildType(const ParseNode& node) {
    if (node.label == "<type>") {
        for (const ParseNode& child : node.children) {
            if (!child.token.has_value() || isToken(child, TokenType::IDENT)) return buildType(child);
        }
    }

    if (node.label == "<array-type>") return buildArrayType(node);
    if (node.label == "<record-type>") return buildRecordType(node);
    if (node.label == "<range>") return buildRange(node);
    if (node.label == "<enumerated>") return buildEnumerated(node);
    if (isToken(node, TokenType::IDENT)) return namedTypeFromToken(node);

    throw std::runtime_error("Unsupported type node: " + node.label);
}

void collectIndexList(const ParseNode& node, std::vector<std::unique_ptr<ExpressionNode>>& out) {
    for (const ParseNode& child : node.children) {
        if (child.label == "<index-list>") {
            collectIndexList(child, out);
        } else if (child.token.has_value() &&
                   (child.token->type == TokenType::INTCON ||
                    child.token->type == TokenType::CHARCON ||
                    child.token->type == TokenType::IDENT)) {
            out.push_back(literalFromToken(child));
        }
    }
}

std::unique_ptr<ExpressionNode> buildVariable(const ParseNode& node) {
    const ParseNode* ident = firstToken(node, TokenType::IDENT);
    if (!ident) throw std::runtime_error("Variable without identifier");

    if (node.children.size() == 1) {
        const std::string lowered = lower(ident->token->value);
        if (lowered == "true" || lowered == "false") return literalFromToken(*ident);
    }

    auto base = std::make_unique<VarNode>();
    base->name = ident->token->value;
    base->location = locOf(*ident);
    std::unique_ptr<ExpressionNode> current = std::move(base);

    for (const ParseNode& child : node.children) {
        if (child.label != "<component-variable>") continue;

        if (firstToken(child, TokenType::LBRACK)) {
            auto access = std::make_unique<ArrayAccessNode>();
            access->array = std::move(current);
            if (const ParseNode* indices = firstChildLabel(child, "<index-list>")) {
                collectIndexList(*indices, access->indices);
            }
            access->location = locOf(child);
            current = std::move(access);
        } else if (firstToken(child, TokenType::PERIOD)) {
            auto access = std::make_unique<RecordAccessNode>();
            access->record = std::move(current);
            if (const ParseNode* field = firstToken(child, TokenType::IDENT)) {
                access->fieldName = field->token->value;
            }
            access->location = locOf(child);
            current = std::move(access);
        }
    }

    return current;
}

std::vector<std::unique_ptr<ExpressionNode>> buildArguments(const ParseNode& node) {
    std::vector<std::unique_ptr<ExpressionNode>> args;

    if (node.label == "<parameter-list>") {
        for (const ParseNode& child : node.children) {
            if (child.label == "<expression>") args.push_back(buildExpression(child));
        }
        return args;
    }

    if (const ParseNode* params = firstChildLabel(node, "<parameter-list>")) {
        return buildArguments(*params);
    }

    return args;
}

std::unique_ptr<FuncCallNode> buildFuncCall(const ParseNode& node) {
    auto call = std::make_unique<FuncCallNode>();
    if (const ParseNode* ident = firstToken(node, TokenType::IDENT)) call->name = ident->token->value;
    call->arguments = buildArguments(node);
    call->location = locOf(node);
    return call;
}

std::unique_ptr<ProcCallNode> buildProcCall(const ParseNode& node) {
    auto call = std::make_unique<ProcCallNode>();
    if (const ParseNode* ident = firstToken(node, TokenType::IDENT)) call->name = ident->token->value;
    call->arguments = buildArguments(node);
    call->location = locOf(node);
    return call;
}

std::unique_ptr<ExpressionNode> buildFactor(const ParseNode& node) {
    for (const ParseNode& child : node.children) {
        if (child.label == "<variable>") return buildVariable(child);
        if (child.label == "<procedure/function-call>") return buildFuncCall(child);
        if (child.label == "<expression>") return buildExpression(child);

        if (child.token.has_value() &&
            (child.token->type == TokenType::INTCON ||
             child.token->type == TokenType::REALCON ||
             child.token->type == TokenType::CHARCON ||
             child.token->type == TokenType::STRING ||
             child.token->type == TokenType::IDENT)) {
            return literalFromToken(child);
        }

        if (isToken(child, TokenType::NOTSY)) {
            auto unary = std::make_unique<UnaryOpNode>();
            unary->op = "not";
            unary->location = locOf(child);

            for (const ParseNode& sibling : node.children) {
                if (sibling.label == "<factor>") {
                    unary->operand = buildFactor(sibling);
                    break;
                }
            }
            return unary;
        }
    }

    throw std::runtime_error("Unsupported factor");
}

std::unique_ptr<ExpressionNode> buildTerm(const ParseNode& node) {
    std::unique_ptr<ExpressionNode> current;
    std::string pendingOp;

    for (const ParseNode& child : node.children) {
        if (child.label == "<factor>") {
            auto rhs = buildFactor(child);
            if (!current) {
                current = std::move(rhs);
            } else {
                auto bin = std::make_unique<BinOpNode>();
                bin->op = pendingOp;
                bin->left = std::move(current);
                bin->right = std::move(rhs);
                bin->location = bin->left->location;
                current = std::move(bin);
            }
        } else if (child.label == "<multiplicative-operator>") {
            pendingOp = opFrom(child);
        }
    }

    if (!current) throw std::runtime_error("Empty term");
    return current;
}

std::unique_ptr<ExpressionNode> buildSimpleExpression(const ParseNode& node) {
    std::unique_ptr<ExpressionNode> current;
    std::string pendingOp;
    std::string unaryPrefix;

    for (const ParseNode& child : node.children) {
        if (!current && (isToken(child, TokenType::PLUS) || isToken(child, TokenType::MINUS))) {
            unaryPrefix = opFrom(child);
        } else if (child.label == "<term>") {
            auto rhs = buildTerm(child);

            if (!unaryPrefix.empty() && !current) {
                auto unary = std::make_unique<UnaryOpNode>();
                unary->op = unaryPrefix;
                unary->location = rhs->location;
                unary->operand = std::move(rhs);
                rhs = std::move(unary);
            }

            if (!current) {
                current = std::move(rhs);
            } else {
                auto bin = std::make_unique<BinOpNode>();
                bin->op = pendingOp;
                bin->left = std::move(current);
                bin->right = std::move(rhs);
                bin->location = bin->left->location;
                current = std::move(bin);
            }
        } else if (child.label == "<additive-operator>") {
            pendingOp = opFrom(child);
        }
    }

    if (!current) throw std::runtime_error("Empty simple expression");
    return current;
}

std::unique_ptr<ExpressionNode> buildExpression(const ParseNode& node) {
    if (node.label != "<expression>") {
        if (node.label == "<simple-expression>") return buildSimpleExpression(node);
        if (node.label == "<term>") return buildTerm(node);
        if (node.label == "<factor>") return buildFactor(node);
    }

    std::unique_ptr<ExpressionNode> left;
    std::string op;

    for (const ParseNode& child : node.children) {
        if (child.label == "<simple-expression>") {
            if (!left) {
                left = buildSimpleExpression(child);
            } else {
                auto bin = std::make_unique<BinOpNode>();
                bin->op = op;
                bin->left = std::move(left);
                bin->right = buildSimpleExpression(child);
                bin->location = bin->left->location;
                left = std::move(bin);
            }
        } else if (child.label == "<relational-operator>") {
            op = opFrom(child);
        }
    }

    if (!left) throw std::runtime_error("Empty expression");
    return left;
}

std::unique_ptr<StatementNode> buildAssignment(const ParseNode& node) {
    auto assign = std::make_unique<AssignNode>();
    if (const ParseNode* var = firstChildLabel(node, "<variable>")) assign->target = buildVariable(*var);
    if (const ParseNode* expr = firstChildLabel(node, "<expression>")) assign->value = buildExpression(*expr);
    assign->location = locOf(node);
    return assign;
}

std::unique_ptr<StatementNode> firstStatementAfter(const ParseNode& node, size_t start) {
    for (size_t i = start; i < node.children.size(); ++i) {
        if (isStatementLabel(node.children[i].label)) return buildStatement(node.children[i]);
    }
    return nullptr;
}

std::unique_ptr<StatementNode> buildIf(const ParseNode& node) {
    auto ifNode = std::make_unique<IfNode>();
    ifNode->location = locOf(node);

    for (size_t i = 0; i < node.children.size(); ++i) {
        const ParseNode& child = node.children[i];
        if (child.label == "<expression>" && !ifNode->condition) {
            ifNode->condition = buildExpression(child);
        } else if (isToken(child, TokenType::THENSY)) {
            ifNode->thenBranch = firstStatementAfter(node, i + 1);
        } else if (isToken(child, TokenType::ELSESY)) {
            ifNode->elseBranch = firstStatementAfter(node, i + 1);
        }
    }

    return ifNode;
}

std::unique_ptr<StatementNode> buildWhile(const ParseNode& node) {
    auto whileNode = std::make_unique<WhileNode>();
    whileNode->location = locOf(node);
    if (const ParseNode* expr = firstChildLabel(node, "<expression>")) whileNode->condition = buildExpression(*expr);
    if (const ParseNode* body = firstChildLabel(node, "<compound-statement>")) whileNode->body = buildCompoundStatement(*body);
    return whileNode;
}

std::unique_ptr<StatementNode> buildFor(const ParseNode& node) {
    auto forNode = std::make_unique<ForNode>();
    forNode->location = locOf(node);

    if (const ParseNode* ident = firstToken(node, TokenType::IDENT)) forNode->controlVariable = ident->token->value;

    auto exprs = childrenLabel(node, "<expression>");
    if (exprs.size() >= 1) forNode->startValue = buildExpression(*exprs[0]);
    if (exprs.size() >= 2) forNode->endValue = buildExpression(*exprs[1]);

    for (const ParseNode& child : node.children) {
        if (isToken(child, TokenType::DOWNTOSY)) forNode->direction = ForDirection::Downto;
        else if (isToken(child, TokenType::TOSY)) forNode->direction = ForDirection::To;
    }

    if (const ParseNode* body = firstChildLabel(node, "<compound-statement>")) forNode->body = buildCompoundStatement(*body);
    return forNode;
}

void collectStatements(const ParseNode& node, std::vector<std::unique_ptr<StatementNode>>& out) {
    for (const ParseNode& child : node.children) {
        if (!isStatementLabel(child.label)) continue;
        auto stmt = buildStatement(child);
        if (stmt) out.push_back(std::move(stmt));
    }
}

std::unique_ptr<StatementNode> buildRepeat(const ParseNode& node) {
    auto repeat = std::make_unique<RepeatNode>();
    repeat->location = locOf(node);
    if (const ParseNode* list = firstChildLabel(node, "<statement-list>")) collectStatements(*list, repeat->body);
    if (const ParseNode* expr = firstChildLabel(node, "<expression>")) repeat->condition = buildExpression(*expr);
    return repeat;
}

void collectCaseBranches(const ParseNode& node, std::vector<CaseBranchNode>& branches) {
    if (node.label != "<case-block>") return;

    CaseBranchNode branch;
    branch.location = locOf(node);
    bool afterColon = false;

    for (const ParseNode& child : node.children) {
        if (child.label == "<case-block>") {
            continue;
        }

        if (!afterColon && child.label == "<constant>") {
            branch.labels.push_back(buildConstant(child));
        } else if (isToken(child, TokenType::COLON)) {
            afterColon = true;
        } else if (afterColon && !branch.statement && isStatementLabel(child.label)) {
            branch.statement = buildStatement(child);
        }
    }

    if (!branch.labels.empty() || branch.statement) branches.push_back(std::move(branch));

    for (const ParseNode& child : node.children) {
        if (child.label == "<case-block>") collectCaseBranches(child, branches);
    }
}

std::unique_ptr<StatementNode> buildCase(const ParseNode& node) {
    auto caseNode = std::make_unique<CaseNode>();
    caseNode->location = locOf(node);
    if (const ParseNode* expr = firstChildLabel(node, "<expression>")) caseNode->selector = buildExpression(*expr);
    if (const ParseNode* block = firstChildLabel(node, "<case-block>")) collectCaseBranches(*block, caseNode->branches);
    return caseNode;
}

std::unique_ptr<StatementNode> buildStatement(const ParseNode& node) {
    if (node.label == "<statement>" && node.children.empty()) return nullptr;
    if (node.label == "<assignment-statement>") return buildAssignment(node);
    if (node.label == "<if-statement>") return buildIf(node);
    if (node.label == "<while-statement>") return buildWhile(node);
    if (node.label == "<for-statement>") return buildFor(node);
    if (node.label == "<repeat-statement>") return buildRepeat(node);
    if (node.label == "<case-statement>") return buildCase(node);
    if (node.label == "<procedure/function-call>") return buildProcCall(node);

    if (node.label == "<statement>" && !node.children.empty()) {
        return buildStatement(node.children.front());
    }

    return nullptr;
}

std::unique_ptr<BlockNode> buildCompoundStatement(const ParseNode& node) {
    auto block = std::make_unique<BlockNode>();
    block->location = locOf(node);
    if (const ParseNode* list = firstChildLabel(node, "<statement-list>")) collectStatements(*list, block->statements);
    return block;
}

std::vector<std::unique_ptr<ParamDeclNode>> buildFormalParameters(const ParseNode& node) {
    std::vector<std::unique_ptr<ParamDeclNode>> params;

    for (const ParseNode& group : node.children) {
        if (group.label != "<parameter-group>") continue;

        auto param = std::make_unique<ParamDeclNode>();
        param->location = locOf(group);

        if (const ParseNode* ids = firstChildLabel(group, "<identifier-list>")) {
            param->names = identifierList(*ids);
        }

        for (const ParseNode& child : group.children) {
            if (child.label == "<array-type>") {
                param->type = buildType(child);
            } else if (isToken(child, TokenType::IDENT)) {
                param->type = namedTypeFromToken(child);
            }
        }

        params.push_back(std::move(param));
    }

    return params;
}

std::unique_ptr<DeclarationNode> buildProcedure(const ParseNode& node) {
    auto proc = std::make_unique<ProcDeclNode>();
    proc->location = locOf(node);

    if (const ParseNode* ident = firstToken(node, TokenType::IDENT)) proc->name = ident->token->value;
    if (const ParseNode* params = firstChildLabel(node, "<formal-parameter-list>")) proc->parameters = buildFormalParameters(*params);

    if (const ParseNode* block = firstChildLabel(node, "<block>")) {
        if (const ParseNode* decl = firstChildLabel(*block, "<declaration-part>")) proc->declarations = buildDeclarationPart(*decl);
        if (const ParseNode* body = firstChildLabel(*block, "<compound-statement>")) proc->body = buildCompoundStatement(*body);
    }

    return proc;
}

std::unique_ptr<DeclarationNode> buildFunction(const ParseNode& node) {
    auto func = std::make_unique<FuncDeclNode>();
    func->location = locOf(node);

    bool nameSet = false;
    bool afterColon = false;
    for (const ParseNode& child : node.children) {
        if (isToken(child, TokenType::IDENT) && !nameSet) {
            func->name = child.token->value;
            nameSet = true;
        } else if (isToken(child, TokenType::COLON)) {
            afterColon = true;
        } else if (afterColon && isToken(child, TokenType::IDENT)) {
            func->returnType = namedTypeFromToken(child);
            afterColon = false;
        }
    }

    if (const ParseNode* params = firstChildLabel(node, "<formal-parameter-list>")) func->parameters = buildFormalParameters(*params);

    if (const ParseNode* block = firstChildLabel(node, "<block>")) {
        if (const ParseNode* decl = firstChildLabel(*block, "<declaration-part>")) func->declarations = buildDeclarationPart(*decl);
        if (const ParseNode* body = firstChildLabel(*block, "<compound-statement>")) func->body = buildCompoundStatement(*body);
    }

    return func;
}

void appendConstDecls(const ParseNode& node, std::vector<std::unique_ptr<DeclarationNode>>& out) {
    for (size_t i = 0; i < node.children.size(); ++i) {
        if (!isToken(node.children[i], TokenType::IDENT)) continue;

        auto decl = std::make_unique<ConstDeclNode>();
        decl->name = node.children[i].token->value;
        decl->location = locOf(node.children[i]);

        for (size_t j = i + 1; j < node.children.size(); ++j) {
            if (node.children[j].label == "<constant>") {
                decl->value = buildConstant(node.children[j]);
                break;
            }
            if (isToken(node.children[j], TokenType::SEMICOLON)) break;
        }

        out.push_back(std::move(decl));
    }
}

void appendTypeDecls(const ParseNode& node, std::vector<std::unique_ptr<DeclarationNode>>& out) {
    for (size_t i = 0; i < node.children.size(); ++i) {
        if (!isToken(node.children[i], TokenType::IDENT)) continue;

        auto decl = std::make_unique<TypeDeclNode>();
        decl->name = node.children[i].token->value;
        decl->location = locOf(node.children[i]);

        for (size_t j = i + 1; j < node.children.size(); ++j) {
            if (node.children[j].label == "<type>") {
                decl->type = buildType(node.children[j]);
                break;
            }
            if (isToken(node.children[j], TokenType::SEMICOLON)) break;
        }

        out.push_back(std::move(decl));
    }
}

void appendVarDecls(const ParseNode& node, std::vector<std::unique_ptr<DeclarationNode>>& out) {
    for (size_t i = 0; i < node.children.size(); ++i) {
        if (node.children[i].label != "<identifier-list>") continue;

        auto decl = std::make_unique<VarDeclNode>();
        decl->names = identifierList(node.children[i]);
        decl->location = locOf(node.children[i]);

        for (size_t j = i + 1; j < node.children.size(); ++j) {
            if (node.children[j].label == "<type>") {
                decl->type = buildType(node.children[j]);
                break;
            }
            if (isToken(node.children[j], TokenType::SEMICOLON)) break;
        }

        out.push_back(std::move(decl));
    }
}

std::vector<std::unique_ptr<DeclarationNode>> buildDeclarationPart(const ParseNode& node) {
    std::vector<std::unique_ptr<DeclarationNode>> decls;

    for (const ParseNode& child : node.children) {
        if (child.label == "<const-declaration>") appendConstDecls(child, decls);
        else if (child.label == "<type-declaration>") appendTypeDecls(child, decls);
        else if (child.label == "<var-declaration>") appendVarDecls(child, decls);
        else if (child.label == "<subprogram-declaration>" && !child.children.empty()) {
            const ParseNode& sub = child.children.front();
            if (sub.label == "<procedure-declaration>") decls.push_back(buildProcedure(sub));
            else if (sub.label == "<function-declaration>") decls.push_back(buildFunction(sub));
        }
    }

    return decls;
}

std::unique_ptr<ProgramNode> buildProgram(const ParseNode& node) {
    if (node.label != "<program>") throw std::runtime_error("buildAST expected <program> root");

    auto program = std::make_unique<ProgramNode>();
    program->location = locOf(node);

    if (const ParseNode* header = firstChildLabel(node, "<program-header>")) {
        if (const ParseNode* ident = firstToken(*header, TokenType::IDENT)) program->name = ident->token->value;
    }

    if (const ParseNode* decl = firstChildLabel(node, "<declaration-part>")) program->declarations = buildDeclarationPart(*decl);
    if (const ParseNode* body = firstChildLabel(node, "<compound-statement>")) program->body = buildCompoundStatement(*body);

    return program;
}

} // namespace

std::unique_ptr<ProgramNode> buildAST(const ParseNode& root) {
    return buildProgram(root);
}

ASTNode::ASTNode(ASTNodeKind kind) : kind(kind) {}
TypeNode::TypeNode(ASTNodeKind kind) : ASTNode(kind) {}
DeclarationNode::DeclarationNode(ASTNodeKind kind) : ASTNode(kind) {}
StatementNode::StatementNode(ASTNodeKind kind) : ASTNode(kind) {}
ExpressionNode::ExpressionNode(ASTNodeKind kind) : ASTNode(kind) {}

NamedTypeNode::NamedTypeNode() : TypeNode(ASTNodeKind::NamedType) {}
SubrangeTypeNode::SubrangeTypeNode() : TypeNode(ASTNodeKind::SubrangeType) {}
EnumTypeNode::EnumTypeNode() : TypeNode(ASTNodeKind::EnumType) {}
ArrayTypeNode::ArrayTypeNode() : TypeNode(ASTNodeKind::ArrayType) {}
RecordTypeNode::RecordTypeNode() : TypeNode(ASTNodeKind::RecordType) {}
VarDeclNode::VarDeclNode() : DeclarationNode(ASTNodeKind::VarDecl) {}
ConstDeclNode::ConstDeclNode() : DeclarationNode(ASTNodeKind::ConstDecl) {}
TypeDeclNode::TypeDeclNode() : DeclarationNode(ASTNodeKind::TypeDecl) {}
ParamDeclNode::ParamDeclNode() : DeclarationNode(ASTNodeKind::ParamDecl) {}
ProcDeclNode::ProcDeclNode() : DeclarationNode(ASTNodeKind::ProcDecl) {}
FuncDeclNode::FuncDeclNode() : DeclarationNode(ASTNodeKind::FuncDecl) {}
BlockNode::BlockNode() : StatementNode(ASTNodeKind::Block) {}
ProgramNode::ProgramNode() : ASTNode(ASTNodeKind::Program) {}
AssignNode::AssignNode() : StatementNode(ASTNodeKind::Assign) {}
IfNode::IfNode() : StatementNode(ASTNodeKind::If) {}
WhileNode::WhileNode() : StatementNode(ASTNodeKind::While) {}
ForNode::ForNode() : StatementNode(ASTNodeKind::For) {}
RepeatNode::RepeatNode() : StatementNode(ASTNodeKind::Repeat) {}
CaseNode::CaseNode() : StatementNode(ASTNodeKind::Case) {}
ProcCallNode::ProcCallNode() : StatementNode(ASTNodeKind::ProcCall) {}
FuncCallNode::FuncCallNode() : ExpressionNode(ASTNodeKind::FuncCall) {}
BinOpNode::BinOpNode() : ExpressionNode(ASTNodeKind::BinOp) {}
UnaryOpNode::UnaryOpNode() : ExpressionNode(ASTNodeKind::UnaryOp) {}
VarNode::VarNode() : ExpressionNode(ASTNodeKind::Var) {}
IntLiteralNode::IntLiteralNode() : ExpressionNode(ASTNodeKind::IntLiteral) {}
RealLiteralNode::RealLiteralNode() : ExpressionNode(ASTNodeKind::RealLiteral) {}
CharLiteralNode::CharLiteralNode() : ExpressionNode(ASTNodeKind::CharLiteral) {}
StringLiteralNode::StringLiteralNode() : ExpressionNode(ASTNodeKind::StringLiteral) {}
BoolLiteralNode::BoolLiteralNode() : ExpressionNode(ASTNodeKind::BoolLiteral) {}
ArrayAccessNode::ArrayAccessNode() : ExpressionNode(ASTNodeKind::ArrayAccess) {}
RecordAccessNode::RecordAccessNode() : ExpressionNode(ASTNodeKind::RecordAccess) {}

std::string NamedTypeNode::toString(int i) const {
    return indent(i) + "NamedType(" + name + ")" + ann(*this);
}

std::string SubrangeTypeNode::toString(int i) const {
    std::string s = indent(i) + "SubrangeType" + ann(*this);
    appendNode(s, "lower", lowerBound.get(), i + 2);
    appendNode(s, "upper", upperBound.get(), i + 2);
    return s;
}

std::string EnumTypeNode::toString(int i) const {
    return indent(i) + "EnumType(" + join(values) + ")" + ann(*this);
}

std::string ArrayTypeNode::toString(int i) const {
    std::string s = indent(i) + "ArrayType" + ann(*this);
    appendNode(s, "index", indexType.get(), i + 2);
    appendNode(s, "element", elementType.get(), i + 2);
    return s;
}

std::string RecordTypeNode::toString(int i) const {
    std::string s = indent(i) + "RecordType" + ann(*this);
    for (const RecordFieldNode& field : fields) {
        s += "\n" + indent(i + 2) + "Field(" + join(field.names) + ")";
        if (field.type) s += "\n" + field.type->toString(i + 4);
    }
    return s;
}

std::string VarDeclNode::toString(int i) const {
    std::string s = indent(i) + "VarDecl(" + join(names) + ")" + ann(*this);
    appendNode(s, "type", type.get(), i + 2);
    return s;
}

std::string ConstDeclNode::toString(int i) const {
    std::string s = indent(i) + "ConstDecl(" + name + ")" + ann(*this);
    appendNode(s, "value", value.get(), i + 2);
    return s;
}

std::string TypeDeclNode::toString(int i) const {
    std::string s = indent(i) + "TypeDecl(" + name + ")" + ann(*this);
    appendNode(s, "type", type.get(), i + 2);
    return s;
}

std::string ParamDeclNode::toString(int i) const {
    std::string s = indent(i) + "ParamDecl(" + join(names) + (byReference ? ", var" : "") + ")" + ann(*this);
    appendNode(s, "type", type.get(), i + 2);
    return s;
}

std::string ProcDeclNode::toString(int i) const {
    std::string s = indent(i) + "ProcDecl(" + name + ", block:" + std::to_string(blockIndex) + ")" + ann(*this);
    if (!parameters.empty()) appendSection(s, "params", i + 2);
    for (const auto& param : parameters) if (param) s += "\n" + param->toString(i + 4);
    if (!declarations.empty()) appendSection(s, "declarations", i + 2);
    for (const auto& decl : declarations) if (decl) s += "\n" + decl->toString(i + 4);
    appendNode(s, "body", body.get(), i + 2);
    return s;
}

std::string FuncDeclNode::toString(int i) const {
    std::string s = indent(i) + "FuncDecl(" + name + ", block:" + std::to_string(blockIndex) + ")" + ann(*this);
    if (!parameters.empty()) appendSection(s, "params", i + 2);
    for (const auto& param : parameters) if (param) s += "\n" + param->toString(i + 4);
    appendNode(s, "returnType", returnType.get(), i + 2);
    if (!declarations.empty()) appendSection(s, "declarations", i + 2);
    for (const auto& decl : declarations) if (decl) s += "\n" + decl->toString(i + 4);
    appendNode(s, "body", body.get(), i + 2);
    return s;
}

std::string BlockNode::toString(int i) const {
    std::string s = indent(i) + "Block(block:" + std::to_string(blockIndex) + ")" + ann(*this);
    for (const auto& stmt : statements) if (stmt) s += "\n" + stmt->toString(i + 2);
    return s;
}

std::string ProgramNode::toString(int i) const {
    std::string s = indent(i) + "Program(" + name + ", block:" + std::to_string(blockIndex) + ")" + ann(*this);
    if (!declarations.empty()) appendSection(s, "declarations", i + 2);
    for (const auto& decl : declarations) if (decl) s += "\n" + decl->toString(i + 4);
    appendNode(s, "body", body.get(), i + 2);
    return s;
}

std::string AssignNode::toString(int i) const {
    std::string s = indent(i) + "Assign" + ann(*this);
    appendNode(s, "target", target.get(), i + 2);
    appendNode(s, "value", value.get(), i + 2);
    return s;
}

std::string IfNode::toString(int i) const {
    std::string s = indent(i) + "If" + ann(*this);
    appendNode(s, "condition", condition.get(), i + 2);
    appendNode(s, "then", thenBranch.get(), i + 2);
    appendNode(s, "else", elseBranch.get(), i + 2);
    return s;
}

std::string WhileNode::toString(int i) const {
    std::string s = indent(i) + "While" + ann(*this);
    appendNode(s, "condition", condition.get(), i + 2);
    appendNode(s, "body", body.get(), i + 2);
    return s;
}

std::string ForNode::toString(int i) const {
    std::string s = indent(i) + "For(" + controlVariable + ", " +
                    (direction == ForDirection::Downto ? "downto" : "to") + ")" + ann(*this);
    appendNode(s, "start", startValue.get(), i + 2);
    appendNode(s, "end", endValue.get(), i + 2);
    appendNode(s, "body", body.get(), i + 2);
    return s;
}

std::string RepeatNode::toString(int i) const {
    std::string s = indent(i) + "Repeat" + ann(*this);
    if (!body.empty()) appendSection(s, "body", i + 2);
    for (const auto& stmt : body) if (stmt) s += "\n" + stmt->toString(i + 4);
    appendNode(s, "until", condition.get(), i + 2);
    return s;
}

std::string CaseNode::toString(int i) const {
    std::string s = indent(i) + "Case" + ann(*this);
    appendNode(s, "selector", selector.get(), i + 2);
    for (const CaseBranchNode& branch : branches) {
        s += "\n" + indent(i + 2) + "Branch:";
        if (!branch.labels.empty()) {
            s += "\n" + indent(i + 4) + "labels:";
            for (const auto& label : branch.labels) if (label) s += "\n" + label->toString(i + 6);
        }
        appendNode(s, "statement", branch.statement.get(), i + 4);
    }
    return s;
}

std::string ProcCallNode::toString(int i) const {
    std::string s = indent(i) + "ProcCall(" + name + ")" + ann(*this);
    for (const auto& arg : arguments) if (arg) s += "\n" + arg->toString(i + 2);
    return s;
}

std::string FuncCallNode::toString(int i) const {
    std::string s = indent(i) + "FuncCall(" + name + ")" + ann(*this);
    for (const auto& arg : arguments) if (arg) s += "\n" + arg->toString(i + 2);
    return s;
}

std::string BinOpNode::toString(int i) const {
    std::string s = indent(i) + "BinOp(" + op + ")" + ann(*this);
    appendNode(s, "left", left.get(), i + 2);
    appendNode(s, "right", right.get(), i + 2);
    return s;
}

std::string UnaryOpNode::toString(int i) const {
    std::string s = indent(i) + "UnaryOp(" + op + ")" + ann(*this);
    appendNode(s, "operand", operand.get(), i + 2);
    return s;
}

std::string VarNode::toString(int i) const {
    return indent(i) + "Var(" + name + ")" + ann(*this);
}

std::string IntLiteralNode::toString(int i) const {
    return indent(i) + "Int(" + std::to_string(value) + ")" + ann(*this);
}

std::string RealLiteralNode::toString(int i) const {
    std::ostringstream out;
    out << indent(i) << "Real(" << value << ")" << ann(*this);
    return out.str();
}

std::string CharLiteralNode::toString(int i) const {
    const std::string text = value == '\0' ? "\\0" : std::string(1, value);
    return indent(i) + "Char('" + text + "')" + ann(*this);
}

std::string StringLiteralNode::toString(int i) const {
    return indent(i) + "String(\"" + value + "\")" + ann(*this);
}

std::string BoolLiteralNode::toString(int i) const {
    return indent(i) + std::string("Bool(") + (value ? "true" : "false") + ")" + ann(*this);
}

std::string ArrayAccessNode::toString(int i) const {
    std::string s = indent(i) + "ArrayAccess" + ann(*this);
    appendNode(s, "array", array.get(), i + 2);
    if (!indices.empty()) appendSection(s, "indices", i + 2);
    for (const auto& index : indices) if (index) s += "\n" + index->toString(i + 4);
    return s;
}

std::string RecordAccessNode::toString(int i) const {
    std::string s = indent(i) + "RecordAccess(" + fieldName + ")" + ann(*this);
    appendNode(s, "record", record.get(), i + 2);
    return s;
}
