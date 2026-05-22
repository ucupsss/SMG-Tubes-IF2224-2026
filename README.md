# Arion Compiler - Milestone 3

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

Program ini merupakan compiler sederhana untuk bahasa Arion yang sudah mencakup lexical analysis, syntax analysis, dan semantic analysis.

Pada milestone 3, program melakukan analisis semantik terhadap program Arion. Input akan diproses melalui lexer dan parser untuk membentuk parse tree, lalu parse tree tersebut dibangun menjadi AST dan dianotasi oleh semantic analyzer. Untuk kebutuhan integrasi milestone, semantic analyzer juga dapat menerima parse tree terformat dari keluaran milestone 2.

Keluaran utama milestone 3 meliputi:

- `tab`, yaitu tabel identifier untuk reserved words, predefined identifiers, program, deklarasi, variabel, konstanta, tipe, prosedur, dan fungsi.
- `btab`, yaitu tabel blok yang menyimpan informasi scope, parameter, dan ukuran variabel.
- `atab`, yaitu tabel array untuk menyimpan informasi indeks dan elemen array.
- Decorated AST, yaitu AST yang sudah diberi anotasi semantik seperti `tab_index`, `type`, dan `lev`.
- Semantic diagnostics, yaitu daftar error atau warning semantik.
- Status analisis, yaitu `SUCCESS` jika tidak ada error semantik dan `FAILED` jika ditemukan error.

Program melakukan pemeriksaan semantik sesuai kebutuhan milestone 3, antara lain:

- pengelolaan scope dan lexical level;
- deteksi redeklarasi identifier dalam scope yang sama;
- pengecekan identifier yang belum dideklarasikan;
- resolusi tipe primitif, named type, subrange, enum, array, dan record;
- pengecekan kompatibilitas tipe pada assignment dan ekspresi;
- validasi operator aritmetika, relasional, Boolean, unary, `div`, dan `mod`;
- validasi akses array dan field record;
- validasi pemanggilan procedure dan function, termasuk jumlah dan tipe argumen;
- validasi parameter by-value dan by-reference;
- validasi statement `if`, `while`, `repeat`, `for`, dan `case`;
- dukungan predefined identifiers seperti `True`, `False`, `Read`, `Readln`, `Write`, dan `Writeln`.

## Requirements

- G++ dengan dukungan C++17
- GNU Make

## Cara Instalasi dan Penggunaan Program

### 1. Build program

Jalankan perintah berikut dari root repository:

```bash
make
```

Executable akan dibuat di:

- Windows: `bin/windows/arion_compiler.exe`
- Linux/Mac: `bin/linux/arion_compiler`

### 2. Jalankan program

```bash
make run
```

Program akan menampilkan folder test yang tersedia:

- `milestone-1` untuk lexical analyzer;
- `milestone-2` untuk syntax analyzer;
- `milestone-3` untuk semantic analyzer.

### 3. Pilih input dan output

Untuk pengujian milestone 3:

1. Pilih folder `milestone-3`.
2. Masukkan nama file input dari `test/milestone-3/input`, misalnya `1.txt`.
3. Program akan menampilkan hasil semantic analyzer di terminal.
4. Masukkan nama file output tujuan di `test/milestone-3/output`, misalnya `1.txt`.

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
├── include/                 Header file program
├── src/                     Implementasi lexer, parser, AST, semantic analyzer, CLI, dan driver
├── test/
│   ├── milestone-1/         Test lexical analyzer
│   ├── milestone-2/         Test syntax analyzer
│   └── milestone-3/         Test semantic analyzer
├── doc/                     Laporan milestone sebelumnya
├── Makefile
└── README.md
```

Setiap folder milestone di dalam `test` memiliki struktur:

```text
test/milestone-x/
├── input/
└── output/
```

## Pembagian Tugas Milestone 3

| NIM | Nama | Tugas | Kontribusi |
| --- | --- | --- | --- |
| 13524014 | Yusuf Faishal Listyardi | Symbol Table, Semantic Declaration, dan Laporan| 25% |
| 13524046 | Farrel Limjaya | Semantic Expression, Semantic Statement, dan Laporan | 25% |
| 13524066 | Nathanael Gunawan | Implementasi Statement, Integrasi Statement, CLI, dan Laporan | 25% |
| 13524070 | A. Fawwaz Azam Wicaksono | Revisi Milestone 2, Implementasi AST, Test Case, dan Laporan | 25% |
