#include "symbol_table.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string normalizeIdentifier(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

TypeInfo makeType(int code, const std::string& name) {
    TypeInfo type;
    type.code = code;
    type.baseType = code;
    type.isNamed = true;
    type.name = name;
    return type;
}

TabEntry makeReserved(const std::string& name) {
    TabEntry entry;
    entry.identifier = name;
    entry.obj = OBJ_RESERVED;
    entry.type = TYPE_VOID;
    entry.typeInfo = makeType(TYPE_VOID, "void");
    return entry;
}

TabEntry makePredefinedType(const std::string& name, int typeCode) {
    TabEntry entry;
    entry.identifier = name;
    entry.obj = OBJ_TYPE;
    entry.type = typeCode;
    entry.typeInfo = makeType(typeCode, name);
    return entry;
}

TabEntry makePredefinedConstant(const std::string& name, int typeCode, int value) {
    TabEntry entry;
    entry.identifier = name;
    entry.obj = OBJ_CONSTANT;
    entry.type = typeCode;
    entry.typeInfo = makeType(typeCode, name);
    entry.adr = value;
    return entry;
}

TabEntry makePredefinedProcedure(const std::string& name) {
    TabEntry entry;
    entry.identifier = name;
    entry.obj = OBJ_PROCEDURE;
    entry.type = TYPE_VOID;
    entry.typeInfo = makeType(TYPE_VOID, "void");
    return entry;
}

std::string objectName(int obj) {
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

int primitiveTypeSize(int typeCode) {
    switch (typeCode) {
        case TYPE_INTEGER:
        case TYPE_REAL:
        case TYPE_CHAR:
        case TYPE_BOOLEAN:
        case TYPE_STRING:
        case TYPE_SUBRANGE:
        case TYPE_ENUM:
            return 1;
        default:
            return 0;
    }
}

}

void SymbolTable::init() {
    clear();
    initReservedWords();
    initPredefinedIdentifiers();
}

void SymbolTable::clear() {
    tabEntries.clear();
    btabEntries.clear();
    atabEntries.clear();
    display.clear();

    tabEntries.push_back(TabEntry{});
    btabEntries.push_back(BTabEntry{});
    atabEntries.push_back(ATabEntry{});
    display.push_back(0);
}

int SymbolTable::enterTab(const TabEntry& entry) {
    TabEntry stored = entry;

    stored.lev = currentLevel();

    if (stored.typeInfo.code != TYPE_VOID || stored.type == TYPE_VOID) {
        stored.type = stored.typeInfo.code;
    } else {
        stored.typeInfo.code = stored.type;
    }

    if (stored.typeInfo.baseType == TYPE_VOID && stored.type != TYPE_VOID) {
        stored.typeInfo.baseType = stored.type;
    }

    if (stored.ref == 0 && stored.typeInfo.ref != 0) {
        stored.ref = stored.typeInfo.ref;
    }

    if (!btabEntries.empty()) {
        const int block = currentBlock();
        stored.link = btabEntries[static_cast<size_t>(block)].last;
    }

    tabEntries.push_back(stored);
    const int index = static_cast<int>(tabEntries.size()) - 1;

    if (!btabEntries.empty()) {
        btabEntries[static_cast<size_t>(currentBlock())].last = index;
    }

    return index;
}

int SymbolTable::enterATab(const ATabEntry& entry) {
    ATabEntry stored = entry;

    if (stored.elsz <= 0) {
        stored.elsz = primitiveTypeSize(stored.etyp);
    }

    if (stored.size <= 0 && stored.high >= stored.low && stored.elsz > 0) {
        stored.size = (stored.high - stored.low + 1) * stored.elsz;
    }

    atabEntries.push_back(stored);
    return static_cast<int>(atabEntries.size()) - 1;
}

int SymbolTable::lookupTab(const std::string& name) const {
    if (!display.empty() && !btabEntries.empty()) {
        for (auto it = display.rbegin(); it != display.rend(); ++it) {
            const int found = lookupTab(name, *it);
            if (found != 0) {
                return found;
            }
        }
    }

    const std::string target = normalizeIdentifier(name);
    for (int i = static_cast<int>(tabEntries.size()) - 1; i > 0; --i) {
        if (normalizeIdentifier(tabEntries[static_cast<size_t>(i)].identifier) == target) {
            return i;
        }
    }

    return 0;
}

