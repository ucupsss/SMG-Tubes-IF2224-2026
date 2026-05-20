#include "symbol_table.hpp"
#include "text_utils.hpp"

#include <stdexcept>

namespace {

std::string normalizeIdentifier(std::string name) {
    return text_util::lowercase(name);
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

int SymbolTable::enterBTab(const BTabEntry& entry) {
    btabEntries.push_back(entry);
    return static_cast<int>(btabEntries.size()) - 1;
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

void SymbolTable::pushScope(int btabIdx) {
    if (btabIdx < 0 || btabIdx >= static_cast<int>(btabEntries.size())) {
        throw std::out_of_range("btab index out of range");
    }

    display.push_back(btabIdx);
}

void SymbolTable::popScope() {
    if (display.size() <= 1) {
        return;
    }

    display.pop_back();
}

int SymbolTable::currentLevel() const {
    return display.empty() ? 0 : static_cast<int>(display.size()) - 1;
}

int SymbolTable::currentBlock() const {
    return display.empty() ? 0 : display.back();
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

BTabEntry& SymbolTable::btabAt(int index) {
    if (index < 0 || index >= static_cast<int>(btabEntries.size())) {
        throw std::out_of_range("btab index out of range");
    }

    return btabEntries[static_cast<size_t>(index)];
}

const BTabEntry& SymbolTable::btabAt(int index) const {
    if (index < 0 || index >= static_cast<int>(btabEntries.size())) {
        throw std::out_of_range("btab index out of range");
    }

    return btabEntries[static_cast<size_t>(index)];
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

const std::vector<BTabEntry>& SymbolTable::btab() const {
    return btabEntries;
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
            if (type.ref > 0 && type.ref < static_cast<int>(btabEntries.size())) {
                return btabEntries[static_cast<size_t>(type.ref)].vsze;
            }
            return 0;
        default:
            return 0;
    }
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
