#pragma once
#ifndef BASE_H
#define BASE_H

class Config;
class NetworkCommunication;
class CLI;
class Parameter;

class Base {
public:
	static Config& config();
	static NetworkCommunication& network();
	static CLI& cli();
	static Parameter& parameter();

private:
	static Config config_;
	static NetworkCommunication network_;
	static CLI cli_;
	static Parameter parameter_;
};

#endif