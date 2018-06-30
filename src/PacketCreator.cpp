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

Packet PacketCreator::inform(const string& to, const string& file, const string& directory) {
	Packet packet;
	packet.addHeader(HEADER_INFORM);
	packet.addString(to);
	packet.addString(file);
	packet.addString(directory);
	packet.finalize();
	
	return packet;
}

Packet PacketCreator::informResult(bool accept, int id, int port, const vector<string>& addresses) {
	Packet packet;
	packet.addHeader(HEADER_INFORM_RESULT);
	packet.addBool(accept);
	packet.addInt(id);
	packet.addInt(addresses.size());
	packet.addInt(port);
	
	for (auto& address : addresses)
		packet.addString(address);
		
	packet.finalize();
	
	return packet;
}

Packet PacketCreator::send(const string& to, const string& file, const string& directory, const pair<size_t, const unsigned char*>& data, bool first, bool direct_connected) {
	Packet packet;
	packet.addHeader(HEADER_SEND);
	
	if (direct_connected)
		packet.addInt(0);
	else
		packet.addString(to);
		
	packet.addString(file);
	packet.addString(directory);
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

Packet PacketCreator::initialize(const string& version) {
	Packet packet;
	packet.addHeader(HEADER_INITIALIZE);
	packet.addString(version);
	packet.finalize();
	
	return packet;
}