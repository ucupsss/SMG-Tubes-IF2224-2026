#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <filesystem>
#include <string>
#include <vector>

std::vector<std::string> getTestFolders(const std::filesystem::path& testRoot);
std::filesystem::path resolveTxtPath(const std::filesystem::path& filePath);
bool readInputFile(const std::filesystem::path& inputPath, std::string& source);
bool writeOutputFile(const std::filesystem::path& outputPath, const std::vector<std::string>& lines);

#endif
