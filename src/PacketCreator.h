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
	HEADER_INITIALIZE
};

class Packet;

class PacketCreator {
public:
	static Packet join(const std::string& name);
	static Packet available();
	static Packet inform(const std::string& to, const std::string& file);
	static Packet send(const std::string& to, const std::string& file, const std::vector<unsigned char>& data, bool first);
	static Packet sendResult(int id, bool result);
	static Packet initialize(const std::string& version);
};

#endif