#include "CLI.h"
#include "Base.h"
#include "Parameter.h"
#include "Config.h"
#include "NetworkCommunication.h"
#include "PacketCreator.h"
#include "Packet.h"
#include "Timer.h"
#include "IO.h"

#include <algorithm>

// Network
#include <sys/stat.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

using namespace std;

#ifdef WIN32
constexpr auto quick_exit = _exit; // mingw32 does not support quick_exit for now
#endif

extern string g_protocol_standard;

static void monitoring() {
	auto name = Base::config().get<string>("name", "");
	
	// Register at Server as monitoring with certain name
	Base::network().send(PacketCreator::join(name));
	
	Log(INFORMATION) << "Registering at Server..\n";
}

static void splitBaseFile(string input, string& base, string& file) {
	size_t pos = 0;
	
	while (((pos = input.find("/")) != string::npos) || ((pos = input.find("\\")) != string::npos)) {		
		base += input.substr(0, pos + 1);
	    input.erase(0, pos + 1);
	}
	
	file = input;
}

// Returns local IP addresses
// From github.com/wewearglasses
static vector<string> getIPAddresses() {
	struct ifaddrs* interfaces = NULL;
	vector<string> addresses;
	
	if (getifaddrs(&interfaces) == 0) {
		auto* temp = interfaces;
		
		while (temp != NULL) {
			// Add as an IP if the interface belongs to AF_INET
			if (temp->ifa_addr->sa_family == AF_INET)
				addresses.push_back(inet_ntoa(((struct sockaddr_in*)temp->ifa_addr)->sin_addr));
				
			temp = temp->ifa_next;
		}
	}
	
	freeifaddrs(interfaces);
	return addresses;
}

