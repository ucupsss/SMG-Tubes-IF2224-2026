#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <filesystem>
#include <string>
#include <vector>

bool readInputFile(const std::filesystem::path& inputPath, std::string& source);
bool writeOutputFile(const std::filesystem::path& outputPath, const std::vector<std::string>& lines);

#endif
