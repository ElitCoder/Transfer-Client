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

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#endif

using namespace std;

#ifdef WIN32
constexpr auto quick_exit = _exit; // mingw32 does not support quick_exit for now

#pragma comment(lib, "IPHLPAPI.lib")
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

#ifdef WIN32
// From SO
const char* inet_ntop(int af, const void* src, char* dst, int cnt) {
    struct sockaddr_in srcaddr;
 
    memset(&srcaddr, 0, sizeof(struct sockaddr_in));
    memcpy(&(srcaddr.sin_addr), src, sizeof(srcaddr.sin_addr));
 
    srcaddr.sin_family = af;
	
    if (WSAAddressToString((struct sockaddr*)&srcaddr, sizeof(struct sockaddr_in), 0, dst, (LPDWORD) &cnt) != 0) {
        DWORD rv = WSAGetLastError();
        printf("WSAAddressToString() : %lu\n",rv);
		
        return NULL;
    }
	
    return dst;
}
#endif

// Returns local IP addresses
// From github.com/wewearglasses
static vector<string> getIPAddresses() {
	vector<string> addresses;
	
#ifdef WIN32
	ULONG family = AF_INET;
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	ULONG buffer_size = 30000;
	PIP_ADAPTER_ADDRESSES p_addresses = (IP_ADAPTER_ADDRESSES*)malloc(buffer_size);
	
	if (GetAdaptersAddresses(family, flags, NULL, p_addresses, &buffer_size) == ERROR_BUFFER_OVERFLOW)
		Log(ERROR) << "Could not obtain adapter addresses\n";
		
	auto current_address = p_addresses;
	
	while (current_address) {
		auto* address = current_address->FirstUnicastAddress;
		
		while (address) {
			auto family = address->Address.lpSockaddr->sa_family;
			
			if (family == AF_INET) {
				SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(address->Address.lpSockaddr);

		        char str_buffer[INET_ADDRSTRLEN] = {0};
		        inet_ntop(AF_INET, &(ipv4->sin_addr), str_buffer, INET_ADDRSTRLEN);				
				addresses.push_back(str_buffer);
			}
			
			address = address->Next;
		}
		
		current_address = current_address->Next;
	}
	
	free(p_addresses);
#else
	struct ifaddrs* interfaces = NULL;
	
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
#endif
	
	return addresses;
}

static size_t lexicograpicallyCompare(const string& reference, const string& element) {
	for (size_t i = 0; i < min(reference.length(), element.length()); i++) {
		if (reference.at(i) != element.at(i))
			return i;
	}
	
	return min(reference.length(), element.length());
}

static void sortMostLikelyIP(const vector<string>& own_ips, vector<string>& remote_ips) {
	vector<string> possible_lan = { "192.168", "10." };
	vector<string> filtered_own;

	// See if we use some of these possible LAN IPs
	for (auto& ip : own_ips) {
		for (auto& possible : possible_lan) {
			auto matched = lexicograpicallyCompare(ip, possible);
			
			// Needs to match the full template in order to register as possible "LAN"
			if (matched == possible.length())
				filtered_own.push_back(ip);
		}
	}
	
	// Go through remote IPs and sort them if they match with filtered
	sort(remote_ips.begin(), remote_ips.end(), [&filtered_own] (auto& first, auto& second) {
		size_t best_first = 0;
		size_t best_second = 0;
		
		for (auto& filtered : filtered_own) {
			auto match_first = lexicograpicallyCompare(filtered, first);
			auto match_second = lexicograpicallyCompare(filtered, second);
			
			if (match_first > best_first)
				best_first = match_first;
			
			if (match_second > best_second)
				best_second = match_second;
		}
		
		return best_first > best_second;
	});
}

