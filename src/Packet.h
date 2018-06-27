#pragma once
#ifndef PACKET_H
#define PACKET_H

#include <string>
#include <vector>
#include <memory>

class PartialPacket;

class Packet {
public:
    Packet();
    Packet(const unsigned char *buffer, const unsigned int size);
    
    Packet(PartialPacket &&partialPacket);
    
    void addHeader(const unsigned char header);
    void addString(const std::string &str);
    void addInt(const int nbr);
    void addFloat(const float nbr);
    void addBool(const bool val);
    void addBytes(const std::pair<size_t, const unsigned char*>& bytes);
    
    unsigned char getByte();
    int getInt();
    float getFloat();
    std::string getString();
    bool getBool();
    std::pair<size_t, const unsigned char*> getBytes();
    
    const unsigned char* getData() const;
    unsigned int getSize() const;
    unsigned int getSent() const;
    void addSent(const int sent);
    bool fullySent() const;
    
    bool isEmpty() const;
    void finalize();
    
    void deepCopy(const Packet& packet);
    
    std::shared_ptr<std::vector<unsigned char>>& internal();
    
private:
    bool isFinalized() const;
    
    std::shared_ptr<std::vector<unsigned char>> m_packet;
    unsigned int m_sent, m_read;
    
    bool m_finalized;
};

#endif