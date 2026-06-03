#include "driver.hpp"

#include "codegen.hpp"
#include "formatter.hpp"
#include "intermediate.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

std::string formatLexicalError(const Token& token) {
    return "Lexical error at line " +
           std::to_string(token.line) +
           ", column " +
           std::to_string(token.column) +
           ": " +
           token.value;
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string trimLeft(const std::string& text) {
    size_t pos = 0;
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        pos = 3;
    }

    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return text.substr(pos);
}

std::string trimRight(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

bool looksLikeFormattedParseTree(const std::string& source) {
    std::istringstream input(source);
    std::string line;

    while (std::getline(input, line)) {
        line = trimLeft(trimRight(line));
        if (!line.empty()) {
            return startsWith(line, "<program>");
        }
    }

    return false;
}

bool tokenTypeFromLabel(const std::string& label, TokenType& type) {
    static const std::vector<std::pair<std::string, TokenType>> tokenTypes = {
        {"intcon", TokenType::INTCON},
        {"realcon", TokenType::REALCON},
        {"charcon", TokenType::CHARCON},
        {"string", TokenType::STRING},
        {"notsy", TokenType::NOTSY},
        {"plus", TokenType::PLUS},
        {"minus", TokenType::MINUS},
        {"times", TokenType::TIMES},
        {"idiv", TokenType::IDIV},
        {"rdiv", TokenType::RDIV},
        {"imod", TokenType::IMOD},
        {"andsy", TokenType::ANDSY},
        {"orsy", TokenType::ORSY},
        {"eql", TokenType::EQL},
        {"neq", TokenType::NEQ},
        {"gtr", TokenType::GTR},
        {"geq", TokenType::GEQ},
        {"lss", TokenType::LSS},
        {"leq", TokenType::LEQ},
        {"lparent", TokenType::LPARENT},
        {"rparent", TokenType::RPARENT},
        {"lbrack", TokenType::LBRACK},
        {"rbrack", TokenType::RBRACK},
        {"comma", TokenType::COMMA},
        {"semicolon", TokenType::SEMICOLON},
        {"period", TokenType::PERIOD},
        {"colon", TokenType::COLON},
        {"becomes", TokenType::BECOMES},
        {"constsy", TokenType::CONSTSY},
        {"typesy", TokenType::TYPESY},
        {"varsy", TokenType::VARSY},
        {"functionsy", TokenType::FUNCTIONSY},
        {"proceduresy", TokenType::PROCEDURESY},
        {"arraysy", TokenType::ARRAYSY},
        {"recordsy", TokenType::RECORDSY},
        {"programsy", TokenType::PROGRAMSY},
        {"ident", TokenType::IDENT},
        {"beginsy", TokenType::BEGINSY},
        {"ifsy", TokenType::IFSY},
        {"casesy", TokenType::CASESY},
        {"repeatsy", TokenType::REPEATSY},
        {"whilesy", TokenType::WHILESY},
        {"forsy", TokenType::FORSY},
        {"endsy", TokenType::ENDSY},
        {"elsesy", TokenType::ELSESY},
        {"untilsy", TokenType::UNTILSY},
        {"ofsy", TokenType::OFSY},
        {"dosy", TokenType::DOSY},
        {"tosy", TokenType::TOSY},
        {"downtosy", TokenType::DOWNTOSY},
        {"thensy", TokenType::THENSY},
        {"comment", TokenType::COMMENT},
        {"unknown", TokenType::UNKNOWN},
        {"eof", TokenType::END_OF_FILE}
    };

    for (const auto& candidate : tokenTypes) {
        if (candidate.first == label) {
            type = candidate.second;
            return true;
        }
    }

    return false;
}

std::string defaultTokenValue(TokenType type) {
    switch (type) {
        case TokenType::NOTSY: return "not";
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::TIMES: return "*";
        case TokenType::IDIV: return "div";
        case TokenType::RDIV: return "/";
        case TokenType::IMOD: return "mod";
        case TokenType::ANDSY: return "and";
        case TokenType::ORSY: return "or";
        case TokenType::EQL: return "=";
        case TokenType::NEQ: return "<>";
        case TokenType::GTR: return ">";
        case TokenType::GEQ: return ">=";
        case TokenType::LSS: return "<";
        case TokenType::LEQ: return "<=";
        case TokenType::LPARENT: return "(";
        case TokenType::RPARENT: return ")";
        case TokenType::LBRACK: return "[";
        case TokenType::RBRACK: return "]";
        case TokenType::COMMA: return ",";
        case TokenType::SEMICOLON: return ";";
        case TokenType::PERIOD: return ".";
        case TokenType::COLON: return ":";
        case TokenType::BECOMES: return ":=";
        case TokenType::CONSTSY: return "const";
        case TokenType::TYPESY: return "type";
        case TokenType::VARSY: return "var";
        case TokenType::FUNCTIONSY: return "function";
        case TokenType::PROCEDURESY: return "procedure";
        case TokenType::ARRAYSY: return "array";
        case TokenType::RECORDSY: return "record";
        case TokenType::PROGRAMSY: return "program";
        case TokenType::BEGINSY: return "begin";
        case TokenType::IFSY: return "if";
        case TokenType::CASESY: return "case";
        case TokenType::REPEATSY: return "repeat";
        case TokenType::WHILESY: return "while";
        case TokenType::FORSY: return "for";
        case TokenType::ENDSY: return "end";
        case TokenType::ELSESY: return "else";
        case TokenType::UNTILSY: return "until";
        case TokenType::OFSY: return "of";
        case TokenType::DOSY: return "do";
        case TokenType::TOSY: return "to";
        case TokenType::DOWNTOSY: return "downto";
        case TokenType::THENSY: return "then";
        default: return "";
    }
}

std::string unquoteTreeTokenValue(const std::string& value) {
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

ParseNode parseTreeLabelToNode(const std::string& label, int lineNumber) {
    const size_t open = label.find('(');
    const size_t close = label.rfind(')');
    const std::string tokenLabel = open == std::string::npos ? label : label.substr(0, open);

    TokenType type;
    if (!tokenTypeFromLabel(tokenLabel, type)) {
        return ParseNode(label);
    }

    std::string value = defaultTokenValue(type);
    if (open != std::string::npos && close != std::string::npos && close > open) {
        value = unquoteTreeTokenValue(label.substr(open + 1, close - open - 1));
    }

    return ParseNode(label, Token{type, value, lineNumber, 1});
}

bool parseTreeLine(const std::string& rawLine, int& level, std::string& label) {
    static const std::string branch = "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ";
    static const std::string lastBranch = "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 ";

    std::string line = trimRight(rawLine);
    if (line.empty()) {
        return false;
    }

    std::string trimmed = trimLeft(line);
    if (startsWith(trimmed, "<program>")) {
        level = 0;
        label = trimmed;
        return true;
    }

    size_t branchPos = line.find(branch);
    size_t markerSize = branch.size();
    if (branchPos == std::string::npos) {
        branchPos = line.find(lastBranch);
        markerSize = lastBranch.size();
    }
    if (branchPos == std::string::npos) {
        branchPos = line.find("|-- ");
        markerSize = 4;
    }
    if (branchPos == std::string::npos) {
        branchPos = line.find("`-- ");
        markerSize = 4;
    }
    if (branchPos == std::string::npos) {
        throw std::runtime_error("invalid parse tree line: " + line);
    }

    int prefixWidth = 0;
    size_t pos = line.size() >= 3 &&
                 static_cast<unsigned char>(line[0]) == 0xEF &&
                 static_cast<unsigned char>(line[1]) == 0xBB &&
                 static_cast<unsigned char>(line[2]) == 0xBF
        ? 3
        : 0;

    while (pos < branchPos) {
        const unsigned char byte = static_cast<unsigned char>(line[pos]);
        if ((byte & 0x80) == 0) {
            ++pos;
        } else if ((byte & 0xE0) == 0xC0) {
            pos += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            pos += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            pos += 4;
        } else {
            ++pos;
        }

        ++prefixWidth;
    }

    level = prefixWidth / 4 + 1;
    label = line.substr(branchPos + markerSize);
    return true;
}

ParseNode parseFormattedParseTree(const std::string& source) {
    std::istringstream input(source);
    std::string line;
    ParseNode root("<empty>");
    std::vector<ParseNode*> stack;
    bool hasRoot = false;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;

        int level = 0;
        std::string label;
        if (!parseTreeLine(line, level, label)) {
            continue;
        }

        ParseNode node = parseTreeLabelToNode(label, lineNumber);
        if (!hasRoot) {
            if (level != 0) {
                throw std::runtime_error("parse tree must start with <program>");
            }
            root = node;
            stack.clear();
            stack.push_back(&root);
            hasRoot = true;
            continue;
        }

        if (level <= 0 || level > static_cast<int>(stack.size())) {
            throw std::runtime_error("invalid parse tree indentation near: " + label);
        }

        while (static_cast<int>(stack.size()) > level) {
            stack.pop_back();
        }

        stack.back()->children.push_back(node);
        stack.push_back(&stack.back()->children.back());
    }

    if (!hasRoot) {
        throw std::runtime_error("empty parse tree input");
    }

    return root;
}

ParseNode parseSourceToTree(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens;

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::UNKNOWN) {
            throw std::runtime_error(formatLexicalError(token));
        }

        if (token.type != TokenType::COMMENT) {
            tokens.push_back(token);
        }

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }

    Parser parser(tokens);
    return parser.parse();
}

