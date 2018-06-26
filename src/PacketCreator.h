#pragma once
#ifndef PACKET_CREATOR_H
#define PACKET_CREATOR_H

#include <string>

enum {
	HEADER_JOIN
};

class Packet;

class PacketCreator {
public:
	static Packet join(const std::string& name);
};

#endif