int SymbolTable::lookupTab(const std::string& name, int blockIdx) const {
    if (blockIdx < 0 || blockIdx >= static_cast<int>(btabEntries.size())) {
        return 0;
    }

    const std::string target = normalizeIdentifier(name);
    int index = btabEntries[static_cast<size_t>(blockIdx)].last;

    while (index > 0 && index < static_cast<int>(tabEntries.size())) {
        const TabEntry& entry = tabEntries[static_cast<size_t>(index)];
        if (normalizeIdentifier(entry.identifier) == target) {
            return index;
        }

        index = entry.link;
    }

    return 0;
}

int SymbolTable::lookupCurrentScope(const std::string& name) const {
    return lookupTab(name, currentBlock());
}

int SymbolTable::currentLevel() const {
    return display.empty() ? 0 : static_cast<int>(display.size()) - 1;
}

int SymbolTable::currentBlock() const {
    return display.empty() ? 0 : display.back();
}

const std::vector<int>& SymbolTable::currentDisplay() const {
    return display;
}

TabEntry& SymbolTable::tabAt(int index) {
    if (index <= 0 || index >= static_cast<int>(tabEntries.size())) {
        throw std::out_of_range("tab index out of range");
    }

    return tabEntries[static_cast<size_t>(index)];
}

const TabEntry& SymbolTable::tabAt(int index) const {
    if (index <= 0 || index >= static_cast<int>(tabEntries.size())) {
        throw std::out_of_range("tab index out of range");
    }

    return tabEntries[static_cast<size_t>(index)];
}

ATabEntry& SymbolTable::atabAt(int index) {
    if (index <= 0 || index >= static_cast<int>(atabEntries.size())) {
        throw std::out_of_range("atab index out of range");
    }

    return atabEntries[static_cast<size_t>(index)];
}

const ATabEntry& SymbolTable::atabAt(int index) const {
    if (index <= 0 || index >= static_cast<int>(atabEntries.size())) {
        throw std::out_of_range("atab index out of range");
    }

    return atabEntries[static_cast<size_t>(index)];
}

const std::vector<TabEntry>& SymbolTable::tab() const {
    return tabEntries;
}

const std::vector<ATabEntry>& SymbolTable::atab() const {
    return atabEntries;
}

TypeInfo SymbolTable::typeOf(int tabIndex) const {
    if (tabIndex <= 0 || tabIndex >= static_cast<int>(tabEntries.size())) {
        TypeInfo error;
        error.code = TYPE_ERROR;
        error.baseType = TYPE_ERROR;
        error.name = "error";
        return error;
    }

    return tabEntries[static_cast<size_t>(tabIndex)].typeInfo;
}

std::optional<TypeInfo> SymbolTable::predefinedType(const std::string& name) const {
    const int index = lookupTab(name);
    if (index == 0) {
        return std::nullopt;
    }

    const TabEntry& entry = tabEntries[static_cast<size_t>(index)];
    if (entry.obj != OBJ_TYPE) {
        return std::nullopt;
    }

    return entry.typeInfo;
}

std::string SymbolTable::typeName(const TypeInfo& type) const {
    if (!type.name.empty()) {
        return type.name;
    }

    switch (type.code) {
        case TYPE_VOID: return "Void";
        case TYPE_INTEGER: return "Integer";
        case TYPE_REAL: return "Real";
        case TYPE_CHAR: return "Char";
        case TYPE_BOOLEAN: return "Boolean";
        case TYPE_STRING: return "String";
        case TYPE_ARRAY: return "Array";
        case TYPE_RECORD: return "Record";
        case TYPE_SUBRANGE: return "Subrange";
        case TYPE_ENUM: return "Enum";
        case TYPE_ERROR: return "Error";
        default: return "Unknown";
    }
}

