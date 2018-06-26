#pragma once
#ifndef PARAMETER_H
#define PARAMETER_H

#include <vector>
#include <string>
#include <unordered_map>

class Parameter {
public:
	void set(int argc, char** argv);
	const std::vector<std::string>& get(const std::string& option);
	bool has(const std::string& option);
	
private:
	void set(const std::string& option, const std::vector<std::string>& parameters);
	
	std::unordered_map<std::string, std::vector<std::string>> options_;
};

#endif