#include "semantic.hpp"
#include "text_utils.hpp"

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

struct TabDisplayInfo {
    std::vector<int> visibleIndices;
    std::vector<int> displayIndexByInternal;
    int idWidth = 10;
};

void collectReferencedTabIndices(const std::string& text, std::vector<bool>& referenced) {
    const std::string marker = "tab:";
    size_t pos = 0;

    while ((pos = text.find(marker, pos)) != std::string::npos) {
        pos += marker.size();

        size_t end = pos;
        while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) {
            ++end;
        }

        if (end > pos) {
            const int index = std::stoi(text.substr(pos, end - pos));
            if (index >= 0 && index < static_cast<int>(referenced.size())) {
                referenced[static_cast<size_t>(index)] = true;
            }
        }
    }
}

bool shouldShowUserEntry(const TabEntry& entry) {
    return entry.obj != OBJ_RESERVED;
}

bool shouldShowReferencedPredefinedEntry(const TabEntry& entry) {
    return entry.obj == OBJ_CONSTANT ||
           entry.obj == OBJ_PROCEDURE ||
           entry.obj == OBJ_FUNCTION;
}

TabDisplayInfo buildTabDisplayInfo(
    const std::vector<TabEntry>& tab,
    const ProgramNode* rootAst
) {
    TabDisplayInfo info;
    info.displayIndexByInternal.assign(tab.size(), 0);

    int programIndex = static_cast<int>(tab.size());
    for (size_t i = 1; i < tab.size(); ++i) {
        if (tab[i].obj == OBJ_PROGRAM) {
            programIndex = static_cast<int>(i);
            break;
        }
    }

    std::vector<bool> referenced(tab.size(), false);
    if (rootAst != nullptr) {
        collectReferencedTabIndices(rootAst->toString(0), referenced);
    }

    for (int i = programIndex; i < static_cast<int>(tab.size()); ++i) {
        if (shouldShowUserEntry(tab[static_cast<size_t>(i)])) {
            info.visibleIndices.push_back(i);
        }
    }

    for (int i = 1; i < programIndex; ++i) {
        const TabEntry& entry = tab[static_cast<size_t>(i)];
        if (referenced[static_cast<size_t>(i)] && shouldShowReferencedPredefinedEntry(entry)) {
            info.visibleIndices.push_back(i);
        }
    }

    for (size_t i = 0; i < info.visibleIndices.size(); ++i) {
        const int internalIndex = info.visibleIndices[i];
        info.displayIndexByInternal[static_cast<size_t>(internalIndex)] =
            33 + static_cast<int>(i);

        const int neededWidth =
            static_cast<int>(tab[static_cast<size_t>(internalIndex)].identifier.size()) + 2;
        info.idWidth = std::max(info.idWidth, neededWidth);
    }

    return info;
}

int toDisplayIndex(const std::vector<int>& displayIndexByInternal, int internalIndex) {
    if (internalIndex <= 0 || internalIndex >= static_cast<int>(displayIndexByInternal.size())) {
        return 0;
    }

    return displayIndexByInternal[static_cast<size_t>(internalIndex)];
}

