#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"
#include "parser.hpp"
#include "symbol_table.hpp"

#include <memory>
#include <string>
#include <vector>

struct SemanticDiagnostic {
    std::string message;
    int line = -1;
    int column = -1;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    void analyze(const ParseNode& root);
    void analyzeAST(std::unique_ptr<ProgramNode> root);

    bool hasErrors() const;
    const std::vector<SemanticDiagnostic>& errors() const;
    const std::vector<SemanticDiagnostic>& warnings() const;
    const ProgramNode* ast() const;
    const SymbolTable& symbols() const;

    std::vector<std::string> formatOutput() const;
    std::vector<std::string> formatDecoratedAST() const;
    std::vector<std::string> formatSymbolTables() const;

private:
    std::unique_ptr<ProgramNode> buildASTFromParseTree(const ParseNode& root);

    void visitProgram(ProgramNode* node);
    void visitBlock(BlockNode* node);

    void visitDeclaration(DeclarationNode* node);
    void visitVarDecl(VarDeclNode* node);
    void visitConstDecl(ConstDeclNode* node);
    void visitTypeDecl(TypeDeclNode* node);
    void visitParamDecl(ParamDeclNode* node);
    void visitProcDecl(ProcDeclNode* node);
    void visitFuncDecl(FuncDeclNode* node);

    void visitStatement(StatementNode* node);
    void visitAssign(AssignNode* node);
    void visitIf(IfNode* node);
    void visitWhile(WhileNode* node);
    void visitFor(ForNode* node);
    void visitRepeat(RepeatNode* node);
    void visitCase(CaseNode* node);
    void visitProcCall(ProcCallNode* node);

    TypeInfo visitExpression(ExpressionNode* node);
    TypeInfo visitBinOp(BinOpNode* node);
    TypeInfo visitUnaryOp(UnaryOpNode* node);
    TypeInfo visitVar(VarNode* node);
    TypeInfo visitArrayAccess(ArrayAccessNode* node);
    TypeInfo visitRecordAccess(RecordAccessNode* node);
    TypeInfo visitFuncCall(FuncCallNode* node);

    TypeInfo resolveType(TypeNode* node);
    TypeInfo resolveNamedType(NamedTypeNode* node);
    TypeInfo resolveArrayType(ArrayTypeNode* node);
    TypeInfo resolveRecordType(RecordTypeNode* node);
    TypeInfo resolveSubrangeType(SubrangeTypeNode* node);
    TypeInfo resolveEnumType(EnumTypeNode* node);

    bool isCompatible(const TypeInfo& lhs, const TypeInfo& rhs) const;
    bool isAssignmentCompatible(const TypeInfo& target, const TypeInfo& value) const;
    bool isOrdinal(const TypeInfo& type) const;
    bool isNumeric(const TypeInfo& type) const;
    bool isBoolean(const TypeInfo& type) const;
    bool isAssignableEntry(const TabEntry& entry) const;

    void annotate(ASTNode* node, const TypeInfo& type, int tabIndex = -1);
    void semanticError(const std::string& message, const SourceLocation& location = {});
    void semanticWarning(const std::string& message, const SourceLocation& location = {});

    SymbolTable symbolTable;
    std::unique_ptr<ProgramNode> rootAst;
    std::vector<SemanticDiagnostic> errorList;
    std::vector<SemanticDiagnostic> warningList;
};

#endif
