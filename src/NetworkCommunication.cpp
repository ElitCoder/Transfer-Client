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

static bool hostConnection(int& server_socket, unsigned short port) {
	server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	if (server_socket < 0) {
		Log(ERROR) << "socket() failed\n";
		
		return false;
	}
	
	int on = 1;
    
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on)) < 0) {
        Log(ERROR) << "Not reusable address\n";
        
		close(server_socket);
        return false;
    }
	
	sockaddr_in socketInformation;
    socketInformation.sin_family = AF_INET;
    socketInformation.sin_addr.s_addr = INADDR_ANY;
    socketInformation.sin_port = htons(port);
    
    if(bind(server_socket, reinterpret_cast<sockaddr*>(&socketInformation), sizeof(socketInformation)) < 0) {
        Log(ERROR) << "bind() failed\n";
        
        close(server_socket);
        return false;
    }
	
	size_t socketInformationSize = sizeof(socketInformation);

	if(getsockname(server_socket, reinterpret_cast<sockaddr*>(&socketInformation), reinterpret_cast<socklen_t*>(&socketInformationSize)) < 0) {
        Log(ERROR) << "Could not get address information\n";
        
        close(server_socket);
        return false;
	}
	
	listen(server_socket, 1);
	
	return true;
}

static bool connect(const string& hostname, unsigned short port, int& server_socket, bool fast_fail) {
#ifdef WIN32
	WSADATA wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
		Log(ERROR) << "WSAStartup() failed\n";

		return false;
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

        return false;
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

		return false;
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
		
		// Only allow one connection attempt in fast fail mode
		if (fast_fail) {
			close(server_socket);
						
			return false;
		}
        
        // Avoid spamming the kernel with connect()
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    int on = 1;
    
    if (setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on)) < 0)
        Log(WARNING) << "Could not set TCP_NODELAY\n";
    
    Log(NETWORK) << "Connected to " << hostname << endl;
	
	return true;
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

static void setFileDescriptorsReceive(int main_socket, int pipe_socket, fd_set& readSet, fd_set& errorSet) {
	FD_ZERO(&readSet);
	FD_ZERO(&errorSet);
	
	FD_SET(main_socket, &readSet);
	FD_SET(main_socket, &errorSet);
	
	FD_SET(pipe_socket, &readSet);
	FD_SET(pipe_socket, &errorSet);
}

static void receiveThread(NetworkCommunication& network) {
    array<unsigned char, NetworkConstants::BUFFER_SIZE> buffer;
	fd_set readSet;
	fd_set errorSet;
	
    while (true) {
		setFileDescriptorsReceive(network.getSocket(), network.getPipe().getSocket(), readSet, errorSet);
		
		if (select(FD_SETSIZE, &readSet, NULL, &errorSet, NULL) == 0)
			break;

		// Shutdown
		if (FD_ISSET(network.getPipe().getSocket(), &readSet))
			break;
		
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
    
    Log(NETWORK) << "receiveThread exiting\n";
	
	// Kill the rest
	network.kill();
}

static void sendThread(NetworkCommunication& network) {
    while (true) {
        auto* packet_pointer = network.getOutgoingPacket();

		// Shutdown
		if (packet_pointer == nullptr)
			break;
			
		// We know it's a packet
		auto& packet = *packet_pointer;

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
    
    Log(NETWORK) << "sendThread exiting\n";
	
	// Kill the rest
	network.kill();
}

NetworkCommunication::NetworkCommunication() {
	shutdown_ = false;
}

NetworkCommunication::~NetworkCommunication() {	
	kill();
	
	if (receive_thread_.joinable())
		receive_thread_.join();
				
	if (send_thread_.joinable())
		send_thread_.join();
				
	if (socket_ >= 0)
		close(socket_);
	
	if (host_socket_ >= 0)
		close(host_socket_);
}

void NetworkCommunication::acceptConnection() {
	socket_ = accept(host_socket_, 0, 0);
	
	if (socket_ < 0) {
		Log(WARNING) << "accept() failed\n";
		
		close(host_socket_);
		host_socket_ = -1;
		return;
	}
	
	receive_thread_ = thread(receiveThread, ref(*this));
    send_thread_ = thread(sendThread, ref(*this));
}

bool NetworkCommunication::start(const string& hostname, unsigned short port, bool fast_fail, bool host) {
	if (host) {
		return hostConnection(host_socket_, port);
	} else {
		if (!connect(hostname, port, socket_, fast_fail))
			return false;
	}
        
    receive_thread_ = thread(receiveThread, ref(*this));
    send_thread_ = thread(sendThread, ref(*this));
	
	return true;
}

Packet* NetworkCommunication::waitForPacket() {
    unique_lock<mutex> lock(incoming_mutex_);
    incoming_cv_.wait(lock, [this] { return !incoming_packets_.empty() || shutdown_; });
	
	// Shutdown
	if (shutdown_)
		return nullptr;
		
	return &incoming_packets_.front();
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

Packet* NetworkCommunication::getOutgoingPacket() {
    unique_lock<mutex> lock(outgoing_mutex_);
    outgoing_cv_.wait(lock, [this] { return !outgoing_packets_.empty() || shutdown_; });
    
	if (shutdown_)
		return nullptr;

    return &outgoing_packets_.front();
}

void NetworkCommunication::popOutgoingPacket() {
    lock_guard<mutex> guard(outgoing_mutex_);
    
    outgoing_packets_.pop_front();
    send_queue_cv_.notify_all();
}

void NetworkCommunication::kill(bool safe) {
	// Shutdown already called
	if (shutdown_)
		return;
		
	// Safe shutdown - wait for all packets to be sent before exiting
	if (safe) {
		unique_lock<mutex> lock(outgoing_mutex_);
		send_queue_cv_.wait(lock, [this] { return outgoing_packets_.empty(); });
	}

	lock_guard<mutex> in_lock(incoming_mutex_);
	lock_guard<mutex> out_lock(outgoing_mutex_);
	
	shutdown_ = true;
	pipe_.setPipe();
	
	incoming_cv_.notify_all();
	outgoing_cv_.notify_all();
}

EventPipe& NetworkCommunication::getPipe() {
	return pipe_;
}

/*
    EventPipe
*/

EventPipe::EventPipe() {
    event_mutex_ = make_shared<mutex>();
    
    if(pipe(mPipes) < 0) {
        Log(ERROR) << "Failed to create pipe, won't be able to wake threads, errno = " << errno << '\n';
    }
    
    if(fcntl(mPipes[0], F_SETFL, O_NONBLOCK) < 0) {
        Log(WARNING) << "Failed to set pipe non-blocking mode\n";
    }
}

EventPipe::~EventPipe() {
    if (mPipes[0] >= 0)
        close(mPipes[0]);
    
    if (mPipes[1] >= 0)
        close(mPipes[1]);
}

void EventPipe::setPipe() {
    lock_guard<mutex> lock(*event_mutex_);
    
    if (write(mPipes[1], "0", 1) < 0)
        Log(ERROR) << "Could not write to pipe, errno = " << errno << '\n';
}

void EventPipe::resetPipe() {
    lock_guard<mutex> lock(*event_mutex_);
    
    unsigned char buffer;

    while(read(mPipes[0], &buffer, 1) == 1)
        ;
}

int EventPipe::getSocket() {
    lock_guard<mutex> lock(*event_mutex_);
    
    return mPipes[0];
}