#include "filehandler.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

bool hasTxtExtension(const fs::path& filePath) {
    return filePath.extension() == ".txt";
}

}

bool readInputFile(const fs::path& inputPath, std::string& source) {
    if (!hasTxtExtension(inputPath)) {
        std::cerr << "Error: file input harus berekstensi .txt\n";
        return false;
    }

    std::ifstream file(inputPath);
    if (!file.is_open()) {
        std::cerr << "Gagal membuka file input: " << inputPath.string() << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    source = buffer.str();
    return true;
}

bool writeOutputFile(const fs::path& outputPath, const std::vector<std::string>& lines) {
    if (!hasTxtExtension(outputPath)) {
        std::cerr << "Error: file output harus berekstensi .txt\n";
        return false;
    }

    fs::create_directories(outputPath.parent_path());

    std::ofstream file(outputPath);
    if (!file.is_open()) {
        std::cerr << "Gagal membuat file output: " << outputPath.string() << "\n";
        return false;
    }

    for (const std::string& line : lines) {
        file << line << "\n";
    }

    return true;
}
