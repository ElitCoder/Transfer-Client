#include "Log.h"
#include "Base.h"
#include "NetworkCommunication.h"
#include "Packet.h"
#include "Config.h"
#include "CLI.h"
#include "Parameter.h"

using namespace std;

string g_protocol_standard = "a3";

static void printStart() {
	Log(NONE) << "Transfer-Client [alpha] [" << __DATE__ << " @ " << __TIME__ << "]\n";
	Log(NONE) << "Protocol standard: " << g_protocol_standard << "\n";
}

static void packetThread() {
	while (true) {
		// Wait until the Server sends something
		auto& packet = Base::network().waitForPacket();
		Base::cli().process(packet);
		Base::network().completePacket();
	}
}

static void cliThread() {
	Base::cli().start();
}

static void process() {
	Log(DEBUG) << "Getting config options for network\n";
	
	auto hostname = Base::config().get<string>("host", "localhost");
	auto port = Base::config().get<unsigned short>("port", 12000);
	
	Base::network().start(hostname, port);
	
	// Process options
	thread cli_thread(cliThread);
	
	// Use main thread for receiving packets
	packetThread();
}

int main(int argc, char** argv) {
	printStart();
	
	Base::config().parse("config");
	Base::parameter().set(argc, argv);
	
	process();
	
	return 0;
}