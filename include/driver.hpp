#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <string>
#include <vector>

std::vector<std::string> runLexer(const std::string& source);
std::vector<std::string> runSyntaxAnalyzer(const std::string& source);
std::vector<std::string> runSemanticAnalyzer(const std::string& source);

#endif
