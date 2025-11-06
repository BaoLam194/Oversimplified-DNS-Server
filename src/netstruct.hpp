#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <cstring>

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

struct DNSMessage
{
    DNSHeader header;
    DNSQuestion *questions;
};

// Parse the message into buffer and current offset len.
void serializeDNSMessage(DNSMessage &message, char *buffer, size_t &len)
{
    // Parse header section
    memcpy(buffer + len, &(message.header), sizeof(message.header));
    len += sizeof(message.header);

    // Parse question section
    uint16_t num = ntohs(message.header.qdCount);
    // Parse each question
    for (int i = 0; i < num; i++)
    {
        DNSQuestion &q = message.questions[i];
        memcpy(buffer + len, q.qName.data(), (size_t)q.qName.length());
        len += q.qName.length();
        memcpy(buffer + len, &(q.qType), sizeof(uint16_t));
        len += sizeof(uint16_t);
        memcpy(buffer + len, &(q.qClass), sizeof(uint16_t));
        len += sizeof(uint16_t);
    }
}