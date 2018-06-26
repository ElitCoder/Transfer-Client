#include "CLI.h"
#include "Base.h"
#include "Parameter.h"
#include "Config.h"
#include "NetworkCommunication.h"
#include "PacketCreator.h"
#include "Packet.h"

#include <fstream>

using namespace std;

static void monitoring() {
	auto name = Base::config().get<string>("name", "");
	
	// Register at Server as monitoring with certain name
	Base::network().send(PacketCreator::join(name));
	
	Log(INFORMATION) << "Registering at Server..\n";
}

static void sendFiles(const string& to) {
	auto& files = Base::parameter().get("-s");
	
	if (files.size() > 1) {
		Log(ERROR) << "It's only supported to send one file for now\n";
		
		return;
	}
	
	auto& file = files.front();
	
	// Inform target of file transfer
	Base::network().send(PacketCreator::inform(to, file));
	auto answer = Base::cli().waitForAnswer();
	auto accepted = answer.getBool();
	
	if (!accepted) {
		Log(ERROR) << "Receiving side did not accept the file transfer or is not connected\n";
		
		return;
	}
	
	// Send the file
	ifstream file_stream(file, ios_base::binary);
	
	if (!file_stream.is_open()) {
		Log(ERROR) << "The file " << file << " could not be opened\n";
		
		return;
	}
	
	file_stream.seekg(0, ios_base::end);
	size_t size = file_stream.tellg();
	file_stream.seekg(0, ios_base::beg);
	
	Log(DEBUG) << "File size " << size << " bytes\n";
	
	for (size_t i = 0; i < size;) {
		int buffer_size = 4 * 1024 * 1024; // 4 MB
		size_t read_amount = min(buffer_size, (int)(size - i));
		
		vector<unsigned char> data;
		data.resize(read_amount);
		
		file_stream.read((char*)&data[0], read_amount);
		
		Log(DEBUG) << "Sending " << read_amount << " bytes, fp: " << i << "\n";

		Base::network().send(PacketCreator::send(to, file, data, i == 0), true);
		i += read_amount;
		
		answer = Base::cli().waitForAnswer();
		accepted = answer.getBool();
		
		if (!accepted)
			Log(WARNING) << "Something went wrong during file transfer\n";
	}
	
	// Tell the receiver that we're done
	Base::network().send(PacketCreator::send(to, file, {}, false), true);
	
	Log(DEBUG) << "Waiting for answer\n";
	
	answer = Base::cli().waitForAnswer();
	accepted = answer.getBool();
	
	if (accepted)
		Log(INFORMATION) << "File successfully sent\n";
	else
		Log(ERROR) << "File could not be sent\n";
}

void CLI::start() {
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

void CLI::process(Packet& packet) {
	auto header = packet.getByte();
	
	packet_ = &packet;
	
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
			
		default: {
			Log(WARNING) << "Unknown packet header ";
			printf("%02X", header);
			Log(NONE) << "\n";
		}
	}
}

void CLI::handleJoin() {
	auto result = packet_->getBool();
	
	if (result)
		Log(INFORMATION) << "Accepted at Server\n";
	else
		Log(WARNING) << "Server did not accept our connection\n";
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
	lock_guard<mutex> lock(answer_mutex_);
	answer_packet_ = make_shared<Packet>(*packet_);
	answer_cv_.notify_one();
}

static string cleanFilename(string input, const string& delimiter) {
	size_t pos = 0;
	string token;
	
	while ((pos = input.find(delimiter)) != string::npos) {
	    token = input.substr(0, pos);
	    input.erase(0, pos + delimiter.length());
	}
	
	return input;
}

void CLI::handleSend() {
	// TODO: Receive file here
	auto id = packet_->getInt();
	auto file = packet_->getString();
	auto bytes = packet_->getBytes();
	auto first = packet_->getBool();
	
	if (bytes.empty()) {
		Log(DEBUG) << "Removing from cache\n";
		
		// Remove from cache
		auto iterator = file_streams_.find(file);
		
		if (iterator != file_streams_.end())
			iterator->second->close();
		
		file_streams_.erase(file);
	
		// Send result that we're done
		Base::network().send(PacketCreator::sendResult(id, true));
		
		return;
	}
	
	// Strip file to only filename
	file = cleanFilename(file, "/");
	
	shared_ptr<ofstream> file_stream;
	
	if (first) {
		// Remove any existing files
		if (system(NULL)) {
			string command = "rm -f " + file;
			
			if (system(command.c_str())) {}
		} else {
			Log(WARNING) << "Shell not available\n";
		}
		
		shared_ptr<ofstream> stream_pointer = make_shared<ofstream>(file, ios::binary | ios::app);
		
		// Add file stream to cache
		file_streams_[file] = stream_pointer;
		//file_stream = stream_pointer;
	}
	
	// Find stream in cache
	auto iterator = file_streams_.find(file);
	
	if (iterator == file_streams_.end())
		Log(WARNING) << "Could not find file stream\n";
		
	file_stream = iterator->second;
		
	if (!file_stream->is_open()) {
		Log(WARNING) << "Could not open " << file << " for writing\n";
		
		return;
	} else {
		Log(INFORMATION) << "Opened " << file << endl;
	}
	
	Log(DEBUG) << "Writing file " << bytes.size() << " bytes\n";
	
	const auto& data = bytes.data();
	file_stream->write((const char*)&data[0], bytes.size());
	
	Log(DEBUG) << "Wrote file\n";
	
	//file_stream->close();
	
	// Send OK to sender
	Base::network().send(PacketCreator::sendResult(id, true));
}

void CLI::handleSendResult() {
	Log(DEBUG) << "GOT SEND RESULT\n";
	
	lock_guard<mutex> lock(answer_mutex_);
	answer_packet_ = make_shared<Packet>(*packet_);
	answer_cv_.notify_one();
}