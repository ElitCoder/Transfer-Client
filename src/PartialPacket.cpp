#include "PartialPacket.h"
#include "Log.h"

using namespace std;

PartialPacket::PartialPacket() : m_size(0) {
    m_packet = make_shared<vector<unsigned char>>();
}

unsigned int PartialPacket::getSize() const {
    return m_header.size() + m_packet->size();
}

void PartialPacket::setFullSize() {
    if(m_header.size() < 4) {
        Log(ERROR) << "Trying to set full size at partial packet when size < 4\n";
        
        return;
    }
    
    m_size = (m_header.front() << 24) | (m_header.at(1) << 16) | (m_header.at(2) << 8) | m_header.at(3); 
}

unsigned int PartialPacket::getFullSize() const {
    return m_size;
}

void PartialPacket::addData(const unsigned char *buffer, const unsigned int size) {
    if(buffer == nullptr || size == 0) {
        Log(ERROR) << "Trying to insert nullptr or size = 0 data in partial packet\n";
        
        return;
    }
    
    // Insert into m_header if the size is not obtained
    if (m_size == 0)
        m_header.insert(m_header.end(), buffer, buffer + size);
    else
        m_packet->insert(m_packet->end(), buffer, buffer + size);
    
    if(m_size == 0 && m_header.size() >= 4) {
        setFullSize();
    }
}

bool PartialPacket::hasHeader() const {
    return m_size != 0;
}

bool PartialPacket::isFinished() const {
    return m_size != 0 ? (m_header.size() + m_packet->size()) == m_size : false;
}

shared_ptr<vector<unsigned char>>& PartialPacket::getData() {
    return m_packet;
}