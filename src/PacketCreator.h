#pragma once
#ifndef PACKET_CREATOR_H
#define PACKET_CREATOR_H

#include <string>

enum {
	HEADER_JOIN,
	HEADER_AVAILABLE
};

class Packet;

class PacketCreator {
public:
	static Packet join(const std::string& name);
	static Packet available();
};

#endif