int displayLinkForEntry(
    const std::vector<TabEntry>& tab,
    const std::vector<int>& displayIndexByInternal,
    const TabEntry& entry
) {
    if (entry.link <= 0 || entry.link >= static_cast<int>(tab.size())) {
        return 0;
    }

    const TabEntry& linked = tab[static_cast<size_t>(entry.link)];
    if (linked.obj != entry.obj || linked.lev != entry.lev) {
        return 0;
    }

    return toDisplayIndex(displayIndexByInternal, entry.link);
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

size_t displayWidth(const std::string& text) {
    size_t width = 0;
    for (unsigned char ch : text) {
        if ((ch & 0xC0) != 0x80) {
            ++width;
        }
    }
    return width;
}

std::string convertAnnotation(
    const std::string& label,
    const std::vector<int>& displayIndexByInternal
) {
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
    const int displayIndex = toDisplayIndex(displayIndexByInternal, tabIndex);
    if (displayIndex > 0) {
        result += "tab_index:" + std::to_string(displayIndex) + ", ";
    }
    result += "type:" + typeNameFromCode(type) + ", lev:" + std::to_string(level);

    return result;
}

void alignTreeAnnotations(std::vector<std::string>& lines) {
    const std::string arrow = " -> ";
    size_t arrowColumn = 0;

    for (const std::string& line : lines) {
        const size_t arrowPos = line.find(arrow);
        if (arrowPos == std::string::npos) {
            continue;
        }

        arrowColumn = std::max(arrowColumn, displayWidth(line.substr(0, arrowPos)));
    }

    for (std::string& line : lines) {
        const size_t arrowPos = line.find(arrow);
        if (arrowPos == std::string::npos) {
            continue;
        }

        const size_t currentColumn = displayWidth(line.substr(0, arrowPos));
        line.insert(arrowPos, std::string(arrowColumn - currentColumn, ' '));
    }
}

std::vector<std::string> formatTreeLines(
    const std::string& text,
    const std::vector<int>& displayIndexByInternal
) {
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
        const std::string label = convertAnnotation(trimTreeIndent(rawLines[i]), displayIndexByInternal);

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

    alignTreeAnnotations(treeLines);
    return treeLines;
}

std::string formatTabEntry(
    int index,
    const TabEntry& entry,
    const std::string& displayIdentifier,
    int displayLink,
    int idWidth
) {
    std::ostringstream out;
    out << std::left
        << std::setw(5) << index
        << std::setw(idWidth) << displayIdentifier
        << std::setw(11) << objectNameFromCode(entry.obj)
        << std::right
        << std::setw(5) << entry.type
        << std::setw(5) << entry.ref
        << std::setw(5) << entry.nrm
        << std::setw(5) << entry.lev
        << std::setw(5) << entry.adr
        << std::setw(6) << displayLink;
    return out.str();
}

std::string formatBTabEntry(int index, const BTabEntry& entry, int displayLast, int displayLpar) {
    std::ostringstream out;
    out << std::left
        << std::setw(4) << index
        << std::right
        << std::setw(6) << displayLast
        << std::setw(6) << displayLpar
        << std::setw(6) << entry.psze
        << std::setw(6) << entry.vsze;
    return out.str();
}

std::string formatATabEntry(int index, const ATabEntry& entry) {
    std::ostringstream out;
    out << std::left
        << std::setw(4) << index
        << std::right
        << std::setw(6) << entry.xtyp
        << std::setw(6) << entry.etyp
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

    const int annotatedIndex = tabIndex != -1 ? tabIndex : node->tabIndex;
    if (annotatedIndex > 0 && annotatedIndex < static_cast<int>(symbolTable.tab().size())) {
        node->lexLevel = symbolTable.tabAt(annotatedIndex).lev;
    } else {
        node->lexLevel = symbolTable.currentLevel();
    }

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
    lines.push_back("Decorated AST:");

    if (rootAst == nullptr) {
        lines.push_back("<empty>");
        return lines;
    }

    const TabDisplayInfo tabDisplay = buildTabDisplayInfo(symbolTable.tab(), rootAst.get());
    std::vector<std::string> astLines = formatTreeLines(
        rootAst->toString(0),
        tabDisplay.displayIndexByInternal
    );
    lines.insert(lines.end(), astLines.begin(), astLines.end());
    return lines;
}

std::vector<std::string> SemanticAnalyzer::formatSymbolTables() const {
    std::vector<std::string> lines;
    lines.push_back("tab (relevant entries only):");
    const TabDisplayInfo tabDisplay = buildTabDisplayInfo(symbolTable.tab(), rootAst.get());

    std::ostringstream header;
    header << std::left
           << std::setw(5) << "idx"
           << std::setw(tabDisplay.idWidth) << "id"
           << std::setw(11) << "obj"
           << std::right
           << std::setw(5) << "type"
           << std::setw(5) << "ref"
           << std::setw(5) << "nrm"
           << std::setw(5) << "lev"
           << std::setw(5) << "adr"
           << std::setw(6) << "link";
    lines.push_back(header.str());
    lines.push_back(std::string(header.str().size(), '-'));
    lines.push_back("...  (reserved words 0-32)");

    const std::vector<TabEntry>& tab = symbolTable.tab();
    for (int internalIndex : tabDisplay.visibleIndices) {
        const TabEntry& entry = tab[static_cast<size_t>(internalIndex)];
        const int displayIndex = toDisplayIndex(tabDisplay.displayIndexByInternal, internalIndex);
        const int displayLink = displayLinkForEntry(tab, tabDisplay.displayIndexByInternal, entry);
        const std::string displayIdentifier =
            entry.obj == OBJ_PROCEDURE && entry.lev == 0 && entry.ref == 0
                ? text_util::lowercase(entry.identifier)
                : entry.identifier;

        lines.push_back(formatTabEntry(
            displayIndex,
            entry,
            displayIdentifier,
            displayLink,
            tabDisplay.idWidth
        ));
    }

    lines.push_back("");
    lines.push_back("btab:");
    std::ostringstream btabHeader;
    btabHeader << std::left
               << std::setw(4) << "idx"
               << std::right
               << std::setw(6) << "last"
               << std::setw(6) << "lpar"
               << std::setw(6) << "psze"
               << std::setw(6) << "vsze";
    lines.push_back(btabHeader.str());
    lines.push_back(std::string(btabHeader.str().size(), '-'));

    const std::vector<BTabEntry>& btab = symbolTable.btab();
    for (size_t i = 0; i < btab.size(); ++i) {
        const int displayLast = toDisplayIndex(tabDisplay.displayIndexByInternal, btab[i].last);
        const int displayLpar = toDisplayIndex(tabDisplay.displayIndexByInternal, btab[i].lpar);
        lines.push_back(formatBTabEntry(static_cast<int>(i), btab[i], displayLast, displayLpar));
    }

    lines.push_back("");

    const std::vector<ATabEntry>& atab = symbolTable.atab();
    if (atab.size() <= 1) {
        lines.push_back("atab: (empty; no arrays declared)");
    } else {
        lines.push_back("atab:");
        lines.push_back("idx   xtyp  etyp  eref   low  high  elsz  size");
        lines.push_back("-----------------------------------------------");
        for (size_t i = 1; i < atab.size(); ++i) {
            lines.push_back(formatATabEntry(static_cast<int>(i), atab[i]));
        }
    }

    return lines;
}

std::unique_ptr<ProgramNode> SemanticAnalyzer::buildASTFromParseTree(const ParseNode& root) {
    return buildAST(root);
}