int SymbolTable::sizeOf(const TypeInfo& type) const {
    switch (type.code) {
        case TYPE_INTEGER:
        case TYPE_REAL:
        case TYPE_CHAR:
        case TYPE_BOOLEAN:
        case TYPE_STRING:
        case TYPE_SUBRANGE:
        case TYPE_ENUM:
            return 1;
        case TYPE_ARRAY:
            if (type.ref > 0 && type.ref < static_cast<int>(atabEntries.size())) {
                return atabEntries[static_cast<size_t>(type.ref)].size;
            }
            return 0;
        case TYPE_RECORD:
            return 0;
        default:
            return 0;
    }
}

std::vector<std::string> SymbolTable::formatTab() const {
    std::vector<std::string> lines;
    lines.push_back("tab:");
    lines.push_back("idx identifier       link obj        type       ref nrm lev adr");
    lines.push_back("---------------------------------------------------------------");

    for (int i = 1; i < static_cast<int>(tabEntries.size()); ++i) {
        const TabEntry& entry = tabEntries[static_cast<size_t>(i)];

        std::ostringstream row;
        row << std::setw(3) << i << " "
            << std::left << std::setw(16) << entry.identifier
            << std::right << std::setw(4) << entry.link << " "
            << std::left << std::setw(10) << objectName(entry.obj)
            << std::left << std::setw(11) << typeName(entry.typeInfo)
            << std::right << std::setw(4) << entry.ref
            << std::setw(4) << entry.nrm
            << std::setw(4) << entry.lev
            << std::setw(4) << entry.adr;

        lines.push_back(row.str());
    }

    return lines;
}

std::vector<std::string> SymbolTable::formatATab() const {
    std::vector<std::string> lines;
    lines.push_back("atab:");
    lines.push_back("idx xtyp       etyp       eref  low high elsz size");
    lines.push_back("---------------------------------------------------");

    for (int i = 1; i < static_cast<int>(atabEntries.size()); ++i) {
        const ATabEntry& entry = atabEntries[static_cast<size_t>(i)];

        TypeInfo indexType;
        indexType.code = entry.xtyp;
        indexType.baseType = entry.xtyp;

        TypeInfo elementType;
        elementType.code = entry.etyp;
        elementType.baseType = entry.etyp;

        std::ostringstream row;
        row << std::setw(3) << i << " "
            << std::left << std::setw(10) << typeName(indexType)
            << std::left << std::setw(10) << typeName(elementType)
            << std::right << std::setw(5) << entry.eref
            << std::setw(5) << entry.low
            << std::setw(5) << entry.high
            << std::setw(5) << entry.elsz
            << std::setw(5) << entry.size;

        lines.push_back(row.str());
    }

    return lines;
}

void SymbolTable::initReservedWords() {
    const char* reservedWords[] = {
        "AND", "ARRAY", "BEGIN", "CASE", "CONST", "DIV", "DOWNTO", "DO",
        "ELSE", "END", "FOR", "FUNCTION", "IF", "MOD", "NOT", "OF",
        "OR", "PROCEDURE", "PROGRAM", "RECORD", "REPEAT", "INTEGER",
        "REAL", "BOOLEAN", "CHAR", "STRING", "THEN", "TO", "TYPE",
        "UNTIL", "VAR", "WHILE"
    };

    for (const std::string word : reservedWords) {
        const std::string lowered = normalizeIdentifier(word);

        if (lowered == "integer") {
            enterTab(makePredefinedType("Integer", TYPE_INTEGER));
        } else if (lowered == "real") {
            enterTab(makePredefinedType("Real", TYPE_REAL));
        } else if (lowered == "boolean") {
            enterTab(makePredefinedType("Boolean", TYPE_BOOLEAN));
        } else if (lowered == "char") {
            enterTab(makePredefinedType("Char", TYPE_CHAR));
        } else if (lowered == "string") {
            enterTab(makePredefinedType("String", TYPE_STRING));
        } else {
            enterTab(makeReserved(word));
        }
    }
}

void SymbolTable::initPredefinedIdentifiers() {
    enterTab(makePredefinedConstant("True", TYPE_BOOLEAN, 1));
    enterTab(makePredefinedConstant("False", TYPE_BOOLEAN, 0));

    enterTab(makePredefinedProcedure("Read"));
    enterTab(makePredefinedProcedure("Readln"));
    enterTab(makePredefinedProcedure("Write"));
    enterTab(makePredefinedProcedure("Writeln"));
}
