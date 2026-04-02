# SMG-Tubes-IF2224-2026
Tugas Besar Teori Bahasa Formal dan Automata 2026

# Milestone 1 - Lexical Analysis (Arion Compiler)

## Identitas Kelompok
**Kelompok:** [Kode Kelompok Anda, misal: K01-KelompokX]
Anggota Kelompok:
1. 13524014 - Yusuf Faishal Listyardi
2. 13524046 -  Farrel Limjaya
3. 13524066 - Nathanael Gunawan
4. 13524070 - A. Fawwaz Azzam Wicaksono


---

## Deskripsi Program
Program ini adalah sebuah **Lexical Analyzer (Lexer)** yang dirancang untuk bahasa pemrograman Arion. Lexer ini dibangun menggunakan bahasa C++ (standar C++17) murni tanpa menggunakan library bantuan otomatisasi DFA. Program bekerja selayaknya *Deterministic Finite Automata* (DFA) murni, yaitu membaca *source code* karakter demi karakter untuk mengelompokkannya menjadi unit-unit bermakna yang disebut token. Program akan menerima masukan *source code* dalam format `.txt` dan menampilkan klasifikasi daftar token yang diurutkan.

---

## Requirements
Untuk melakukan kompilasi dan menjalankan program ini, pastikan sistem Anda telah memasang:
* **G++ Compiler** (Mendukung standar C++17)
* **GNU Make** (Untuk menjalankan skrip `Makefile`)

---

## Cara Instalasi dan Penggunaan Program

### 1. Kompilasi Program
Buka terminal dan arahkan ke dalam direktori *root* repositori (sejajar dengan file `Makefile`). Jalankan perintah berikut untuk mengompilasi *source code*:
```bash
make
```

### 2. Menjalankan Program
Untuk menjalankan program langsung setelah kompilasi:
```bash
make run
```

Atau jalankan langsung executable yang telah terbuat:

**Pada Windows:**
```bash
lexer_app.exe
```

**Pada Linux/Mac:**
```bash
./lexer_app
```

### 3. Memberikan Input
Program akan meminta masukan berupa nama file `.txt` yang berisi *source code* Arion. Pastikan file input terletak pada direktori yang benar. Contoh:
```
test/input/sample.txt
```

### 4. Deskripsi Output
Program akan menghasilkan file output berisi daftar token yang telah diidentifikasi dengan format:
- **Nomor Urut Token**
- **Jenis Token** (tipe token: intcon, realcon, ident, etc.)
- **Nilai Token** (nilai aktual dari token)

Output akan disimpan di folder `test/output/`.

### 5. Pembersihan File Kompilasi
Untuk menghapus file executable dan file hasil kompilasi:
```bash
make clean
```

---

## Pembagian Tugas

| No | Anggota | NIM | Tugas |
|---|---|---|---|
| 1 | Yusuf Faishal Listyardi | 13524014 | [Deskripsi Tugas] |
| 2 | Farrel Limjaya | 13524046 | [Deskripsi Tugas] |
| 3 | Nathanael Gunawan | 13524066 | [Deskripsi Tugas] |
| 4 | A. Fawwaz Azzam Wicaksono | 13524070 | [Deskripsi Tugas] |

---

## Struktur Direktori

```
SMG-Tubes-IF2224-2026/
├── Makefile              # Skrip untuk kompilasi
├── README.md             # File ini
├── bin/                  # Executable File
├── doc/                  # Dokumentasi 
├── src/                  # Folder Source Code
│   ├── main.cpp         # Program utama
│   ├── lexer.cpp        # Implementasi lexer
│   └── lexer.hpp        # Header file lexer
└── test/                # Folder Test
    ├── input/           # File input test
    └── output/          # File output hasil run
```

---

## Catatan Penting

- Program menggunakan **bahasa C++17** dan tidak bergantung pada library eksternal untuk automata.
- Lexer berfungsi sebagai **DFA murni** untuk mengenali token-token dalam bahasa pemrograman Arion.
- Pastikan file input menggunakan format teks biasa (`.txt`) dan path yang benar.