static void sendFile(const string& to, string file, string directory, string base) {
	string full_path = base + directory + file;
	
	if (directory.empty())
		full_path = base + file;
	
	if (IO::isDirectory(full_path)) {
		auto recursive = Base::parameter().has("-r");

		if (!recursive) {
			// We're not doing recursive sending
			Log(WARNING) << "Recursive sending is disabled\n";
			
			return;
		}
		
		Log(DEBUG) << file << " is a folder, doing recursion\n";
		
		// List contents of directory and sendFile on each of them
		auto contents = IO::listDirectory(full_path);
		
		for (auto& recursive_file : contents) {
			// Ignore hidden files (Linux)
			if (recursive_file.front() == '.')
				continue;

			string path = recursive_file;
			string new_base = "";
						
			sendFile(to, path, directory + file + "/", base);
		}
		
		// We're done with this file
		return;
	}
	
	// Inform target of file transfer
	Base::network().send(PacketCreator::inform(to, file, directory));
	auto answer = Base::cli().waitForAnswer();
	auto accepted = answer.getBool();
	
	if (!accepted) {
		Log(ERROR) << "Receiving side did not accept the file transfer or is not connected\n";
		
		return;
	}
	
	auto try_direct = answer.getBool();
	auto num_addresses = answer.getInt();
	auto port = answer.getInt();
	
	vector<string> remote_addresses;
	shared_ptr<NetworkCommunication> direct_connection = nullptr;
	
	Log(DEBUG) << "Direct connection is " << (try_direct ? "enabled" : "disabled") << endl;
	
	if (try_direct) {
		Log(DEBUG) << "Receiving client is waiting at port " << port << endl;
		
		for (int i = 0; i < num_addresses; i++) {
			auto ip = answer.getString();
			remote_addresses.push_back(ip);
			
			Log(DEBUG) << "Available remote address: " << ip << endl;
			Log(DEBUG) << "Trying " << ip << endl;
			
			direct_connection = make_shared<NetworkCommunication>();
			
			if (direct_connection->start(ip, port, true))
				break;
			else
				direct_connection = nullptr;
		}
	}
	
	// What network to use?
	NetworkCommunication* use_network_;
	bool direct_connected = false;
	
	if (direct_connection == nullptr) {
		// Use relay, no direct connection succeeded
		use_network_ = &Base::network();
	} else {
		// Use direct connection
		use_network_ = direct_connection.get();
		direct_connected = true;
	}
	
	size_t size;
	
	try {
		size = IO::getSize(full_path);
	} catch (...) {
		return;
	}
	
	thread direct_packet_thread_;
	
	if (direct_connected)
		direct_packet_thread_ = thread(packetThread, ref(*direct_connection), "");
	
	Log(DEBUG) << "Sending the file " << base << " + " << directory << " + " << file << endl;
	Log(DEBUG) << "File size " << size << " bytes\n";
	
	ifstream file_stream(full_path, ios_base::binary); // It's valid since getSize() did not throw
	Timer timer;
	
	for (size_t i = 0; i < size;) {
		size_t buffer_size = Base::config().get<size_t>("buffer_size", 4 * 1024 * 1024); // 4 MB default
		size_t read_amount = min(buffer_size, size - i);
		
		// Create Packet inplace for speed
		Packet packet;
		packet.addHeader(HEADER_SEND);
		
		// Bypass server changes to the packets
		if (direct_connected)
			packet.addInt(0);
		else
			packet.addString(to);

		packet.addString(file);
		packet.addString(directory);
		
		// Allocate memory for reading data
		auto& data = packet.internal();
		auto old_size = data->size();
		data->resize(data->size() + read_amount + 4);
				
		unsigned char* data_pointer = data->data() + old_size + 4;
		
		file_stream.read((char*)data_pointer, read_amount);
		auto actually_read = file_stream.gcount();
		
		if (actually_read != (long)read_amount)
			data->resize(old_size + actually_read + 4);
			
		int nbr = actually_read;
		
		data->at(old_size) = (nbr >> 24) & 0xFF;
	   	data->at(old_size + 1) = (nbr >> 16) & 0xFF;
	    data->at(old_size + 2) = (nbr >> 8) & 0xFF;
	    data->at(old_size + 3) = nbr & 0xFF;
			
		packet.addBool(i == 0);
		packet.finalize();
		
		use_network_->send(packet);
		i += actually_read;
				
		answer = Base::cli().waitForAnswer();
		answer.getInt();
		accepted = answer.getBool();
		
		if (!accepted) {
			Log(WARNING) << "Something went wrong during file transfer\n";
			
			return;
		}
		
		if (buffer_size < size) {
			auto elapsed_time = timer.elapsedTime();
			
			Log(DEBUG) << "Current speed: " << (static_cast<double>(i) / 1024 / 1024) / elapsed_time << " MB/s\n";
		}
	}
	
	Log(DEBUG) << "Message EOF to receiver\n";

	// Tell the receiver that we're done
	use_network_->send(PacketCreator::send(to, file, directory, { 0, nullptr }, false, direct_connected));
		
	answer = Base::cli().waitForAnswer();
	answer.getInt();
	accepted = answer.getBool();
	
	auto elapsed_time = timer.restart();
	
	if (accepted)
		Log(DEBUG) << "File successfully sent\n";
	else
		Log(ERROR) << "File could not be sent\n";
		
	Log(DEBUG) << "Elapsed time: " << elapsed_time << " seconds\n";
	Log(DEBUG) << "Speed: " << (static_cast<double>(size) / 1024 / 1024) / elapsed_time << " MB/s\n";
	
	Log(NONE) << endl;
	
	if (direct_connected) {
		Log(DEBUG) << "Killing direct connection network\n";
		direct_connection->kill();
		direct_packet_thread_.join();
		Log(DEBUG) << "Killed network\n";
		
		Log(NONE) << endl;
	}
}

static void sendFiles(const string& to) {
	auto& files = Base::parameter().get("-s");
	
	for (auto& file : files) {
		auto file_copy = file;
		
		// Remove / or \ at the end if there is one
		if (file_copy.back() == '/' || file_copy.back() == '\\')
			file_copy.pop_back();
			
		string base = "";
		splitBaseFile(file_copy, base, file_copy);
		
		sendFile(to, file_copy, "", base);
	}
}

// Various test functions for development
static void test() {
	auto addresses = getIPAddresses();
	
	for (auto& address : addresses)
		Log(DEBUG) << "Local address: " << address << endl;
}

