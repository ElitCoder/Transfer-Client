#pragma once
#ifndef CLI_H
#define CLI_H

#include <condition_variable>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <fstream>

class Packet;

class CLI {
public:
	void start();
	void process(Packet& packet);
	
	Packet waitForAnswer();
	
private:
	void handleJoin();
	void handleAvailable();
	void handleInform();
	void handleSend();
	void handleSendResult();
	
	Packet* packet_ = nullptr;
	
	std::condition_variable answer_cv_;
	std::mutex answer_mutex_;
	std::shared_ptr<Packet> answer_packet_ = nullptr;
	
	std::unordered_map<std::string, std::shared_ptr<std::ofstream>> file_streams_;
};

#endif