#include "Packet.h"
#include "Log.h"
#include "PartialPacket.h"

#include <array>
#include <cstring>

using namespace std;

Packet::Packet() : m_sent(0), m_read(0), m_finalized(false) {
    m_packet = make_shared<vector<unsigned char>>();
    
    // Insert header placeholders
    for (int i = 0; i < 4; i++)
        m_packet->push_back(0);
}

Packet::Packet(const unsigned char *buffer, const unsigned int size) : m_sent(0), m_read(0), m_finalized(false) {
    if(buffer == nullptr || size == 0) {
        Log(WARNING) << "Trying to create packet with empty buffer\n";
        
        return;
    }
    
    m_packet = make_shared<vector<unsigned char>>();
    
    m_packet->reserve(size + 4);
    
    // Insert header placeholders
    for (int i = 0; i < 4; i++)
        m_packet->push_back(0);
    
    m_packet->insert(m_packet->end(), buffer, buffer + size);
}

Packet::Packet(PartialPacket &&partialPacket) : m_sent(0), m_read(0), m_finalized(true) {
    m_packet = partialPacket.getData();
}

void Packet::addHeader(const unsigned char header) {
    if(isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet->push_back(header);
}

void Packet::addString(const string &str) {
    if(isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    addInt(str.length());
    m_packet->insert(m_packet->end(), str.begin(), str.end());
}

void Packet::addPointer(const unsigned char *ptr, const unsigned int size) {
    if(isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    if(ptr == nullptr || size == 0) {
        Log(ERROR) << "Trying to add a nullptr or size = 0 to packet\n";
    }
    
    addInt(size);
    
    m_packet->reserve(m_packet->size() + size);
    m_packet->insert(m_packet->end(), ptr, ptr + size);
}

void Packet::addBytes(const pair<size_t, const unsigned char*>& bytes) {
    if (isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    addInt(bytes.first);
    
    if (bytes.first == 0)
        return;

    m_packet->reserve(m_packet->size() + bytes.first);
    m_packet->insert(m_packet->end(), bytes.second, bytes.second + bytes.first);
}

void Packet::addInt(const int nbr) {
    if(isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet->push_back((nbr >> 24) & 0xFF);
    m_packet->push_back((nbr >> 16) & 0xFF);
    m_packet->push_back((nbr >> 8) & 0xFF);
    m_packet->push_back(nbr & 0xFF);
}

void Packet::addBool(const bool val) {
    if(isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet->push_back(val ? 1 : 0);
}

void Packet::addFloat(const float nbr) {
    if(isFinalized()) {
        Log(ERROR) << "Can't add anything to a finalized packet\n";
        
        return;
    }
    
    string floatString = to_string(nbr);
    
    m_packet->push_back(floatString.length());
    m_packet->insert(m_packet->end(), floatString.begin(), floatString.end());
}

float Packet::getFloat() {
    unsigned char length = m_packet->at(m_read++);
    
    string str(m_packet->begin() + m_read, m_packet->begin() + length + m_read);
    m_read += length;
    
    return stof(str);
}

bool Packet::getBool() {
    return m_packet->at(m_read++) == 1 ? true : false;
}

string Packet::getString() {
    unsigned int length = getInt();
    
    string str(m_packet->begin() + m_read, m_packet->begin() + m_read + length);
    m_read += length;
    
    return str;
}

const unsigned char* Packet::getData() const {
    if(!isFinalized()) {
        Log(ERROR) << "Can't return data from packet not finalized\n";
        
        return nullptr;
    }
    
    const unsigned char *data = m_packet->data();
    
    if(data == nullptr) {
        Log(ERROR) << "Trying to return data from packet when nullptr\n";
    }
    
    return data;
}

unsigned int Packet::getSize() const {
    if(!isFinalized()) {
        Log(ERROR) << "Can't return size of data from packet not finalized\n";
        
        return 0;
    }
    
    return m_packet->size();
}

unsigned int Packet::getSent() const {
    return m_sent;
}

unsigned char Packet::getByte() {
    try {
        return m_packet->at(m_read++);
    } catch(const out_of_range &e) {
        Log(ERROR) << "Trying to read beyond packet size, m_read = " << m_read << " packet size = " << m_packet->size() << endl;
        
        return 0;
    }
}

int Packet::getInt() {
    int nbr;
    
    try {
        nbr = (m_packet->at(m_read) << 24) | (m_packet->at(m_read + 1) << 16) | (m_packet->at(m_read + 2) << 8) | m_packet->at(m_read + 3); 
        m_read += 4;
    } catch(...) {
        Log(ERROR) << "Trying to read beyond packet size, m_read = " << m_read << " packet size = " << m_packet->size() << endl;
        
        return 0;
    }
    
    return nbr;
}

pair<size_t, const unsigned char*> Packet::getBytes() {
    auto size = getInt();
    
    auto* iterator = m_packet->data() + m_read;
    m_read += size;
    
    return { size, iterator };
}

void Packet::addSent(const int sent) {
    m_sent += sent;
}

bool Packet::fullySent() const {
    return m_sent >= m_packet->size();
}

bool Packet::isFinalized() const {
    return m_finalized;
}

void Packet::finalize() {
    if(isFinalized()) {
        Log(ERROR) << "Packet is already finalized\n";
        
        return;
    }
    
    unsigned int fullPacketSize = m_packet->size();
    array<unsigned int, 4> packetSize;
    
    packetSize[0] = (fullPacketSize >> 24) & 0xFF;
    packetSize[1] = (fullPacketSize >> 16) & 0xFF;
    packetSize[2] = (fullPacketSize >> 8) & 0xFF;
    packetSize[3] = fullPacketSize & 0xFF;
    
    for (size_t i = 0; i < packetSize.size(); i++)
        m_packet->at(i) = packetSize.at(i);
    
    m_finalized = true;
}

bool Packet::isEmpty() const {
    return isFinalized() ? m_packet->size() <= 4 : m_packet->empty();
}

void Packet::deepCopy(const Packet& packet) {
    m_sent = packet.m_sent;
    m_read = packet.m_read;
    m_finalized = packet.m_finalized;
    
    m_packet = make_shared<vector<unsigned char>>();
    
    // Deep copy vectors
    *(m_packet.get()) = *(packet.m_packet.get());
}

shared_ptr<vector<unsigned char>>& Packet::internal() {
    return m_packet;
}