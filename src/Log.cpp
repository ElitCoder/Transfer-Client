#include "Log.h"

#define PRINT_STREAM	(std::cout)
#define FLUSH_STREAM	(std::flush)

using namespace std;

mutex Log::print_mutex_;
bool Log::enable_debug_ = true;

Log::Log() {
	level_ = NONE;
}

Log::Log(int level) {
	level_ = level;
}

// TODO: Add functionality for writing to file, etc.
Log::~Log() {
	if (level_ == DEBUG && !enable_debug_)
		return;

	lock_guard<mutex> guard(print_mutex_);
	
	switch (level_) {
		case NONE:
			break;
			
		case DEBUG: PRINT_STREAM << "[DEBUG]";
			break;
			
		case INFORMATION: PRINT_STREAM << "[INFORMATION]";
			break;
			
		case ERROR: PRINT_STREAM << "[ERROR]";
			break;
			
		case WARNING: PRINT_STREAM << "[WARNING]";
			break;
			
		case NETWORK: PRINT_STREAM << "[NETWORK]";
			break;
			
		case GAME: PRINT_STREAM << "[GAME]";
			break;
			
		default: PRINT_STREAM << "[UNKNOWN ENUM]";
	}
		
	if (level_ != NONE)
		PRINT_STREAM << " ";

	PRINT_STREAM << str() << FLUSH_STREAM;
}

void Log::setDebug(bool status) {
	enable_debug_ = status;
}