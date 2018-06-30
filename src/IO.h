#pragma once
#ifndef IO_H
#define IO_H

#include <string>
#include <vector>

class IO {
public:
#ifndef WIN32
	static void download(const std::string& url, const std::string& name);
#endif

	static bool isDirectory(const std::string& path);
	static std::vector<std::string> listDirectory(const std::string& path);
	static size_t getSize(const std::string& path);
	static void createDirectory(const std::string& path);
};

#endif