void CLI::start() {
	// TODO: Remove test
	test();
	
	// Remove old update files if they exist
	remove("client.zip");
	remove("update.sh");
	
	// Register at Server
	Base::network().send(PacketCreator::initialize(g_protocol_standard));
	auto packet = waitForAnswer();
	auto accepted = packet.getBool();
	
	if (accepted) {
		Log(DEBUG) << "Server accepted our protocol version\n";
	} else {
		auto code = packet.getInt();
		
		Log(ERROR) << "Client was not accepted, code " << code << "\n";
		
		if (code == ERROR_OLD_PROTOCOL) {
#ifdef WIN32
			Log(ERROR) << "Auto-updating client is not available for Windows, please download the new binaries\n";
#else
			// Auto-update
			auto url = packet.getString();
			auto url_script = packet.getString();
			
			Log(INFORMATION) << "Initiating auto-update\n";
			IO::download(url, "client.zip");
			IO::download(url_script, "update.sh");
			
			// Make update script executable
			chmod("update.sh", 0755);
			
			// Start update script
			if (system("./update.sh")) {}
#endif
		}
		
		quick_exit(-1);
	}
	
	// Check options
	// Monitoring mode, wait for packets
	if (Base::parameter().has("-m")) {
		monitoring();
		
		// Wait for packets, terminating this thread
		return;
	}
	
	// List available hosts
	if (Base::parameter().has("-l")) {
		Base::network().send(PacketCreator::available());
		
		Log(DEBUG) << "Asking for available hosts\n";
		
		return;
	}
	
	// Send files
	if (Base::parameter().has("-s")) {
		// We need a receiver
		if (!Base::parameter().has("-t")) {
			Log(ERROR) << "Specify receiver with \"-t\" option\n";
			
			quick_exit(-1);
		}
		
		// To whom?
		string to = Base::parameter().get("-t").front();
		sendFiles(to);
			
		quick_exit(0);
	}
	
	// If no option has been specified at the end, default to monitoring mode
	monitoring();
}

Packet CLI::waitForAnswer() {
	unique_lock<mutex> lock(answer_mutex_);
	answer_cv_.wait(lock, [this] { return answer_packet_ != nullptr; });
	
	// Make copy and return it
	Packet packet = *answer_packet_;
	
	// Reset pointer
	answer_packet_ = nullptr;
	
	return packet;
}

void CLI::removeOldNetworks(const string& name) {
	lock_guard<mutex> lock(old_networks_mutex_);
	
	while (!old_networks_.empty()) {
		auto& network = old_networks_.front();
		
		if (network.file_ == name)
			break;
				
		// Join exiting packet thread
		network.packet_thread_->join();
				
		// Erase
		old_networks_.pop_front();
	}
}

void CLI::process(NetworkCommunication& network, Packet& packet) {
	auto header = packet.getByte();
	
	packet_ = &packet;
	network_ = &network;
	
	switch (header) {
		case HEADER_JOIN: handleJoin();
			break;
			
		case HEADER_AVAILABLE: handleAvailable();
			break;
			
		case HEADER_INFORM: handleInform();
			break;
			
		case HEADER_SEND: handleSend();
			break;
			
		case HEADER_SEND_RESULT: handleSendResult();
			break;
			
		case HEADER_INITIALIZE: handleInitialize();
			break;
			
		case HEADER_INFORM_RESULT: handleInformResult();
			break;
			
		default: {
			Log(WARNING) << "Unknown packet header ";
			printf("%02X", header);
			Log(NONE) << "\n";
		}
	}
}

void CLI::notifyWaiting() {
	lock_guard<mutex> lock(answer_mutex_);
	answer_packet_ = make_shared<Packet>(*packet_);
	answer_cv_.notify_one();
}

void CLI::handleJoin() {
	auto result = packet_->getBool();
	
	if (result)
		Log(INFORMATION) << "Accepted at Server\n";
	else {
		Log(WARNING) << "Server did not accept our connection\n";
		
		quick_exit(-1);
	}
}

void CLI::handleAvailable() {
	auto size = packet_->getInt();
	
	Log(DEBUG) << "Hosts:\n";
	
	for (int i = 0; i < size; i++) {
		auto id = packet_->getInt();
		auto name = packet_->getString();
		
		Log(DEBUG) << "Host " << id << " : " << name << endl;
	}
	
	// Quit since it's CLI
	quick_exit(0);
}

void CLI::handleInform() {
	notifyWaiting();
}

void CLI::handleInformResult() {
	auto id = packet_->getInt();
	auto file = packet_->getString();
	auto directory = packet_->getString();
	
	// Make sure the same file is not being written right now
	for (auto& network : networks_) {
		if (network.file_ == (directory + file)) {
			Log(ERROR) << "The file " << (directory + file) << " is already being written\n";
			Base::network().send(PacketCreator::informResult(false /* accept or decline */, id, 0, {}));
			
			return;
		}
	}
	
	// Return a list of available local IPs to see if the clients might be on the same network
	auto addresses = getIPAddresses();
	int port = 30500;
	
	if (Base::config().get<bool>("direct", true)) {
		// Find available port
		while (true) {
			networks_.emplace_back();
			networks_.back().network_ = make_shared<NetworkCommunication>();
			
			auto& network = networks_.back().network_;
			
			if (!network->start("", port, false, true)) {
				Log(ERROR) << "Hosting failed at port " << port << "\n";
				
				networks_.pop_back();
				port++;
			} else {
				Log(DEBUG) << "Hosting successful at port " << port << "\n";
				
				break;
			}
		}
	} else {
		// Signal direct connection is disabled by giving no bind addresses
		addresses.clear();
	}
	
	Base::network().send(PacketCreator::informResult(true /* accept or decline */, id, port, addresses));
	
	if (Base::config().get<bool>("direct", true)) {
		auto& network = networks_.back().network_;		
		network->acceptConnection();
		
		networks_.back().file_ = directory + file;
		networks_.back().packet_thread_ = make_shared<thread>(packetThread, ref(*network), networks_.back().file_);
	}
}

