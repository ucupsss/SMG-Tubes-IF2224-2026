#include "semantic.hpp"

SemanticAnalyzer::SemanticAnalyzer() {
    symbolTable.init();
}

void SemanticAnalyzer::analyze(const ParseNode& root) {
    analyzeAST(buildASTFromParseTree(root));
}

void SemanticAnalyzer::analyzeAST(std::unique_ptr<ProgramNode> root) {
    rootAst = std::move(root);
    errorList.clear();
    warningList.clear();
    symbolTable.init();
    currentLevel = symbolTable.currentLevel();
    currentBlock = symbolTable.currentBlock();
    insideLoop = false;

    if (!rootAst) {
        semanticError("missing AST root");
        return;
    }

    visitProgram(rootAst.get());
}

bool SemanticAnalyzer::hasErrors() const {
    return !errorList.empty();
}

const std::vector<SemanticDiagnostic>& SemanticAnalyzer::errors() const {
    return errorList;
}

const std::vector<SemanticDiagnostic>& SemanticAnalyzer::warnings() const {
    return warningList;
}

const ProgramNode* SemanticAnalyzer::ast() const {
    return rootAst.get();
}

const SymbolTable& SemanticAnalyzer::symbols() const {
    return symbolTable;
}

std::vector<std::string> SemanticAnalyzer::formatOutput() const {
    std::vector<std::string> lines;

    std::vector<std::string> astLines = formatDecoratedAST();
    lines.insert(lines.end(), astLines.begin(), astLines.end());
    lines.push_back("");

    std::vector<std::string> symbolLines = formatSymbolTables();
    lines.insert(lines.end(), symbolLines.begin(), symbolLines.end());
    lines.push_back("");
    lines.push_back("=== Semantic Diagnostics ===");

    if (warningList.empty() && errorList.empty()) {
        lines.push_back("No semantic errors or warnings.");
    } else {
        for (const SemanticDiagnostic& warning : warningList) {
            std::string message = "Warning";
            if (warning.line >= 0) {
                message += " at line " + std::to_string(warning.line);

                if (warning.column >= 0) {
                    message += ", column " + std::to_string(warning.column);
                }
            }
            message += ": " + warning.message;
            lines.push_back(message);
        }
        for (const SemanticDiagnostic& error : errorList) {
            std::string message = "Error";
            if (error.line >= 0) {
                message += " at line " + std::to_string(error.line);

                if (error.column >= 0) {
                    message += ", column " + std::to_string(error.column);
                }
            }
            message += ": " + error.message;
            lines.push_back(message);
        }
    }
    lines.push_back("");
    lines.push_back("=== Semantic Analysis Result ===");
    if (hasErrors()) {
        lines.push_back("FAILED");
    } else {
        lines.push_back("SUCCESS");
    }
    return lines;
}

std::vector<std::string> SemanticAnalyzer::formatDecoratedAST() const {
    std::vector<std::string> lines;
    lines.push_back("=== Decorated AST ===");
    if (rootAst == nullptr) {
        lines.push_back("<empty>");
        return lines;
    }
    lines.push_back(rootAst->toString(0));
    return lines;
}

std::vector<std::string> SemanticAnalyzer::formatSymbolTables() const {
    std::vector<std::string> lines;
    lines.push_back("=== Symbol Tables ===");
    std::vector<std::string> tabLines = symbolTable.formatTab();
    lines.insert(lines.end(), tabLines.begin(), tabLines.end());
    lines.push_back("");

    std::vector<std::string> btabLines = symbolTable.formatBTab();
    lines.insert(lines.end(), btabLines.begin(), btabLines.end());
    lines.push_back("");

    std::vector<std::string> atabLines = symbolTable.formatATab();
    lines.insert(lines.end(), atabLines.begin(), atabLines.end());
    return lines;
}

std::unique_ptr<ProgramNode> SemanticAnalyzer::buildASTFromParseTree(const ParseNode& root) {
    return buildAST(root);
}
