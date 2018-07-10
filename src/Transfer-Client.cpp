#include "Log.h"
#include "Base.h"
#include "NetworkCommunication.h"
#include "Packet.h"
#include "Config.h"
#include "CLI.h"
#include "Parameter.h"

#include <signal.h>

using namespace std;

#ifdef WIN32
constexpr auto quick_exit = _exit; // mingw32 does not support quick_exit for now
#endif

string g_protocol_standard = "a9";
static mutex g_cli_sync_;

static void printStart() {
	Log(NONE) << "Transfer-Client [alpha] [" << __DATE__ << " @ " << __TIME__ << "]\n";
	Log(NONE) << "Protocol standard: " << g_protocol_standard << "\n";
}

void packetThread(NetworkCommunication& network, int id, bool do_accept) {
	if (do_accept)
		network.acceptConnection();

	while (true) {
		// Wait until the Server sends something
		auto* packet = network.waitForPacket();
		
		// Is shutdown ordered?
		if (packet == nullptr)
			break;
			
		// Remove old networks if there are any
		Base::cli().removeOldNetworks(id);

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
	thread network_thread = thread(packetThread, ref(network), -1, false);
	
	// Run CLI
	Base::cli().start();
	
	network_thread.join();
	
	// Kill any remaining old networks
	Base::cli().removeOldNetworks(-1);
}

void handler(int unused) {
	if (unused) {}
	
	Log(DEBUG) << "Terminating using handler and quick_exit\n";
	
	quick_exit(0);
}

int main(int argc, char** argv) {
	signal(SIGINT, handler);
	signal(SIGTERM, handler);
	
#ifdef SIGBREAK
	// Make sure closing the cmd-window on Windows won't cause any problems
	signal(SIGBREAK, handler);
	
	Log(DEBUG) << "System is using SIGBREAK handler\n";
#endif

	printStart();
	
	Base::config().parse("config");
	Base::parameter().set(argc, argv);
	
	process();
	
	return 0;
}