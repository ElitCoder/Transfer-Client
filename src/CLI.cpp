#include "CLI.h"
#include "Base.h"
#include "Parameter.h"
#include "Config.h"
#include "NetworkCommunication.h"
#include "PacketCreator.h"
#include "Packet.h"
#include "Timer.h"

#include <fstream>
#include <filesystem>

using namespace std;

static void monitoring() {
	auto name = Base::config().get<string>("name", "");
	
	// Register at Server as monitoring with certain name
	Base::network().send(PacketCreator::join(name));
	
	Log(INFORMATION) << "Registering at Server..\n";
}

static void sendFiles(const string& to) {
	auto& files = Base::parameter().get("-s");
	
	for (auto& file : files) {
		// Is file a directory?
		if (filesystem::is_directory(file)) {
			Log(WARNING) << "Recursive sending is not available for now.\n";
			
			continue;
		}
		
		// Inform target of file transfer
		Base::network().send(PacketCreator::inform(to, file));
		auto answer = Base::cli().waitForAnswer();
		auto accepted = answer.getBool();
		
		if (!accepted) {
			Log(ERROR) << "Receiving side did not accept the file transfer or is not connected\n";
			
			continue;
		}
		
		// Send the file
		ifstream file_stream(file, ios_base::binary);
		
		if (!file_stream) {
			Log(ERROR) << "The file " << file << " could not be opened\n";
			
			continue;
		}
		
		file_stream.seekg(0, ios_base::end);
		size_t size = file_stream.tellg();
		file_stream.seekg(0, ios_base::beg);
		
		Log(DEBUG) << "File size " << size << " bytes\n";
		
		Timer timer;
		
		for (size_t i = 0; i < size;) {
			int buffer_size = 4 * 1024 * 1024; // 4 MB
			size_t read_amount = min(buffer_size, (int)(size - i));
			
			vector<unsigned char> data;
			data.resize(read_amount);
			
			file_stream.read((char*)&data[0], read_amount);
			auto actually_read = file_stream.gcount();
			data.resize(actually_read);
			
			Log(DEBUG) << "Sending " << actually_read << " bytes, fp: " << i << "\n";

			Base::network().send(PacketCreator::send(to, file, data, i == 0), true);
			i += actually_read;
			
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
		
		auto elapsed_time = timer.restart();
		
		if (accepted)
			Log(INFORMATION) << "File successfully sent\n";
		else
			Log(ERROR) << "File could not be sent\n";
			
		Log(DEBUG) << "Elapsed time: " << elapsed_time << " seconds\n";
		Log(DEBUG) << "Speed: " << (static_cast<double>(size) / 1024 / 1024) / elapsed_time << " MB/s\n";
	}
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

void CLI::notifyWaiting() {
	lock_guard<mutex> lock(answer_mutex_);
	answer_packet_ = make_shared<Packet>(*packet_);
	answer_cv_.notify_one();
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
	notifyWaiting();
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
	
	// Add folder ID if the option is enabled
	if (Base::config().has("output_folder"))
		file = Base::config().get<string>("output_folder", "") + "/" + file;
	
	shared_ptr<ofstream> file_stream;
	
	if (first) {
		Log(DEBUG) << "Removing existing files and preparing stream\n";
		
		// Create folder if it does not exist
		if (Base::config().has("output_folder"))
			filesystem::create_directory(Base::config().get<string>("output_folder", ""));
		
		// Remove any existing files
		remove(file.c_str());
		
		shared_ptr<ofstream> stream_pointer = make_shared<ofstream>(file, ios::binary | ios::app);
		
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
	
	Log(DEBUG) << "Writing file " << file << " with " << bytes.size() << " bytes\n";
	
	const auto& data = bytes.data();
	file_stream->write((const char*)&data[0], bytes.size());
	file_stream->flush();
	
	// Send OK to sender
	Base::network().send(PacketCreator::sendResult(id, true));
}

void CLI::handleSendResult() {
	notifyWaiting();
}