std::string formatCodegenDiagnostic(const CodegenDiagnostic& diagnostic) {
    std::string message = "Error";
    if (diagnostic.line >= 0) {
        message += " at line " + std::to_string(diagnostic.line);

        if (diagnostic.column >= 0) {
            message += ", column " + std::to_string(diagnostic.column);
        }
    }

    message += ": " + diagnostic.message;
    return message;
}

std::string formatRuntimeDiagnostic(const RuntimeDiagnostic& diagnostic) {
    if (!diagnostic.message.empty()) {
        return diagnostic.message;
    }

    if (diagnostic.instructionPointer >= 0) {
        return "Runtime error at instruction " +
               std::to_string(diagnostic.instructionPointer);
    }

    return "Runtime error";
}

}

std::vector<std::string> runLexer(const std::string& source) {
    Lexer lexer(source);
    std::vector<std::string> outputLines;

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }

        outputLines.push_back(formatToken(token));
    }

    return outputLines;
}

std::vector<std::string> runSyntaxAnalyzer(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens;

    while (true) {
        Token token = lexer.getNextToken();

        if (token.type == TokenType::UNKNOWN) {
            return {formatLexicalError(token)};
        }

        if (token.type != TokenType::COMMENT) {
            tokens.push_back(token);
        }

        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }

    try {
        Parser parser(tokens);
        ParseNode parseTree = parser.parse();
        return formatParseTree(parseTree);
    } catch (const ParseError& error) {
        return {error.what()};
    }
}

