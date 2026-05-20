#ifndef AST_HPP
#define AST_HPP

#include <memory>
#include <string>
#include <vector>

struct ParseNode;

struct SourceLocation {
    int line = -1;
    int column = -1;
};

enum class ASTNodeKind {
    Program,
    Block,
    VarDecl,
    ConstDecl,
    TypeDecl,
    ParamDecl,
    ProcDecl,
    FuncDecl,
    NamedType,
    ArrayType,
    RecordType,
    SubrangeType,
    EnumType,
    Assign,
    If,
    While,
    For,
    Repeat,
    Case,
    ProcCall,
    FuncCall,
    BinOp,
    UnaryOp,
    Var,
    IntLiteral,
    RealLiteral,
    CharLiteral,
    StringLiteral,
    BoolLiteral,
    ArrayAccess,
    RecordAccess
};

struct ASTNode {
    explicit ASTNode(ASTNodeKind kind);
    virtual ~ASTNode() = default;

    ASTNodeKind kind;
    SourceLocation location;
    int inferredType = 0;
    int tabIndex = -1;
    int lexLevel = 0;

    virtual std::string toString(int indent = 0) const = 0;
};

struct TypeNode : ASTNode {
    explicit TypeNode(ASTNodeKind kind);
    int typeCode = 0;
    int ref = 0;
    bool isNamed = false;
    std::string typeName;
};

struct NamedTypeNode : TypeNode {
    std::string name;

    NamedTypeNode();
    std::string toString(int indent = 0) const override;
};

struct SubrangeTypeNode : TypeNode {
    std::unique_ptr<ASTNode> lowerBound;
    std::unique_ptr<ASTNode> upperBound;

    SubrangeTypeNode();
    std::string toString(int indent = 0) const override;
};

struct EnumTypeNode : TypeNode {
    std::vector<std::string> values;

    EnumTypeNode();
    std::string toString(int indent = 0) const override;
};

struct ArrayTypeNode : TypeNode {
    std::unique_ptr<TypeNode> indexType;
    std::unique_ptr<TypeNode> elementType;

    ArrayTypeNode();
    std::string toString(int indent = 0) const override;
};

struct RecordFieldNode {
    std::vector<std::string> names;
    std::unique_ptr<TypeNode> type;
    SourceLocation location;
};

struct RecordTypeNode : TypeNode {
    std::vector<RecordFieldNode> fields;

    RecordTypeNode();
    std::string toString(int indent = 0) const override;
};

struct DeclarationNode : ASTNode {
    explicit DeclarationNode(ASTNodeKind kind);
};

struct VarDeclNode : DeclarationNode {
    std::vector<std::string> names;
    std::unique_ptr<TypeNode> type;

    VarDeclNode();
    std::string toString(int indent = 0) const override;
};

struct ConstDeclNode : DeclarationNode {
    std::string name;
    std::unique_ptr<ASTNode> value;

    ConstDeclNode();
    std::string toString(int indent = 0) const override;
};

struct TypeDeclNode : DeclarationNode {
    std::string name;
    std::unique_ptr<TypeNode> type;

    TypeDeclNode();
    std::string toString(int indent = 0) const override;
};

struct ParamDeclNode : DeclarationNode {
    std::vector<std::string> names;
    std::unique_ptr<TypeNode> type;
    bool byReference = false;

    ParamDeclNode();
    std::string toString(int indent = 0) const override;
};

struct BlockNode;

struct ProcDeclNode : DeclarationNode {
    std::string name;
    std::vector<std::unique_ptr<ParamDeclNode>> parameters;
    std::vector<std::unique_ptr<DeclarationNode>> declarations;
    std::unique_ptr<BlockNode> body;
    int blockIndex = -1;

    ProcDeclNode();
    std::string toString(int indent = 0) const override;
};

struct FuncDeclNode : DeclarationNode {
    std::string name;
    std::vector<std::unique_ptr<ParamDeclNode>> parameters;
    std::unique_ptr<TypeNode> returnType;
    std::vector<std::unique_ptr<DeclarationNode>> declarations;
    std::unique_ptr<BlockNode> body;
    int blockIndex = -1;

    FuncDeclNode();
    std::string toString(int indent = 0) const override;
};

