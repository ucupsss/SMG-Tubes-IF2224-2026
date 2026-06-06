# Arion Compiler

Tugas Besar IF2224 Teori Bahasa Formal dan Otomata 2026.

## Identitas Kelompok

**Kelompok:** SMG

| NIM | Nama |
| --- | --- |
| 13524014 | Yusuf Faishal Listyardi |
| 13524046 | Farrel Limjaya |
| 13524066 | Nathanael Gunawan |
| 13524070 | A. Fawwaz Azam Wicaksono |

## Deskripsi Program

Arion Compiler adalah compiler dan interpreter sederhana untuk bahasa Arion. Program memproses source code melalui lexer, parser, AST builder, semantic analyzer, intermediate code generator, lalu menjalankan intermediate code dengan stack machine interpreter.

Keluaran program disesuaikan dengan folder milestone yang dipilih:

- `milestone-1`: hasil lexical analysis.
- `milestone-2`: parse tree dari syntax analysis.
- `milestone-3`: semantic diagnostics, decorated AST, dan symbol table.
- `milestone-4`: semantic diagnostics, intermediate code, program output, runtime diagnostics, dan status eksekusi.

Pada tahap akhir, program mendukung control flow, procedure/function, nested routine, parameter by-value/by-reference, array, record, aggregate copy, `read/readln`, `write/writeln`, serta runtime error handling seperti division by zero, invalid input, out-of-bounds access, dan stack overflow.

## Requirements

- G++ dengan dukungan C++17
- GNU Make

## Cara Instalasi dan Penggunaan

Build program dari root repository:

```bash
make
```

Executable akan dibuat di:

- Windows: `bin/windows/arion_compiler.exe`
- Linux/Mac: `bin/linux/arion_compiler`

Jalankan program:

```bash
make run
```

Pada Windows, executable juga dapat dijalankan langsung:

```powershell
.\bin\windows\arion_compiler.exe
```

Alur penggunaan:

1. Pilih folder test, misalnya `milestone-4`.
2. Masukkan nama file input dari `test/<milestone>/input`, misalnya `1.txt`.
3. Jika program membutuhkan input runtime, masukkan nilai saat diminta.
4. Lihat hasil analisis/eksekusi di terminal.
5. Masukkan nama file output tujuan di `test/<milestone>/output`.

Gunakan `0` untuk kembali dan `q` untuk keluar.

Clean build:

```bash
make clean
```

Hapus seluruh artifact build:

```bash
make clean-all
```

## Struktur Direktori

```text
.
├── include/                 Header program
├── src/                     Implementasi compiler, interpreter, CLI, dan driver
├── test/
│   ├── milestone-1/         Test lexical analyzer
│   ├── milestone-2/         Test syntax analyzer
│   ├── milestone-3/         Test semantic analyzer
│   └── milestone-4/         Test intermediate code dan interpreter
├── doc/                     Laporan milestone
├── Makefile
└── README.md
```

Setiap folder milestone memiliki struktur:

```text
test/milestone-x/
├── input/
└── output/
```

## Pengujian

Test Milestone 4 berada di:

```text
test/milestone-4/input/
test/milestone-4/output/
```

Tersedia 24 test case yang mencakup eksekusi valid dan error handling.

| No | Fokus Pengujian | Status |
| --- | --- | --- |
| 1 | Program dasar, assignment, ekspresi aritmetika, `writeln` | Success |
| 2 | `while`, `if-else`, dan akumulasi nilai | Success |
| 3 | `for-to`, `repeat-until`, dan `case` | Success |
| 4 | `for-downto` | Success |
| 5 | Short-circuit Boolean `or` | Success |
| 6 | Runtime error: integer division by zero | Failed |
| 7 | Nested routine dan static link | Success |
| 8 | Function return dan parameter by-reference | Success |
| 9 | Array scalar access dan assignment elemen | Success |
| 10 | Array of record dan akses field bertingkat | Success |
| 11 | Parameter by-reference ke elemen array | Success |
| 12 | Array dengan indeks ordinal `char` | Success |
| 13 | Real division dan operasi campuran integer-real | Success |
| 14 | Assignment/copy array | Success |
| 15 | Copy elemen aggregate array of record | Success |
| 16 | Copy aggregate melalui parameter by-reference | Success |
| 17 | `read`, `readln`, dan input scalar berbagai tipe | Success |
| 18 | Input ke target terstruktur scalar: elemen array dan field record | Success |
| 19 | Runtime error: input tidak sesuai tipe target | Failed |
| 20 | Parameter record by-value | Success |
| 21 | Parameter array by-value | Success |
| 22 | Semantic error: structured value pada konteks yang tidak didukung | Failed |
| 23 | Runtime error: array index out of bounds | Failed |
| 24 | Runtime error: stack overflow dari recursive call | Failed |

## Pembagian Tugas

Kontribusi dibagi rata sebesar 25% untuk setiap anggota pada tiap milestone.

| NIM | Nama | Milestone 1 | Milestone 2 | Milestone 3 | Milestone 4 |
| --- | --- | --- | --- | --- | --- |
| 13524014 | Yusuf Faishal Listyardi | DFA, implementasi program, laporan | `parser.cpp`, `parser_declaration.cpp`, laporan | Symbol table, semantic declaration, laporan | Codegen declaration dan integrasi |
| 13524046 | Farrel Limjaya | DFA, implementasi program, laporan | `parser_statement.cpp`, `parser_expression.cpp`, laporan | Semantic expression, semantic statement, laporan | Codegen statement dan codegen core |
| 13524066 | Nathanael Gunawan | DFA, implementasi program, laporan | Integrasi parser ke CLI, laporan | Implementasi statement, integrasi statement, CLI, laporan | Interpreter |
| 13524070 | A. Fawwaz Azam Wicaksono | DFA, implementasi program, laporan | Revisi lexer, test case, laporan | Revisi Milestone 2, AST, test case, laporan | Intermediate code dan codegen expression |