std::vector<std::string> runSemanticAnalyzer(const std::string& source) {
    try {
        ParseNode parseTree = looksLikeFormattedParseTree(source)
            ? parseFormattedParseTree(source)
            : parseSourceToTree(source);

        SemanticAnalyzer analyzer;
        analyzer.analyze(parseTree);
        return analyzer.formatOutput();
    } catch (const ParseError& error) {
        return {error.what()};
    } catch (const std::runtime_error& error) {
        return {error.what()};
    }
}

std::vector<std::string> runIntermediateCodeInterpreter(const std::string& source) {
    try {
        ParseNode parseTree = looksLikeFormattedParseTree(source)
            ? parseFormattedParseTree(source)
            : parseSourceToTree(source);

        SemanticAnalyzer analyzer;
        analyzer.analyze(parseTree);

        std::vector<std::string> lines;
        if (analyzer.hasErrors()) {
            std::vector<std::string> semanticLines = analyzer.formatOutput();
            lines.insert(lines.end(), semanticLines.begin(), semanticLines.end());

            lines.push_back("");
            lines.push_back("Intermediate Code:");
            lines.push_back("<not generated because semantic analysis failed>");
            lines.push_back("");
            lines.push_back("Program Output:");
            lines.push_back("<not executed>");
            lines.push_back("");
            lines.push_back("Status: FAILED");
            return lines;
        }

        const ProgramNode* ast = analyzer.ast();
        if (ast == nullptr) {
            return {
                "Intermediate Code:",
                "<not generated because semantic analyzer did not produce an AST>",
                "",
                "Program Output:",
                "<not executed>",
                "",
                "Status: FAILED"
            };
        }

        CodeGenerator generator;
        CodeGenerationResult codegen = generator.generate(*ast, analyzer.symbols());

        lines.push_back("Intermediate Code:");
        std::vector<std::string> codeLines = formatProgram(codegen.program);
        if (codeLines.empty()) {
            lines.push_back("<empty>");
        } else {
            lines.insert(lines.end(), codeLines.begin(), codeLines.end());
        }

        lines.push_back("");
        lines.push_back("Codegen diagnostics:");
        if (codegen.diagnostics.empty()) {
            lines.push_back("No code generation errors.");
        } else {
            for (const CodegenDiagnostic& diagnostic : codegen.diagnostics) {
                lines.push_back(formatCodegenDiagnostic(diagnostic));
            }
        }

        if (!codegen.success()) {
            lines.push_back("");
            lines.push_back("Program Output:");
            lines.push_back("<not executed>");
            lines.push_back("");
            lines.push_back("Runtime diagnostics:");
            lines.push_back("<not executed because code generation failed>");
            lines.push_back("");
            lines.push_back("Status: FAILED");
            return lines;
        }

        StackMachineInterpreter interpreter;
        ExecutionResult execution = interpreter.execute(codegen.program);

        lines.push_back("");
        lines.push_back("Program Output:");
        if (execution.outputLines.empty()) {
            lines.push_back("<no output>");
        } else {
            lines.insert(lines.end(), execution.outputLines.begin(), execution.outputLines.end());
        }

        lines.push_back("");
        lines.push_back("Runtime diagnostics:");
        if (execution.diagnostics.empty()) {
            lines.push_back("No runtime errors.");
        } else {
            for (const RuntimeDiagnostic& diagnostic : execution.diagnostics) {
                lines.push_back(formatRuntimeDiagnostic(diagnostic));
            }
        }

        lines.push_back("");
        lines.push_back(std::string("Status: ") + (execution.success() ? "SUCCESS" : "FAILED"));
        return lines;
    } catch (const ParseError& error) {
        return {error.what()};
    } catch (const std::runtime_error& error) {
        return {error.what()};
    }
}
