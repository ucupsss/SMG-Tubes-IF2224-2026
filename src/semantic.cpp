#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

std::string typeNameFromCode(int type) {
    switch (type) {
        case TYPE_INTEGER: return "integer";
        case TYPE_REAL: return "real";
        case TYPE_CHAR: return "char";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_STRING: return "string";
        case TYPE_ARRAY: return "array";
        case TYPE_RECORD: return "record";
        case TYPE_SUBRANGE: return "subrange";
        case TYPE_ENUM: return "enum";
        case TYPE_ERROR: return "error";
        default: return "void";
    }
}

std::string objectNameFromCode(int obj) {
    switch (obj) {
        case OBJ_CONSTANT: return "constant";
        case OBJ_VARIABLE: return "variable";
        case OBJ_TYPE: return "type";
        case OBJ_PROCEDURE: return "procedure";
        case OBJ_FUNCTION: return "function";
        case OBJ_PROGRAM: return "program";
        case OBJ_RESERVED: return "reserved";
        default: return "unknown";
    }
}

int treeLevelOf(const std::string& line) {
    int spaces = 0;
    while (spaces < static_cast<int>(line.size()) && line[spaces] == ' ') {
        ++spaces;
    }

    return spaces / 2;
}

std::string trimTreeIndent(const std::string& line) {
    size_t firstNonSpace = line.find_first_not_of(' ');
    if (firstNonSpace == std::string::npos) {
        return "";
    }

    return line.substr(firstNonSpace);
}

bool hasNextSibling(const std::vector<int>& levels, size_t index, int level) {
    for (size_t i = index + 1; i < levels.size(); ++i) {
        if (levels[i] < level) {
            return false;
        }

        if (levels[i] == level) {
            return true;
        }
    }

    return false;
}

std::string convertAnnotation(const std::string& label) {
    const std::string marker = " [type:";
    const size_t markerPos = label.find(marker);
    if (markerPos == std::string::npos) {
        return label;
    }

    const std::string nodeText = label.substr(0, markerPos);
    const size_t typeStart = markerPos + marker.size();
    const size_t tabStart = label.find(", tab:", typeStart);
    const size_t levStart = label.find(", lev:", typeStart);
    const size_t end = label.find(']', typeStart);

    if (tabStart == std::string::npos || levStart == std::string::npos || end == std::string::npos) {
        return label;
    }

    const int type = std::stoi(label.substr(typeStart, tabStart - typeStart));
    const int tabIndex = std::stoi(label.substr(tabStart + 6, levStart - (tabStart + 6)));
    const int level = std::stoi(label.substr(levStart + 6, end - (levStart + 6)));

    std::string result = nodeText + " -> ";
    if (tabIndex >= 0) {
        result += "tab_index:" + std::to_string(tabIndex) + ", ";
    }
    result += "type:" + typeNameFromCode(type) + ", lev:" + std::to_string(level);

    return result;
}

std::vector<std::string> formatTreeLines(const std::string& text) {
    std::vector<std::string> rawLines;
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty()) {
            rawLines.push_back(line);
        }
    }

    std::vector<int> levels;
    for (const std::string& rawLine : rawLines) {
        levels.push_back(treeLevelOf(rawLine));
    }

    std::vector<std::string> treeLines;
    for (size_t i = 0; i < rawLines.size(); ++i) {
        const int level = levels[i];
        const std::string label = convertAnnotation(trimTreeIndent(rawLines[i]));

        if (level == 0) {
            treeLines.push_back(label);
            continue;
        }

        std::string prefix;
        for (int ancestorLevel = 1; ancestorLevel < level; ++ancestorLevel) {
            prefix += hasNextSibling(levels, i, ancestorLevel) ? "│   " : "    ";
        }

        prefix += hasNextSibling(levels, i, level) ? "├─ " : "└─ ";
        treeLines.push_back(prefix + label);
    }

    return treeLines;
}

std::string formatTabEntry(int index, const TabEntry& entry) {
    std::ostringstream out;
    out << std::left
        << std::setw(4) << index
        << std::setw(14) << entry.identifier
        << std::setw(11) << objectNameFromCode(entry.obj)
        << std::setw(8) << typeNameFromCode(entry.type)
        << std::right
        << std::setw(5) << entry.ref
        << std::setw(5) << entry.nrm
        << std::setw(5) << entry.lev
        << std::setw(5) << entry.adr
        << std::setw(6) << entry.link;
    return out.str();
}

