#pragma once
#ifndef CLI_H
#define CLI_H

#include <condition_variable>
#include <mutex>
#include <memory>

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
};

#endif