#pragma once
#ifndef NETWORK_COMMUNICATION_H
#define NETWORK_COMMUNICATION_H

#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>

enum NetworkConstants {
    BUFFER_SIZE = 65536
};

class Packet;
class PartialPacket;

class NetworkCommunication {
public:
    NetworkCommunication();
    ~NetworkCommunication();
    
    void start(const std::string& hostname, unsigned short port);
    int getSocket() const;
    
    void send(const Packet& packet, bool wait = false);
    
    void waitForPacket();
    Packet* getPacket();
    bool hasPacket();
    void completePacket();
    
    PartialPacket& getPartialPacket();
    void moveCompletePartialPackets();
    
    Packet& getOutgoingPacket();
    void popOutgoingPacket();
    
private:
    bool hasFullPartialPacket() const;
    void pushPartialPacket(const PartialPacket& partial);
    PartialPacket& getFullPartialPacket();
    void popFullPartialPacket();
    
    int socket_;
    
    std::thread receive_thread_;
    std::thread send_thread_;
    
    std::mutex incoming_mutex_;
    std::condition_variable incoming_cv_;
    std::list<Packet> incoming_packets_;
    
    std::mutex outgoing_mutex_;
    std::condition_variable outgoing_cv_;
    std::list<Packet> outgoing_packets_;
    
    std::condition_variable send_queue_cv_;
    
    std::list<PartialPacket> partial_packets_;
};

#endif