std::string formatBTabEntry(int index, const BTabEntry& entry) {
    std::ostringstream out;
    out << std::left
        << std::setw(4) << index
        << std::right
        << std::setw(6) << entry.last
        << std::setw(6) << entry.lpar
        << std::setw(6) << entry.psze
        << std::setw(6) << entry.vsze;
    return out.str();
}

std::string formatATabEntry(int index, const ATabEntry& entry) {
    std::ostringstream out;
    out << std::left
        << std::setw(4) << index
        << std::setw(11) << typeNameFromCode(entry.xtyp)
        << std::setw(11) << typeNameFromCode(entry.etyp)
        << std::right
        << std::setw(6) << entry.eref
        << std::setw(6) << entry.low
        << std::setw(6) << entry.high
        << std::setw(6) << entry.elsz
        << std::setw(6) << entry.size;
    return out.str();
}

}

SemanticAnalyzer::SemanticAnalyzer() {
    symbolTable.init();
}

void SemanticAnalyzer::annotate(ASTNode* node, const TypeInfo& type, int tabIndex) {
    if (!node) {
        return;
    }

    node->inferredType = type.code;
    if (tabIndex != -1 || node->tabIndex == -1) {
        node->tabIndex = tabIndex;
    }
    node->lexLevel = symbolTable.currentLevel();

    if (auto* typeNode = dynamic_cast<TypeNode*>(node)) {
        typeNode->typeCode = type.code;
        typeNode->ref = type.ref;
        typeNode->isNamed = type.isNamed;
        typeNode->typeName = type.name;
    }
}

void SemanticAnalyzer::analyze(const ParseNode& root) {
    analyzeAST(buildASTFromParseTree(root));
}

void SemanticAnalyzer::analyzeAST(std::unique_ptr<ProgramNode> root) {
    rootAst = std::move(root);
    errorList.clear();
    warningList.clear();
    symbolTable.init();

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

    std::vector<std::string> symbolLines = formatSymbolTables();
    lines.insert(lines.end(), symbolLines.begin(), symbolLines.end());

    lines.push_back("");
    std::vector<std::string> astLines = formatDecoratedAST();
    lines.insert(lines.end(), astLines.begin(), astLines.end());

    lines.push_back("");
    lines.push_back("Semantic diagnostics:");

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
    lines.push_back(std::string("Status: ") + (hasErrors() ? "FAILED" : "SUCCESS"));

    return lines;
}

std::vector<std::string> SemanticAnalyzer::formatDecoratedAST() const {
    std::vector<std::string> lines;
    lines.push_back("Decorated AST (contoh anotasi minimal):");

    if (rootAst == nullptr) {
        lines.push_back("<empty>");
        return lines;
    }

    std::vector<std::string> astLines = formatTreeLines(rootAst->toString(0));
    lines.insert(lines.end(), astLines.begin(), astLines.end());
    return lines;
}

std::vector<std::string> SemanticAnalyzer::formatSymbolTables() const {
    std::vector<std::string> lines;
    lines.push_back("tab (hanya sebagian yang relevan):");
    lines.push_back("idx id            obj        type      ref  nrm  lev  adr  link");
    lines.push_back("---------------------------------------------------------------");
    lines.push_back("... (reserved words dan predefined identifiers)");

    const std::vector<TabEntry>& tab = symbolTable.tab();
    for (size_t i = 1; i < tab.size(); ++i) {
        if (tab[i].obj == OBJ_RESERVED || tab[i].obj == OBJ_TYPE) {
            continue;
        }

        lines.push_back(formatTabEntry(static_cast<int>(i), tab[i]));
    }

    lines.push_back("");
    lines.push_back("btab:");
    lines.push_back("idx  last  lpar  psze  vsze");
    lines.push_back("----------------------------");

    const std::vector<BTabEntry>& btab = symbolTable.btab();
    for (size_t i = 0; i < btab.size(); ++i) {
        lines.push_back(formatBTabEntry(static_cast<int>(i), btab[i]));
    }

    lines.push_back("");

    const std::vector<ATabEntry>& atab = symbolTable.atab();
    if (atab.size() <= 1) {
        lines.push_back("atab: (kosong karena tidak ada array)");
    } else {
        lines.push_back("atab:");
        lines.push_back("idx xtyp       etyp         eref   low  high  elsz  size");
        lines.push_back("--------------------------------------------------------");
        for (size_t i = 1; i < atab.size(); ++i) {
            lines.push_back(formatATabEntry(static_cast<int>(i), atab[i]));
        }
    }

    return lines;
}

std::unique_ptr<ProgramNode> SemanticAnalyzer::buildASTFromParseTree(const ParseNode& root) {
    return buildAST(root);
}
