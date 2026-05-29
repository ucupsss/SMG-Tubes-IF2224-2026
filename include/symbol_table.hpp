#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <string>
#include <vector>

inline constexpr int TYPE_VOID = 0;
inline constexpr int TYPE_INTEGER = 1;
inline constexpr int TYPE_REAL = 2;
inline constexpr int TYPE_CHAR = 3;
inline constexpr int TYPE_BOOLEAN = 4;
inline constexpr int TYPE_STRING = 5;
inline constexpr int TYPE_ARRAY = 6;
inline constexpr int TYPE_RECORD = 7;
inline constexpr int TYPE_SUBRANGE = 8;
inline constexpr int TYPE_ENUM = 9;
inline constexpr int TYPE_ERROR = -1;

inline constexpr int OBJ_CONSTANT = 0;
inline constexpr int OBJ_VARIABLE = 1;
inline constexpr int OBJ_TYPE = 2;
inline constexpr int OBJ_PROCEDURE = 3;
inline constexpr int OBJ_FUNCTION = 4;
inline constexpr int OBJ_PROGRAM = 5;
inline constexpr int OBJ_RESERVED = 6;

struct TypeInfo {
    int code = TYPE_VOID;
    int baseType = TYPE_VOID;
    int ref = 0;
    int low = 0;
    int high = 0;
    int stringLength = -1;
    bool isNamed = false;
    std::string name;
};

struct TabEntry {
    std::string identifier;
    int link = 0;
    int obj = OBJ_VARIABLE;
    int type = TYPE_VOID;
    int ref = 0;
    int nrm = 1;
    int lev = 0;
    int adr = 0;
    TypeInfo typeInfo;
    bool hasConstantOrdinal = false;
    int constantOrdinalValue = 0;
    bool hasConstantReal = false;
    double constantRealValue = 0.0;
    bool hasConstantString = false;
    std::string constantStringValue;
};

struct BTabEntry {
    int last = 0;
    int lpar = 0;
    int psze = 0;
    int vsze = 0;
};

struct ATabEntry {
    int xtyp = TYPE_VOID;
    int etyp = TYPE_VOID;
    int eref = 0;
    int low = 0;
    int high = 0;
    int elsz = 0;
    int size = 0;
};

class SymbolTable {
public:
    void init();
    void clear();

    int enterTab(const TabEntry& entry);
    int enterBTab(const BTabEntry& entry);
    int enterATab(const ATabEntry& entry);

    int lookupTab(const std::string& name) const;
    int lookupTab(const std::string& name, int blockIdx) const;
    int lookupCurrentScope(const std::string& name) const;

    void pushScope(int btabIdx);
    void popScope();

    int currentLevel() const;
    int currentBlock() const;

    TabEntry& tabAt(int index);
    const TabEntry& tabAt(int index) const;
    BTabEntry& btabAt(int index);
    const BTabEntry& btabAt(int index) const;
    ATabEntry& atabAt(int index);
    const ATabEntry& atabAt(int index) const;

    const std::vector<TabEntry>& tab() const;
    const std::vector<BTabEntry>& btab() const;
    const std::vector<ATabEntry>& atab() const;

    TypeInfo typeOf(int tabIndex) const;
    std::string typeName(const TypeInfo& type) const;
    int sizeOf(const TypeInfo& type) const;

private:
    void initReservedWords();
    void initPredefinedIdentifiers();

    std::vector<TabEntry> tabEntries;
    std::vector<BTabEntry> btabEntries;
    std::vector<ATabEntry> atabEntries;
    std::vector<int> display;
};

#endif
