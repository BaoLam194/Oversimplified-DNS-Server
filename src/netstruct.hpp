#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>

// Default should be network order byte, so need to change to host byte order before changing
struct DNSHeader
{
    uint16_t transactionId;
    uint16_t flags;
    uint16_t qdCount;
    uint16_t anCount;
    uint16_t nsCount;
    uint16_t arCount;
};

struct DNSQuestion
{
    std::string qName; // Not include the \x00 now
    uint16_t qType;
    uint16_t qClass;
};
struct DNSAnswer
{
    std::string name; // Not include the \x00 now
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t rdLength;
    std::string rData;
};
struct DNSMessage
{
    DNSHeader header;
    DNSQuestion *questions;
    DNSAnswer *answers;
};

// Serialize the message into buffer and current offset len.
void serializeDNSMessage(DNSMessage &message, char *buffer, size_t &len)
{
    // Serialize header section
    memcpy(buffer + len, &(message.header), sizeof(message.header));
    len += sizeof(message.header);

    // Serialize question section
    size_t numQ = ntohs(message.header.qdCount);
    // Serialize each question
    for (int i = 0; i < numQ; i++)
    {
        DNSQuestion &q = message.questions[i];
        memcpy(buffer + len, q.qName.data(), (size_t)q.qName.length());
        len += q.qName.length();
        memcpy(buffer + len, &(q.qType), sizeof(uint16_t));
        len += sizeof(uint16_t);
        memcpy(buffer + len, &(q.qClass), sizeof(uint16_t));
        len += sizeof(uint16_t);
    }

    // Serialize each answer
    size_t numA = ntohs(message.header.anCount);
    for (int i = 0; i < numA; i++)
    {
        DNSAnswer &a = message.answers[i];
        memcpy(buffer + len, a.name.data(), (size_t)a.name.length());
        len += a.name.length();
        memcpy(buffer + len, &(a.type), sizeof(uint16_t));
        len += sizeof(uint16_t);
        memcpy(buffer + len, &(a._class), sizeof(uint16_t));
        len += sizeof(uint16_t);
        memcpy(buffer + len, &(a.ttl), sizeof(uint16_t));
        len += sizeof(uint32_t);
        memcpy(buffer + len, &(a.rdLength), sizeof(uint16_t));
        len += sizeof(uint16_t);
        memcpy(buffer + len, a.rData.data(), (size_t)a.rData.length());
        len += a.rData.length();
    }
}