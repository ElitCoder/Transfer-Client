#pragma once
#ifndef CLI_H
#define CLI_H

#include <condition_variable>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <thread>
#include <list>

enum {
	ERROR_OLD_PROTOCOL
};

class Packet;
class NetworkCommunication;

struct HostNetwork {
	std::shared_ptr<NetworkCommunication> network_;
	std::shared_ptr<std::thread> packet_thread_;
	
	int id_;
};

class CLI {
public:
	void start();
	void process(NetworkCommunication& network, Packet& packet);
	
	Packet waitForAnswer();
	
	void removeOldNetworks(int id);
	void shutdown();
	
	void sendFile(const std::string& to, std::string file, std::string directory, std::string base);
	
private:
	void handleJoin();
	void handleAvailable();
	void handleInform();
	void handleSend();
	void handleSendResult();
	void handleInitialize();
	void handleInformResult();
	void handleClientDisconnect();
	
	void notifyWaiting();
	
	Packet* packet_ 				= nullptr;
	NetworkCommunication* network_	= nullptr;
	
	std::condition_variable answer_cv_;
	std::mutex answer_mutex_;
	std::shared_ptr<Packet> answer_packet_ = nullptr;
	
	std::unordered_map<std::string, std::shared_ptr<std::ofstream>> file_streams_;
	std::unordered_map<int, std::vector<std::string>> file_id_connections_;
	
	std::list<HostNetwork> networks_;
	
	// Packet threads to be killed, but couldn't since they were processing the packet which killed them
	// Needs to be joined by another thread
	std::mutex old_networks_mutex_;
	std::list<HostNetwork> old_networks_;
	
	// What direct connected IPs was successful
	std::unordered_map<std::string, bool> connect_results_;
	
	// Active direct connection to the receiving side, to avoid re-opening the connection for every file
	std::shared_ptr<NetworkCommunication> active_direct_connection_ = nullptr;
	std::shared_ptr<std::thread> active_packet_thread_ = nullptr;
	
	// Our client ID from the server
	int client_id_ = -1;
};

// Start different packetThreads for direct connections
void packetThread(NetworkCommunication& network, int id, bool do_accept);

#endif