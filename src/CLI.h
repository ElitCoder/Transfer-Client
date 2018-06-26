#pragma once
#ifndef CLI_H
#define CLI_H

class Packet;

class CLI {
public:
	void start();
	void process(Packet& packet);
	
private:
	void handleJoin();
	void handleAvailable();
	
	Packet* packet_ = nullptr;
};

#endif