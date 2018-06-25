#include "Log.h"
#include "Base.h"
#include "NetworkCommunication.h"
#include "Packet.h"
#include "Config.h"
#include "CLI.h"

using namespace std;

static mutex g_main_sync;

static void printStart() {
	Log(NONE) << "Transfer-Client [alpha] [" << __DATE__ << " @ " << __TIME__ << "]\n";
}

static void packetThread() {
	while (true) {
		// Wait until the Server sends something
		Base::network().waitForPacket();
		
		// Lock the game, we have a Packet
		g_main_sync.lock();
		
		while (Base::network().hasPacket()) {
			auto* packet = Base::network().getPacket();
			
			if (packet == nullptr)
				break;
				
			//Base::game().process(*packet);
			Base::network().completePacket();
		}
		
		g_main_sync.unlock();
	}
}

static void process(const vector<string>& files) {
	Log(DEBUG) << "Getting config options for network\n";
	
	auto hostname = Base::settings().get<string>("host", "localhost");
	auto port = Base::settings().get<unsigned short>("port", 12000);
	
	Base::network().start(hostname, port);
	
	thread packet_thread(packetThread);
	
	Base::cli().start(files);
}

int main(int argc, char** argv) {
	printStart();
	Base::settings().parse("config");
	
	for (int i = 1; i < argc; i++) {
		Log(DEBUG) << "arg: " << argv[i] << endl;
	}
	
	vector<string> files(argv + 1, argv + argc - 1);
	
	process(files);
	
	return 0;
}