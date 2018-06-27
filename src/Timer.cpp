#include "Timer.h"

using namespace std;

Timer::Timer() {
	start();
}

Timer::Timer(size_t ms) {
	start(ms);
}

void Timer::start() {
	start_time_ = chrono::system_clock::now();
	elapsed_time_ = start_time_;
}

void Timer::start(size_t ms) {
	start();
	
	elapsed_time_ = start_time_ + chrono::milliseconds(ms);
}

// Get current elapsed time and restart timer
double Timer::restart() {
	auto now = chrono::system_clock::now();
	auto nanoseconds = chrono::duration_cast<chrono::nanoseconds>(now - start_time_);
	start_time_ = now;
	
	// Return seconds
	return (double)nanoseconds.count() / 1e09;
}

bool Timer::elapsed() const {
	auto now = chrono::system_clock::now();
	auto nanoseconds = chrono::duration_cast<chrono::nanoseconds>(now - elapsed_time_);
	
	return nanoseconds.count() > 0;
}