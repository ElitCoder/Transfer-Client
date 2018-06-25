#include "CLI.h"
#include "Base.h"
#include "Log.h"
#include "NetworkCommunication.h"
#include "PacketCreator.h"
#include "Packet.h"

using namespace std;

void CLI::start(const vector<string>& files) {
	Log(NONE) << "Which host should receive the files?\n";
	
	// Ask the Server for a list of available hosts
	Base::network().send(PacketCreator::available());
}