static void sendFile(const string& to, string file, string directory, string base, unordered_map<string, bool>& connect_results) {
	string full_path = base + directory + file;
	
	if (directory.empty())
		full_path = base + file;
	
	bool is_directory;
	
	try {
		is_directory = IO::isDirectory(full_path);
	} catch (...) {
		Log(WARNING) << "File " << full_path << " does not exist, skipping\n";
		
		return;
	}
	
	if (is_directory) {
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
						
			sendFile(to, path, directory + file + "/", base, connect_results);
		}
		
		// We're done with this file
		return;
	}
	
	// Inform target of file transfer
	Base::network().send(PacketCreator::inform(to, file, directory, Base::config().get<bool>("direct", true)));
	auto answer = Base::cli().waitForAnswer();
	auto accepted = answer.getBool();
	
	if (!accepted) {
		Log(ERROR) << "Receiving side did not accept the file transfer or is not connected\n";
		
		return;
	}
	
	auto try_direct = answer.getBool();
	auto num_addresses = answer.getInt();
	auto port = answer.getInt();
	auto own_id = answer.getInt();
	
	Log(DEBUG) << "Local ID " << own_id << endl;
	
	vector<string> remote_addresses;
	shared_ptr<NetworkCommunication> direct_connection = nullptr;
	
	Log(DEBUG) << "Direct connection is " << (try_direct ? "enabled" : "disabled") << endl;
	
	if (try_direct) {
		Log(DEBUG) << "Receiving client is waiting at port " << port << endl;
				
		for (int i = 0; i < num_addresses; i++) {
			auto ip = answer.getString();
			remote_addresses.push_back(ip);
			
			Log(DEBUG) << "Remote address " << ip << endl;
		}
		
		// Sort local IPs based on most likely to be connected
		sortMostLikelyIP(getIPAddresses(), remote_addresses);
		
		for (size_t i = 0; i < remote_addresses.size(); i++) {
			auto& ip = remote_addresses.at(i);
			
			// See if this IP is unreachable
			auto unreachable = connect_results.find(ip);
			
			if (unreachable != connect_results.end()) {
				Log(DEBUG) << "Skipping IP " << ip << endl;
				
				continue;
			}
			
			Log(DEBUG) << "Trying " << ip << endl;
			
			direct_connection = make_shared<NetworkCommunication>();
			
			if (direct_connection->start(ip, port, true)) {
				// Works
				break;
			} else {
				direct_connection = nullptr;
				
				// Add to known IPs to fail
				connect_results[ip] = false;
			}
		}
	}
	
	// What network to use?
	NetworkCommunication* use_network_;
	bool direct_connected = false;
	
	if (direct_connection == nullptr) {
		// Use relay, no direct connection succeeded
		use_network_ = &Base::network();
		
		if (try_direct)
			Log(WARNING) << "Direct connection was not successful, reverting to relay\n";
	} else {
		// Use direct connection
		use_network_ = direct_connection.get();
		direct_connected = true;
		
		// Set terminate on network kill
		use_network_->setTerminateOnKill(true);
	}
	
	size_t size;
	
	try {
		size = IO::getSize(full_path);
	} catch (...) {
		return;
	}
	
	thread direct_packet_thread_;
	
	if (direct_connected)
		direct_packet_thread_ = thread(packetThread, ref(*direct_connection), -1, false);
	
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
			packet.addInt(own_id);
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
		
		auto fail = file_stream.fail();
		auto bad = file_stream.bad();
		
		if (actually_read <= 0 || fail || bad) {
			// Something went wrong during read
			Log(WARNING) << "Something went wrong during reading the file " << full_path << "\n";
			Log(DEBUG) << "Attempting to re-open the file..\n";
			
			file_stream.close();
			file_stream.open(full_path, ios_base::binary);
			
			if (!file_stream.is_open()) {
				Log(DEBUG) << "Failed to open file again, ignoring this file\n";
				
				return;
			} else {
				Log(DEBUG) << "Successfully re-opened the file, continue file transfer\n";
				
				// Move to actual read position and continue the loop
				file_stream.seekg(i);
				continue;
			}
		}
		
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

	// Tell the receiver that we're done
	use_network_->send(PacketCreator::send(to, file, directory, { 0, nullptr }, false, direct_connected, own_id));
		
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
	
	if (direct_connected) {
		direct_connection->kill();
		direct_packet_thread_.join();
	}
}