void CLI::handleSend() {
	// TODO: Receive file here	
	auto id = packet_->getInt();
	auto file = packet_->getString();
	auto directory = packet_->getString();
	auto bytes = packet_->getBytes();
	auto first = packet_->getBool();
	
	// Add directory
	file = directory + file;
	auto original_file = file;
	
	// Add folder ID if the option is enabled
	if (Base::config().has("output_folder"))
		file = Base::config().get<string>("output_folder", "") + "/" + file;
		
	if (bytes.first == 0) {
		Log(DEBUG) << "Removing from cache\n";
		
		// Remove from cache
		auto iterator = file_streams_.find(file);
		
		// Extra check
		if (iterator != file_streams_.end()) {
			if (iterator->second->fail())
				Log(WARNING) << "Fail bit set\n";
				
			if (iterator->second->bad())
				Log(WARNING) << "Bad bit set\n";
				
			if (iterator->second->eof())
				Log(WARNING) << "Eof bit set\n";
		}

		// Send result that we're done before flushing
		network_->send(PacketCreator::sendResult(id, true));

		if (iterator != file_streams_.end()) {
			Log(DEBUG) << "Flushing..\n";
			
			iterator->second->flush();
			iterator->second->close();
			
			file_streams_.erase(file);
			
			Log(DEBUG) << "Done\n";
		}
		
		// Kill network if it's sent over direct connection
		for (auto& network : networks_) {
			if (network.file_ == original_file) {
				Log(DEBUG) << "Kill direct connection network " << original_file << endl;
				
				network.network_->kill(true);
				
				lock_guard<mutex> lock(old_networks_mutex_);
				old_networks_.push_back(network);
				
				networks_.erase(remove_if(networks_.begin(), networks_.end(), [&original_file] (auto& element) {
					return element.file_ == original_file;
				}), networks_.end());
				
				break;
			}
		}
		
		return;
	}
	
	shared_ptr<ofstream> file_stream;
	
	if (first) {
		Log(DEBUG) << "Removing existing files and preparing stream\n";
		
		// Create folder if it does not exist
		if (Base::config().has("output_folder"))
			IO::createDirectory(Base::config().get<string>("output_folder", ""));
		
		// Create directory if it does not exist
		IO::createDirectory(Base::config().get<string>("output_folder", "") + "/" + directory);
		
		// See if file stream already exists, if it does we should not try to write to the same file
		auto iterator = file_streams_.find(file);
		
		if (iterator != file_streams_.end()) {
			Log(WARNING) << "File " << file << " already exists, disabling write\n";
			
			network_->send(PacketCreator::sendResult(id, false));
			return;
		}
		
		// Remove any existing files
		remove(file.c_str());
		
		shared_ptr<ofstream> stream_pointer = make_shared<ofstream>(file, ios::binary);
		
		// Add file stream to cache
		file_streams_[file] = stream_pointer;
	}
	
	// Find stream in cache
	auto iterator = file_streams_.find(file);
	
	if (iterator == file_streams_.end())
		Log(WARNING) << "Could not find file stream\n";
		
	file_stream = iterator->second;
		
	if (!file_stream) {
		Log(WARNING) << "Could not open " << file << " for writing\n";
		
		return;
	}
	
	if (file_stream->fail())
		Log(WARNING) << "Fail bit set\n";
		
	if (file_stream->bad())
		Log(WARNING) << "Bad bit set\n";
		
	if (file_stream->eof())
		Log(WARNING) << "Eof bit set\n";
	
	Log(DEBUG) << "Writing file " << file << " with " << bytes.first << " bytes\n";
	
	file_stream->write((const char*)bytes.second, bytes.first);
	
	// Send OK to sender
	network_->send(PacketCreator::sendResult(id, true));
}

void CLI::handleSendResult() {
	notifyWaiting();
}

void CLI::handleInitialize() {
	notifyWaiting();
}