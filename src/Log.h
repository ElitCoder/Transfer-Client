#pragma once
#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <iostream>
#include <sstream>

enum {
	DEBUG,
	INFORMATION,
	NONE,
	ERROR,
	WARNING,
	NETWORK,
	GAME
};

class Log : public std::ostringstream {
public:
	Log();
	Log(int level);
	
	~Log();
	
	static void setDebug(bool status);

private:
	static std::mutex print_mutex_;
	static bool enable_debug_;
	
	int level_;
};

#endif