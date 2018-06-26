#pragma once
#ifndef PACKET_H
#define PACKET_H

//#include "PartialPacket.h"

#include <string>
#include <vector>

class PartialPacket;

class Packet {
public:
    Packet();
    Packet(const unsigned char *buffer, const unsigned int size);
    
    Packet(PartialPacket &&partialPacket);
    
    void addHeader(const unsigned char header);
    void addString(const std::string &str);
    void addPointer(const unsigned char *ptr, const unsigned int size);
    void addInt(const int nbr);
    void addFloat(const float nbr);
    void addBool(const bool val);
    void addBytes(const std::vector<unsigned char>& bytes);
    
    unsigned char getByte();
    int getInt();
    float getFloat();
    std::string getString();
    bool getBool();
    std::vector<unsigned char> getBytes();
    
    const unsigned char* getData() const;
    unsigned int getSize() const;
    unsigned int getSent() const;
    void addSent(const int sent);
    bool fullySent() const;
    
    bool isEmpty() const;
    void finalize();
    
private:
    bool isFinalized() const;
    
    std::vector<unsigned char> m_packet;
    unsigned int m_sent, m_read;
    
    bool m_finalized;
};

#endif