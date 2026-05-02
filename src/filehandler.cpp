#include "filehandler.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

bool hasTxtExtension(const fs::path& filePath) {
    return filePath.extension() == ".txt";
}

}

std::vector<std::string> getTestFolders(const fs::path& testRoot) {
    std::vector<std::string> folders;

    if (!fs::exists(testRoot) || !fs::is_directory(testRoot)) {
        return folders;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(testRoot)) {
        if (entry.is_directory()) {
            folders.push_back(entry.path().filename().string());
        }
    }

    std::sort(folders.begin(), folders.end());
    return folders;
}

fs::path resolveTxtPath(const fs::path& filePath) {
    if (filePath.extension().empty()) {
        fs::path resolvedPath = filePath;
        resolvedPath += ".txt";
        return resolvedPath;
    }

    return filePath;
}

bool readInputFile(const fs::path& inputPath, std::string& source) {
    const fs::path resolvedInputPath = resolveTxtPath(inputPath);

    if (!hasTxtExtension(resolvedInputPath)) {
        std::cerr << "Error: file input harus berekstensi .txt\n";
        return false;
    }

    std::ifstream file(resolvedInputPath);
    if (!file.is_open()) {
        std::cerr << "Gagal membuka file input: " << resolvedInputPath.string() << "\n";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    source = buffer.str();
    return true;
}

bool writeOutputFile(const fs::path& outputPath, const std::vector<std::string>& lines) {
    const fs::path resolvedOutputPath = resolveTxtPath(outputPath);

    if (!hasTxtExtension(resolvedOutputPath)) {
        std::cerr << "Error: file output harus berekstensi .txt\n";
        return false;
    }

    fs::create_directories(resolvedOutputPath.parent_path());

    std::ofstream file(resolvedOutputPath);
    if (!file.is_open()) {
        std::cerr << "Gagal membuat file output: " << resolvedOutputPath.string() << "\n";
        return false;
    }

    for (const std::string& line : lines) {
        file << line << "\n";
    }

    return true;
}
