#include "NetworkCommunication.h"
#include "Log.h"
#include "PartialPacket.h"
#include "Packet.h"

#include <cstring>
#include <errno.h>
#include <array>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Network
#include <sys/types.h>
#include <fcntl.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#endif

#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

using namespace std;

static void connect(const string& hostname, unsigned short port, int& server_socket) {
#ifdef WIN32
	WSADATA wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
		Log(ERROR) << "WSAStartup() failed\n";

		return;
	}
#endif
		
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if (server_socket < 0) {
        Log(ERROR) << "socket() failed\n";
        
#ifdef WIN32
		closesocket(server_socket);
#else
        close(server_socket);
#endif

        return;
    }
    
    sockaddr_in server;
	server.sin_family = AF_INET;
	hostent* hp = gethostbyname(hostname.c_str());
    
	if (!hp) {
        Log(ERROR) << "Could not find host " << hostname << endl;
        
#ifdef WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif

		return;
	}
	
	memcpy(reinterpret_cast<char*>(&server.sin_addr), reinterpret_cast<char*>(hp->h_addr), hp->h_length);
	server.sin_port = htons(port);
    
    size_t connection_try = 0;
    auto current_time = chrono::system_clock::now() + chrono::milliseconds(1500);
    
    while (connect(server_socket, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        if ((chrono::system_clock::now() - current_time).count() > 0) {
            connection_try++;
            Log(NETWORK) << "Could not connect, attempt #" << connection_try << endl;
            
            current_time += chrono::milliseconds(1500);
        }
        
        // Avoid spamming the kernel with connect()
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    int on = 1;
    
    if (setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on)) < 0)
        Log(WARNING) << "Could not set TCP_NODELAY\n";
    
    Log(NETWORK) << "Connected to " << hostname << endl;    
}

static unsigned int processBuffer(const unsigned char *buffer, const unsigned int received, PartialPacket &partialPacket) {
    if(partialPacket.hasHeader()) {        
        unsigned int insert = received;
        
        if(partialPacket.getSize() + received >= partialPacket.getFullSize()) {            
            insert = partialPacket.getFullSize() - partialPacket.getSize();
        }
        
        if(insert > received) {
            Log(ERROR) << "Trying to insert more data than available in buffer, insert = " << insert << ", received = " << received << endl;
        }
        
        partialPacket.addData(buffer, insert);
                
        return insert;
    }
    
    else {
        unsigned int leftHeader = 4 - partialPacket.getSize();
        unsigned int adding = received > leftHeader ? leftHeader : received;
        
        if(adding > received) {
            Log(ERROR) << "Trying to insert more data than available in buffer, adding = " << adding << ", received = " << received << endl;
        }
        
        partialPacket.addData(buffer, adding);
        
        return adding;
    }
}

static void receiveThread(NetworkCommunication& network) {
    array<unsigned char, NetworkConstants::BUFFER_SIZE> buffer;
    
    while (true) {
#ifdef WIN32
		int received = recv(network.getSocket(), (char*)buffer.data(), NetworkConstants::BUFFER_SIZE, 0);
#else
        int received = recv(network.getSocket(), buffer.data(), NetworkConstants::BUFFER_SIZE, 0);
#endif
        
        if(received <= 0) {
            Log(NETWORK) << "Receiving thread got error: " << strerror(errno) << endl;
            
            break;
        }
                
        int processed = 0;
    
        do {
            processed += processBuffer(buffer.data() + processed, received - processed, network.getPartialPacket());
        } while (processed < received);
        
        network.moveCompletePartialPackets();
    }
    
    Log(NETWORK) << "Receiving thread lost connection to server!\n";
}

static void sendThread(NetworkCommunication& network) {
    while (true) {
        Packet &packet = network.getOutgoingPacket();
        
        int sending = min((unsigned int)NetworkConstants::BUFFER_SIZE, packet.getSize() - packet.getSent());

#ifdef WIN32
		int sent = send(network.getSocket(), (const char*)(packet.getData() + packet.getSent()), sending, 0);
#else
        int sent = send(network.getSocket(), packet.getData() + packet.getSent(), sending, 0);
#endif
        
        if(sent <= 0)
            break;
        
        packet.addSent(sent);
        
        if (packet.fullySent())
            network.popOutgoingPacket();
    }
    
    Log(NETWORK) << "Sending thread lost connection to server!\n";
}

NetworkCommunication::NetworkCommunication() {}

NetworkCommunication::~NetworkCommunication() {
    lock_guard<mutex> incoming_guard(incoming_mutex_);
    lock_guard<mutex> outgoing_guard(outgoing_mutex_);
    
    incoming_packets_.clear();
    outgoing_packets_.clear();
    partial_packets_.clear();
}

void NetworkCommunication::start(const string& hostname, unsigned short port) {
    connect(hostname, port, socket_);
        
    receive_thread_ = thread(receiveThread, ref(*this));
    send_thread_ = thread(sendThread, ref(*this));
}

Packet& NetworkCommunication::waitForPacket() {
    unique_lock<mutex> lock(incoming_mutex_);
    incoming_cv_.wait(lock, [this] { return !incoming_packets_.empty(); });
	
	return incoming_packets_.front();
}

void NetworkCommunication::completePacket() {
    lock_guard<mutex> guard(incoming_mutex_);
    
    incoming_packets_.pop_front();
}

void NetworkCommunication::send(const Packet& packet, bool wait) {
    unique_lock<mutex> lock(outgoing_mutex_);
    
    if (wait)
        send_queue_cv_.wait(lock, [this] { return outgoing_packets_.size() < 10; });
    
    outgoing_packets_.push_back(packet);
    outgoing_cv_.notify_one();
}

int NetworkCommunication::getSocket() const {
    return socket_;
}

PartialPacket& NetworkCommunication::getPartialPacket() {
    if (partial_packets_.empty() || partial_packets_.back().isFinished())
        pushPartialPacket(PartialPacket());
    
    return partial_packets_.back();
}

bool NetworkCommunication::hasFullPartialPacket() const {
    return partial_packets_.empty() ? false : partial_packets_.front().isFinished();
}

void NetworkCommunication::moveCompletePartialPackets() {
    if (!hasFullPartialPacket())
        return;
    
    lock_guard<mutex> guard(incoming_mutex_);
    
    while (hasFullPartialPacket()) {
        incoming_packets_.push_back(move(getFullPartialPacket()));
        popFullPartialPacket();
    }
    
    incoming_cv_.notify_one();
}

void NetworkCommunication::pushPartialPacket(const PartialPacket &partial) {
    partial_packets_.push_back(partial);
}

PartialPacket& NetworkCommunication::getFullPartialPacket() {
    return partial_packets_.front();
}

void NetworkCommunication::popFullPartialPacket() {
    partial_packets_.pop_front();
}

Packet& NetworkCommunication::getOutgoingPacket() {
    unique_lock<mutex> lock(outgoing_mutex_);
    outgoing_cv_.wait(lock, [this] { return !outgoing_packets_.empty(); });
    
    return outgoing_packets_.front();
}

void NetworkCommunication::popOutgoingPacket() {
    lock_guard<mutex> guard(outgoing_mutex_);
    
    outgoing_packets_.pop_front();
    send_queue_cv_.notify_one();
}