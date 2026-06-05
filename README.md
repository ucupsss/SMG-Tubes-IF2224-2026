# Arion Compiler - Milestone 4

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

Program ini merupakan compiler dan interpreter sederhana untuk bahasa Arion. Sampai Milestone 4, program sudah mencakup:

- lexical analysis;
- syntax analysis;
- AST construction;
- semantic analysis dan decorated AST;
- intermediate code generation;
- eksekusi intermediate code dengan stack machine interpreter.

Pada Milestone 4, source code Arion diproses dari lexer sampai semantic analyzer untuk menghasilkan decorated AST dan symbol table. Hasil tersebut kemudian digunakan oleh code generator untuk menghasilkan intermediate code linear. Intermediate code ini dijalankan oleh stack machine interpreter untuk menghasilkan output akhir program.

Keluaran utama Milestone 4 meliputi:

- semantic diagnostics;
- intermediate code;
- code generation diagnostics;
- program output;
- runtime diagnostics;
- status eksekusi.

## Fitur Milestone 4

### Intermediate Code

Program menghasilkan instruksi intermediate berbasis stack machine, antara lain:

- `INT` untuk alokasi frame;
- `LIT` untuk literal;
- `LOD` dan `STO` untuk load/store scalar;
- `LDA`, `LDI`, dan `STI` untuk alamat, indirect load, dan indirect store;
- `CPY` untuk copy array/record;
- `LDR` untuk load aggregate by-value sebagai argumen routine;
- `CAL` dan `RET` untuk pemanggilan dan return procedure/function;
- `JMP` dan `JPC` untuk control flow;
- `OPR` untuk operasi aritmetika, relasional, Boolean, dan output;
- `INP` dan `INL` untuk `read` dan `readln`.

### Stack Machine Interpreter

Interpreter menjalankan intermediate code dengan model frame stack yang memuat:

- static link;
- dynamic link;
- return address;
- parameter;
- variabel lokal;
- slot return value untuk function scalar.

Interpreter juga memiliki runtime diagnostics untuk error seperti division by zero, stack underflow, invalid jump target, akses memori di luar batas, dan input yang tidak sesuai tipe target.

### Statement dan Ekspresi yang Didukung

Implementasi Milestone 4 mendukung:

- assignment scalar;
- assignment array/record dengan copy aggregate;
- ekspresi aritmetika `+`, `-`, `*`, `/`, `div`, dan `mod`;
- ekspresi relasional `==`, `<>`, `<`, `<=`, `>`, dan `>=`;
- ekspresi Boolean `and`, `or`, dan `not`;
- short-circuit evaluation untuk `and` dan `or`;
- `if-else`;
- `while`;
- `repeat-until`;
- `for-to` dan `for-downto`;
- `case`;
- procedure dan function call;
- parameter by-value dan by-reference;
- nested routine dengan static link;
- akses array dan record, termasuk array of record;
- `write`, `writeln`, `read`, dan `readln`.

### Array, Record, dan Parameter

Array dan record direpresentasikan sebagai beberapa slot memori berurutan. Implementasi mendukung:

- read/write field record scalar;
- read/write element array scalar;
- assignment antar array/record kompatibel;
- copy array/record sebagai parameter by-value;
- passing array/record by-reference.

Untuk menjaga kontrak backend tetap jelas, compiler menolak beberapa konteks structured value yang tidak dieksekusi sebagai scalar:

- function dengan return type array atau record;
- `write` atau `writeln` terhadap whole array/record;
- `read` atau `readln` terhadap whole array/record.

Kasus tersebut dihentikan di semantic analysis dengan diagnostic yang eksplisit.

## Requirements

- G++ dengan dukungan C++17
- GNU Make

## Cara Instalasi dan Penggunaan Program

### 1. Build program

Jalankan perintah berikut dari root repository:

```bash
make
```

atau:

```bash
make build
```

Executable akan dibuat di:

- Windows: `bin/windows/arion_compiler.exe`
- Linux/Mac: `bin/linux/arion_compiler`

### 2. Jalankan program

```bash
make run
```

Pada Windows, executable juga dapat dijalankan langsung:

```powershell
.\bin\windows\arion_compiler.exe
```

Program akan menampilkan folder test yang tersedia:

- `milestone-1` untuk lexical analyzer;
- `milestone-2` untuk syntax analyzer;
- `milestone-3` untuk semantic analyzer;
- `milestone-4` untuk intermediate code dan interpreter.

### 3. Pilih input dan output

Untuk pengujian Milestone 4:

1. Pilih folder `milestone-4`.
2. Masukkan nama file input dari `test/milestone-4/input`, misalnya `1.txt`.
3. Jika program Arion membutuhkan input runtime, masukkan input ketika diminta oleh interpreter.
4. Program akan menampilkan semantic diagnostics, intermediate code, codegen diagnostics, program output, runtime diagnostics, dan status.
5. Masukkan nama file output tujuan di `test/milestone-4/output`, misalnya `1.txt`.

Gunakan `0` untuk kembali dan `q` untuk keluar dari program.

### 4. Clean build

```bash
make clean
```

Untuk menghapus seluruh artifact build:

```bash
make clean-all
```

## Struktur Direktori

```text
.
|-- include/                 Header file program
|-- src/                     Implementasi lexer, parser, AST, semantic analyzer, codegen, interpreter, CLI, dan driver
|-- test/
|   |-- milestone-1/         Test lexical analyzer
|   |-- milestone-2/         Test syntax analyzer
|   |-- milestone-3/         Test semantic analyzer
|   `-- milestone-4/         Test intermediate code dan interpreter
|-- doc/                     Laporan milestone
|-- Makefile
`-- README.md
```

Setiap folder milestone di dalam `test` memiliki struktur:

```text
test/milestone-x/
|-- input/
`-- output/
```

## Pengujian Milestone 4

Test Milestone 4 berada di:

```text
test/milestone-4/input/
test/milestone-4/output/
```

Cakupan test meliputi:

- ekspresi dan assignment scalar;
- control flow;
- procedure/function call;
- nested routine dan lexical scope;
- parameter by-value dan by-reference;
- akses array dan record;
- aggregate assignment;
- real division;
- input `read/readln`;
- error handling runtime seperti division by zero;
- semantic rejection untuk konteks structured value yang tidak didukung.

Catatan pengujian:

- `8.txt` dan `20.txt` sengaja menghasilkan runtime error division by zero.
- `30.txt` sengaja menghasilkan semantic error untuk return array/record dan I/O whole record.

## Pembagian Tugas Milestone 4

Rincian kontribusi resmi dicantumkan pada laporan Milestone 4. Ringkasan area implementasi:

| NIM | Nama | Area |
| --- | --- | --- |
| 13524014 | Yusuf Faishal Listyardi | Intermediate representation, symbol table integration, dan dokumentasi |
| 13524046 | Farrel Limjaya | Expression code generation, operation handling, dan runtime behavior |
| 13524066 | Nathanael Gunawan | Statement code generation, control flow, routine frame, dan CLI integration |
| 13524070 | A. Fawwaz Azam Wicaksono | Aggregate handling, input procedures, hardening, test case, dan README |
