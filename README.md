# Arion Compiler - Milestone 2

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

Program ini merupakan compiler sederhana untuk bahasa Arion pada tahap lexical analysis dan syntax analysis. Pada milestone 2, program membaca source code Arion dari file teks, menjalankannya melalui lexer untuk membentuk token, lalu memeriksa susunan token tersebut menggunakan parser Recursive Descent.

Keluaran utama milestone 2 adalah parse tree yang merepresentasikan struktur sintaks program sesuai grammar Arion. Jika input tidak valid, program akan menampilkan lexical error atau syntax error beserta lokasi kesalahannya.

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

Program akan menampilkan folder test yang tersedia, misalnya `milestone-1` dan `milestone-2`.

### 3. Pilih input dan output

Untuk pengujian milestone 2:

1. Pilih folder `milestone-2`.
2. Masukkan nama file input dari `test/milestone-2/input`, misalnya `1.txt`.
3. Program akan menampilkan hasil syntax analyzer di terminal.
4. Masukkan nama file output tujuan di `test/milestone-2/output`, misalnya `1.txt`.

Gunakan `0` untuk kembali dan `q` untuk keluar dari program.

### 4. Clean build

```bash
make clean
```

Untuk menghapus seluruh artifact build:

```bash
make clean-all
```

## Pembagian Tugas

| NIM | Nama | Tugas | Kontribusi |
| --- | --- | --- | --- |
| 13524014 | Yusuf Faishal Listyardi | Implementasi `parser.cpp`, `parser_declaration.cpp`, dan laporan | 25% |
| 13524046 | Farrel Limjaya | Implementasi `parser_statement.cpp`, `parser_expression.cpp`, dan laporan | 25% |
| 13524066 | Nathanael Gunawan | Integrasi parser ke CLI dan laporan | 25% |
| 13524070 | A. Fawwaz Azam Wicaksono | Revisi lexer, test case, dan laporan | 25% |