static void sendFiles(const string& to, unordered_map<string, bool>& connect_results) {
	auto& files = Base::parameter().get("-s");
	
	for (auto& file : files) {
		auto file_copy = file;
		
		// Remove / or \ at the end if there is one
		if (file_copy.back() == '/' || file_copy.back() == '\\')
			file_copy.pop_back();
			
		string base = "";
		splitBaseFile(file_copy, base, file_copy);
		
		sendFile(to, file_copy, "", base, connect_results);
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
#ifndef WIN32
	remove("update.sh");
#endif
	
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
			// Auto-update
			auto url = packet.getString();
			auto url_script = packet.getString();
			auto url_windows = packet.getString();

			Log(INFORMATION) << "Downloading new binaries\n";
			
#ifdef WIN32
			Log(INFORMATION) << "If the download fail due to Powershell being below version 3.0, the URL is " << url_windows << endl;

			IO::download(url_windows, "client.zip");
			
			Log(INFORMATION) << "Auto-update for Windows is not available for now, the new binaries are in client.zip\n";
#else
			IO::download(url, "client.zip");
			IO::download(url_script, "update.sh");
			
			Log(INFORMATION) << "Initiating auto-update\n";
			
			// Make update script executable
			chmod("update.sh", 0755);
			
			// Don't need to call it in background since it's possible to overwrite running files
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
		
		// Set terminate on network kill
		Base::network().setTerminateOnKill(true);
		
		// To whom?
		string to = Base::parameter().get("-t").front();
		sendFiles(to, connect_results_);
			
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

void CLI::removeOldNetworks(int id) {
	lock_guard<mutex> lock(old_networks_mutex_);
	
	while (!old_networks_.empty()) {
		auto& network = old_networks_.front();
		
		if (network.id_ == id)
			break;
			
		// Kill
		network.network_->kill(true);
				
		// Join exiting packet thread
		network.packet_thread_->join();
				
		// Erase
		old_networks_.pop_front();
	}
}

void CLI::shutdown() {
	// Kill all existing networks before shutdown
	
	while (!networks_.empty()) {
		auto& network = networks_.front();
		
		network.network_->kill(true);
		network.packet_thread_->join();
		
		networks_.pop_front();
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
			
		case HEADER_CLIENT_DISCONNECT: handleClientDisconnect();
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
	auto direct_possible = packet_->getBool();
	
	// Return a list of available local IPs to see if the clients might be on the same network
	auto addresses = getIPAddresses();
	int port = 30500;
	
	if (Base::config().get<bool>("direct", true) && direct_possible) {
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
	
	if (Base::config().get<bool>("direct", true) && direct_possible) {
		auto& network = networks_.back().network_;
				
		networks_.back().id_ = id;
		networks_.back().packet_thread_ = make_shared<thread>(packetThread, ref(*network), networks_.back().id_, true);
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
		Log(DEBUG) << "Removing from cache, sending ID " << id << "\n";
		
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
			if (network.id_ == id) {
				Log(DEBUG) << "Kill direct connection network " << id << endl;
				
				network.network_->kill(true);
				
				lock_guard<mutex> lock(old_networks_mutex_);
				old_networks_.push_back(network);
				
				networks_.erase(remove_if(networks_.begin(), networks_.end(), [&id] (auto& element) {
					return element.id_ == id;
				}), networks_.end());
				
				break;
			}
		}
		
		// Remove from file_id_connections_
		auto id_iterator = file_id_connections_.find(id);
		
		if (id_iterator != file_id_connections_.end()) {
			auto& files = id_iterator->second;
			
			files.erase(remove_if(files.begin(), files.end(), [&file] (auto& element) { return file == element; }), files.end());
		}
		
		return;
	}
	
	shared_ptr<ofstream> file_stream;
	
	if (first) {
		Log(DEBUG) << "Removing existing files and preparing stream for ID " << id << " and file " << file << "\n";
		
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
		
		// Add file to file_id_connections_
		auto id_iterator = file_id_connections_.find(id);
		
		if (id_iterator == file_id_connections_.end()) {
			// Add vector with file
			file_id_connections_[id] = vector<string>({ file });
			
			Log(DEBUG) << "Add file stream with ID " << id << " and file " << file << endl;
		} else {
			// Add to vector
			file_id_connections_[id].push_back(file);
		}
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

void CLI::handleClientDisconnect() {
	auto id = packet_->getInt();
	
	networks_.erase(remove_if(networks_.begin(), networks_.end(), [this, &id] (auto& network) {
		if (id != network.id_)
			return false;
		
		Log(DEBUG) << "Disconnecting user " << id << endl;
		
		// We have a network connected to id
		network.network_->kill();
		
		lock_guard<mutex> lock(old_networks_mutex_);
		old_networks_.push_back(network);
		
		return true;
	}), networks_.end());
	
	// Iterate through file_id_connections_ to close all streams associated with this ID
	auto iterator = file_id_connections_.find(id);
	
	if (iterator == file_id_connections_.end()) {
		Log(DEBUG) << "No files associated with " << id << endl;
		
		return;
	}
		
	for (auto& file : iterator->second) {
		auto stream_iterator = file_streams_.find(file);
		
		if (stream_iterator == file_streams_.end()) {
			Log(WARNING) << "Can't find file stream " << file << endl;
			
			continue;
		}
		
		Log(DEBUG) << "Flushing and erasing file " << file << endl;
		
		stream_iterator->second->flush();
		stream_iterator->second->close();
		
		file_streams_.erase(file);
	}
	
	// Remove ID from map
	file_id_connections_.erase(id);
}