#include "cli.hpp"

#include "driver.hpp"
#include "filehandler.hpp"

#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const fs::path TEST_ROOT = "test";

enum class PromptResult {
    Success,
    Cancel,
    Quit,
    FatalError
};

void printTestFolders(const std::vector<std::string>& folders) {
    std::cout << "\n\nFolder test tersedia:\n";
    std::cout << "0. Keluar\n";
    for (size_t i = 0; i < folders.size(); ++i) {
        std::cout << i + 1 << ". " << folders[i] << "\n";
    }
}

bool isQuitCommand(const std::string& input) {
    return input == "q" || input == "Q" || input == "quit" || input == "QUIT";
}

bool isCancelCommand(const std::string& input) {
    return input == "0";
}

PromptResult readPrompt(const std::string& prompt, std::string& input) {
    std::cout << prompt;

    if (!std::getline(std::cin, input)) {
        return PromptResult::Quit;
    }

    if (isQuitCommand(input)) {
        return PromptResult::Quit;
    }

    if (isCancelCommand(input)) {
        return PromptResult::Cancel;
    }

    return PromptResult::Success;
}

bool parseFolderNumber(const std::string& input, size_t& number) {
    if (input.empty()) {
        return false;
    }

    number = 0;
    for (char c : input) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }

        number = (number * 10) + static_cast<size_t>(c - '0');
    }

    return number > 0;
}

bool selectFolderByInput(
    const std::vector<std::string>& folders,
    const std::string& input,
    std::string& selectedFolder
) {
    size_t folderNumber = 0;
    if (parseFolderNumber(input, folderNumber)) {
        if (folderNumber <= folders.size()) {
            selectedFolder = folders[folderNumber - 1];
            return true;
        }

        return false;
    }

    for (const std::string& folder : folders) {
        if (folder == input) {
            selectedFolder = folder;
            return true;
        }
    }

    return false;
}

PromptResult promptTestFolder(std::string& selectedFolder) {
    while (true) {
        const std::vector<std::string> testFolders = getTestFolders(TEST_ROOT);

        if (testFolders.empty()) {
            std::cerr << "Error: test folder not found.\n";
            return PromptResult::FatalError;
        }

        printTestFolders(testFolders);

        std::string input;
        PromptResult promptResult = readPrompt("Pilih nomor folder test (0/q untuk keluar): ", input);

        if (promptResult == PromptResult::Quit || promptResult == PromptResult::Cancel) {
            return PromptResult::Quit;
        }

        if (input.empty()) {
            std::cerr << "Error: test folder choice cannot be empty.\n";
            continue;
        }

        if (!selectFolderByInput(testFolders, input, selectedFolder)) {
            std::cerr << "Error: invalid test folder '" << input << "'.\n";
            continue;
        }

        return PromptResult::Success;
    }
}

PromptResult promptInputSource(const std::string& selectedFolder, std::string& source) {
    while (true) {
        std::string inputFileName;
        const std::string prompt = "\nMasukkan nama file input dari test/" +
                                   selectedFolder +
                                   "/input (0 untuk kembali, q untuk keluar): ";

        PromptResult promptResult = readPrompt(prompt, inputFileName);

        if (promptResult == PromptResult::Quit) {
            return PromptResult::Quit;
        }

        if (promptResult == PromptResult::Cancel) {
            return PromptResult::Cancel;
        }

        if (inputFileName.empty()) {
            std::cerr << "Error: input file name cannot be empty.\n";
            continue;
        }

        const fs::path inputPath = TEST_ROOT / selectedFolder / "input" / inputFileName;
        if (!readInputFile(inputPath, source)) {
            continue;
        }

        return PromptResult::Success;
    }
}

PromptResult promptOutputFile(
    const std::string& selectedFolder,
    const std::vector<std::string>& outputLines
) {
    while (true) {
        std::string outputFileName;
        const std::string prompt = "Masukkan nama file output ke test/" +
                                   selectedFolder +
                                   "/output (0 untuk kembali, q untuk keluar): ";

        PromptResult promptResult = readPrompt(prompt, outputFileName);

        if (promptResult == PromptResult::Quit) {
            return PromptResult::Quit;
        }

        if (promptResult == PromptResult::Cancel) {
            return PromptResult::Cancel;
        }

        if (outputFileName.empty()) {
            std::cerr << "Error: output file name cannot be empty.\n";
            continue;
        }

        const fs::path outputPath = TEST_ROOT / selectedFolder / "output" / outputFileName;
        if (!writeOutputFile(outputPath, outputLines)) {
            continue;
        }

        std::cout << "Output berhasil ditulis ke " << resolveTxtPath(outputPath).string() << "\n";
        return PromptResult::Success;
    }
}

bool shouldRunLexerOnly(const std::string& selectedFolder) {
    return selectedFolder == "milestone-1";
}

bool shouldRunSyntaxAnalyzer(const std::string& selectedFolder) {
    return selectedFolder == "milestone-2";
}

bool shouldRunSemanticAnalyzer(const std::string& selectedFolder) {
    return selectedFolder == "milestone-3";
}

std::vector<std::string> runAnalyzerForFolder(
    const std::string& selectedFolder,
    const std::string& source
) {
    if (shouldRunLexerOnly(selectedFolder)) {
        return runLexer(source);
    }

    if (shouldRunSyntaxAnalyzer(selectedFolder)) {
        return runSyntaxAnalyzer(source);
    }

    if (shouldRunSemanticAnalyzer(selectedFolder)) {
        return runSemanticAnalyzer(source);
    }

    return {"Error: unsupported test folder '" + selectedFolder + "'."};
}

void printOutputLines(
    const std::string& selectedFolder,
    const std::vector<std::string>& outputLines
) {
    if (shouldRunLexerOnly(selectedFolder)) {
        std::cout << "\nHasil lexical analyzer:\n";
    } 
    else if (shouldRunSyntaxAnalyzer(selectedFolder)) {
        std::cout << "\nHasil syntax analyzer:\n";
    }
    else if (shouldRunSemanticAnalyzer(selectedFolder)) {
        std::cout << "\nHasil semantic analyzer:\n";
    }
    else {
        std::cout << "\nHasil analyzer:\n";
    }

    for (const std::string& line : outputLines) {
        std::cout << line << "\n";
    }

    std::cout << "\n";
}

}

int runCli() {
    while (true) {
        std::string selectedFolder;
        PromptResult result = promptTestFolder(selectedFolder);

        if (result == PromptResult::FatalError) {
            return 1;
        }

        if (result == PromptResult::Quit) {
            break;
        }

        while (true) {
            std::string source;
            result = promptInputSource(selectedFolder, source);

            if (result == PromptResult::Quit) {
                std::cout << "Program selesai.\n";
                return 0;
            }

            if (result == PromptResult::Cancel) {
                break;
            }

            const std::vector<std::string> outputLines = runAnalyzerForFolder(selectedFolder, source);
            printOutputLines(selectedFolder, outputLines);

            result = promptOutputFile(selectedFolder, outputLines);
            if (result == PromptResult::Quit) {
                std::cout << "Program selesai.\n";
                return 0;
            }

            if (result == PromptResult::Cancel) {
                continue;
            }

            std::cout << "\n";
        }
    }

    std::cout << "Program selesai.\n";
    return 0;
}
