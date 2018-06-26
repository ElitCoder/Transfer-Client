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

Packet PacketCreator::inform(const string& to, const string& file) {
	Packet packet;
	packet.addHeader(HEADER_INFORM);
	packet.addString(to);
	packet.addString(file);
	packet.finalize();
	
	return packet;
}

Packet PacketCreator::send(const string& to, const string& file, const vector<unsigned char>& data, bool first) {
	Packet packet;
	packet.addHeader(HEADER_SEND);
	packet.addString(to);
	packet.addString(file);
	packet.addBytes(data);
	packet.addBool(first);
	packet.finalize();
	
	return packet;
}

Packet PacketCreator::sendResult(int id, bool result) {
	Packet packet;
	packet.addHeader(HEADER_SEND_RESULT);
	packet.addInt(id);
	packet.addBool(result);
	packet.finalize();
	
	return packet;
}