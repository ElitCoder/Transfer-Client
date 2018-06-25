#pragma once
#ifndef BASE_H
#define BASE_H

class Config;
class NetworkCommunication;
class CLI;

class Base {
public:
	static Config& settings();
	static NetworkCommunication& network();
	static CLI& cli();

private:
	static Config settings_;
	static NetworkCommunication network_;
	static CLI cli_;
};

#endif