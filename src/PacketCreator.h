#pragma once
#ifndef PACKET_CREATOR_H
#define PACKET_CREATOR_H

#include <string>
#include <vector>

enum {
	HEADER_JOIN,
	HEADER_AVAILABLE,
	HEADER_INFORM,
	HEADER_SEND,
	HEADER_SEND_RESULT,
	HEADER_INITIALIZE,
	HEADER_INFORM_RESULT,
	HEADER_CLIENT_DISCONNECT
};

class Packet;

class PacketCreator {
public:
	static Packet join(const std::string& name);
	static Packet available();
	static Packet inform(const std::string& to, const std::string& file, const std::string& directory, bool direct);
	static Packet informResult(bool accept, int id, int port, const std::vector<std::string>& addresses);
	static Packet send(const std::string& to, const std::string& file, const std::string& directory, const std::pair<size_t, const unsigned char*>& data, bool first, bool direct_connected = false, int id = -1);
	static Packet sendResult(int id, bool result);
	static Packet initialize(const std::string& version);
};

#endif