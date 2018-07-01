#include "Log.h"
#include "Base.h"
#include "NetworkCommunication.h"
#include "Packet.h"
#include "Config.h"
#include "CLI.h"
#include "Parameter.h"

using namespace std;

string g_protocol_standard = "a7";
static mutex g_cli_sync_;

static void printStart() {
	Log(NONE) << "Transfer-Client [alpha] [" << __DATE__ << " @ " << __TIME__ << "]\n";
	Log(NONE) << "Protocol standard: " << g_protocol_standard << "\n";
}

void packetThread(NetworkCommunication& network, string name, bool do_accept) {
	if (do_accept)
		network.acceptConnection();

	while (true) {
		// Wait until the Server sends something
		auto* packet = network.waitForPacket();
		
		// Is shutdown ordered?
		if (packet == nullptr)
			break;
			
		// Remove old networks if there are any
		Base::cli().removeOldNetworks(name);

		// Protect CLI using single threading since there might be multiple packet threads
		g_cli_sync_.lock();
		Base::cli().process(network, *packet);
		g_cli_sync_.unlock();
		
		network.completePacket();
	}
	
	Log(NETWORK) << "packetThread exiting\n";
}

static void process() {
	Log(DEBUG) << "Getting config options for network\n";
	
	auto hostname = Base::config().get<string>("host", "localhost");
	auto port = Base::config().get<unsigned short>("port", 12000);
	
	Base::network().start(hostname, port);
	
	auto& network = Base::network();
	thread network_thread = thread(packetThread, ref(network), "", false);
	
	// Run CLI
	Base::cli().start();
	
	network_thread.join();
	
	// Kill any remaining old networks
	Base::cli().removeOldNetworks("");
}

int main(int argc, char** argv) {
	printStart();
	
	Base::config().parse("config");
	Base::parameter().set(argc, argv);
	
	process();
	
	return 0;
}