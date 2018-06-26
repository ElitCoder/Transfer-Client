#include "PacketCreator.h"
#include "Packet.h"

using namespace std;

Packet PacketCreator::join(const string& name) {
	Packet packet;
	packet.addHeader(HEADER_JOIN);
	packet.addString(name);
	packet.finalize();
	
	return packet;
}

Packet PacketCreator::available() {
	Packet packet;
	packet.addHeader(HEADER_AVAILABLE);
	packet.finalize();
	
	return packet;
}