struct StatementNode : ASTNode {
    explicit StatementNode(ASTNodeKind kind);
};

struct ExpressionNode : ASTNode {
    explicit ExpressionNode(ASTNodeKind kind);
};

struct BlockNode : StatementNode {
    std::vector<std::unique_ptr<StatementNode>> statements;
    int blockIndex = -1;

    BlockNode();
    std::string toString(int indent = 0) const override;
};

struct ProgramNode : ASTNode {
    std::string name;
    std::vector<std::unique_ptr<DeclarationNode>> declarations;
    std::unique_ptr<BlockNode> body;
    int blockIndex = 0;

    ProgramNode();
    std::string toString(int indent = 0) const override;
};

struct AssignNode : StatementNode {
    std::unique_ptr<ExpressionNode> target;
    std::unique_ptr<ExpressionNode> value;

    AssignNode();
    std::string toString(int indent = 0) const override;
};

struct IfNode : StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> thenBranch;
    std::unique_ptr<StatementNode> elseBranch;

    IfNode();
    std::string toString(int indent = 0) const override;
};

struct WhileNode : StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> body;

    WhileNode();
    std::string toString(int indent = 0) const override;
};

enum class ForDirection {
    To,
    Downto
};

struct ForNode : StatementNode {
    std::string controlVariable;
    std::unique_ptr<ExpressionNode> startValue;
    std::unique_ptr<ExpressionNode> endValue;
    ForDirection direction = ForDirection::To;
    std::unique_ptr<BlockNode> body;

    ForNode();
    std::string toString(int indent = 0) const override;
};

struct RepeatNode : StatementNode {
    std::vector<std::unique_ptr<StatementNode>> body;
    std::unique_ptr<ExpressionNode> condition;

    RepeatNode();
    std::string toString(int indent = 0) const override;
};

struct CaseBranchNode {
    std::vector<std::unique_ptr<ExpressionNode>> labels;
    std::unique_ptr<StatementNode> statement;
    SourceLocation location;
};

struct CaseNode : StatementNode {
    std::unique_ptr<ExpressionNode> selector;
    std::vector<CaseBranchNode> branches;

    CaseNode();
    std::string toString(int indent = 0) const override;
};

struct ProcCallNode : StatementNode {
    std::string name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;

    ProcCallNode();
    std::string toString(int indent = 0) const override;
};

struct FuncCallNode : ExpressionNode {
    std::string name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;

    FuncCallNode();
    std::string toString(int indent = 0) const override;
};

struct BinOpNode : ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;

    BinOpNode();
    std::string toString(int indent = 0) const override;
};

struct UnaryOpNode : ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> operand;

    UnaryOpNode();
    std::string toString(int indent = 0) const override;
};

struct VarNode : ExpressionNode {
    std::string name;

    VarNode();
    std::string toString(int indent = 0) const override;
};

struct IntLiteralNode : ExpressionNode {
    int value = 0;

    IntLiteralNode();
    std::string toString(int indent = 0) const override;
};

struct RealLiteralNode : ExpressionNode {
    double value = 0.0;

    RealLiteralNode();
    std::string toString(int indent = 0) const override;
};

struct CharLiteralNode : ExpressionNode {
    char value = '\0';

    CharLiteralNode();
    std::string toString(int indent = 0) const override;
};

struct StringLiteralNode : ExpressionNode {
    std::string value;

    StringLiteralNode();
    std::string toString(int indent = 0) const override;
};

struct BoolLiteralNode : ExpressionNode {
    bool value = false;

    BoolLiteralNode();
    std::string toString(int indent = 0) const override;
};

struct ArrayAccessNode : ExpressionNode {
    std::unique_ptr<ExpressionNode> array;
    std::vector<std::unique_ptr<ExpressionNode>> indices;

    ArrayAccessNode();
    std::string toString(int indent = 0) const override;
};

struct RecordAccessNode : ExpressionNode {
    std::unique_ptr<ExpressionNode> record;
    std::string fieldName;

    RecordAccessNode();
    std::string toString(int indent = 0) const override;
};

std::unique_ptr<ProgramNode> buildAST(const ParseNode& root);

#endif
