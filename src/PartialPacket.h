#pragma once
#ifndef PARTIAL_PACKET_H
#define PARTIAL_PACKET_H

#include <vector>
#include <memory>

class PartialPacket {
public:
    PartialPacket();
    
    unsigned int getFullSize() const;
    unsigned int getSize() const;
    void addData(const unsigned char *buffer, const unsigned int size);
    bool hasHeader() const;
    bool isFinished() const;
    std::shared_ptr<std::vector<unsigned char>>& getData();
    
private:
    void setFullSize();
    
    std::shared_ptr<std::vector<unsigned char>> m_packet;
    std::vector<unsigned char> m_header;
    
    unsigned int m_size;
};

#endif