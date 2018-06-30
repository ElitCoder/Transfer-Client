#pragma once
#ifndef NETWORK_COMMUNICATION_H
#define NETWORK_COMMUNICATION_H

#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>

enum NetworkConstants {
    BUFFER_SIZE = 1048576
};

class Packet;
class PartialPacket;

class EventPipe {
public:
    explicit EventPipe();
    ~EventPipe();
    
    void setPipe();
    void resetPipe();
    
    int getSocket();
    
private:
    int mPipes[2];
    std::shared_ptr<std::mutex> event_mutex_;
};

class NetworkCommunication {
public:
    NetworkCommunication();
    ~NetworkCommunication();
    
    bool start(const std::string& hostname, unsigned short port, bool fast_fail = false, bool host = false);
    void acceptConnection();
    
    int getSocket() const;
    
    void send(const Packet& packet, bool wait = false);
    
    Packet* waitForPacket();
    void completePacket();
    
    PartialPacket& getPartialPacket();
    void moveCompletePartialPackets();
    
    Packet* getOutgoingPacket();
    void popOutgoingPacket();
    
    EventPipe& getPipe();
    void kill(bool safe = false);

private:
    bool hasFullPartialPacket() const;
    void pushPartialPacket(const PartialPacket& partial);
    PartialPacket& getFullPartialPacket();
    void popFullPartialPacket();
    
    int socket_ = -1;
    int host_socket_ = -1;
    
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
    
    std::atomic<bool> shutdown_;
    EventPipe pipe_;
};

#endif