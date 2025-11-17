#ifndef MY_NETWORK_CLASS
#define MY_NETWORK_CLASS

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <vector>

// Default should be network order byte, so need to change to host byte order for executing
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
    std::string qName; // codecrafters.io or google.com, parse later
    uint16_t qType;
    uint16_t qClass;
};
struct DNSAnswer
{
    std::string name; // codecrafters.io or google.com, parse later
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t rdLength;
    std::string rData;
};
struct DNSMessage // Flexible with the number of questions and answers
{
    DNSHeader header;
    std::vector<DNSQuestion> questions;
    std::vector<DNSAnswer> answers;
};

// Serialize the name into labels, codecrafters.io -> \x0ccodecrafters\x02io\x00
// Not dealing with compressed packet now
std::string serializeName(std::string name)
{
    std::string result = "";
    std::string temp = "";
    uint8_t len = 0; // length of each octet(label)
    for (int i = 0; i < name.length(); i++)
    {
        if (len > 63)
        {
            std::cerr << "Invalid length for each label for serializ"
                      << "ing,maximum allowed is 63, receive " << len << " characters." << std::endl;
        }
        if (name[i] == '.')
        {

            result.push_back(len);
            result += temp;
            len = 0;
            temp = "";
        }
        else if (name[i] != '.')
        {
            len++;
            temp += name[i];
        }
    }
    // Final label
    result.push_back(len);
    result += temp;
    // Add null terminate
    result.push_back('\0');
    return result;
}
// Parse the labels into name \x0ccodecrafters\x02io\x00 -> codecrafters.io
// Only works with correct serialized format from beginning
// Store total_len for future use
std::string parseName(char *buffer, size_t &pos, size_t &total_len, bool pointer)
{
    // if (pointer)
    // { // deal differently if it is pointer in compressed packet

    //     return "";
    // }
    std::string result = "";
    size_t i = pos;
    while (uint8_t(buffer[i]) != 0)
    { // parse each label by taking out the labels
        uint8_t len = uint8_t(buffer[i]);

        if (i != pos)
            result += '.';
        // if (len & 0b11000000) // this indicate it is pointer
        // {
        //     // Find offset
        //     uint16_t pointer = len << 8 + uint8_t(name[i + 1]);
        //     uint16_t offset = pointer & ~(0b11000000 << 8);
        //     i += 2;
        //     continue;
        // }
        for (int j = 1; j <= len; j++)
        {
            result += buffer[i + j];
        }
        i += len + 1;
    }
    // Acount for \0
    i++;
    total_len = i - pos;
    pos = i;
    return result;
}

// Serialize the message into buffer and current offset len.
void serializeDNSMessage(char *dest, DNSMessage &src, size_t &pos)
{
    // Serialize header section
    memcpy(dest + pos, &(src.header), sizeof(src.header));
    pos += sizeof(src.header);

    // Serialize question section
    size_t numQ = ntohs(src.header.qdCount);
    // Serialize each question
    for (int i = 0; i < numQ; i++)
    {
        const DNSQuestion &q = src.questions[i];
        std::string temp = serializeName(q.qName);
        memcpy(dest + pos, temp.data(), (size_t)temp.length());
        pos += temp.length();
        memcpy(dest + pos, &(q.qType), sizeof(uint16_t));
        pos += sizeof(uint16_t);
        memcpy(dest + pos, &(q.qClass), sizeof(uint16_t));
        pos += sizeof(uint16_t);
    }

    // Serialize each answer
    size_t numA = ntohs(src.header.anCount);
    for (int i = 0; i < numA; i++)
    {
        const DNSAnswer &a = src.answers[i];
        std::string temp = serializeName(a.name);
        memcpy(dest + pos, temp.data(), (size_t)temp.length());
        pos += temp.length();
        memcpy(dest + pos, &(a.type), sizeof(uint16_t));
        pos += sizeof(uint16_t);
        memcpy(dest + pos, &(a._class), sizeof(uint16_t));
        pos += sizeof(uint16_t);
        memcpy(dest + pos, &(a.ttl), sizeof(uint32_t));
        pos += sizeof(uint32_t);
        memcpy(dest + pos, &(a.rdLength), sizeof(uint16_t));
        pos += sizeof(uint16_t);
        memcpy(dest + pos, a.rData.data(), (size_t)a.rData.length());
        pos += a.rData.length();
    }
}

// Parse the byte buffer into DNSMessage
void parseDNSMessage(DNSMessage &dest, char *src, size_t &pos)
{
    // Parse header section, mimic ID, Opcode and RD only, else turn 0 except QR

    memcpy(&dest.header, src + pos, sizeof(DNSHeader));
    pos += sizeof(DNSHeader);

    // Parse the question section,
    // Parse each question
    size_t numQ = ntohs(dest.header.qdCount);
    for (int i = 0; i < numQ; i++)
    {
        DNSQuestion q;
        size_t label_len = 0;
        q.qName = parseName(src, pos, label_len, false);
        memcpy(&q.qType, src + pos, sizeof(uint16_t));
        pos += sizeof(uint16_t);
        memcpy(&q.qClass, src + pos, sizeof(uint16_t));
        pos += sizeof(uint16_t);
        dest.questions.push_back(q);
    }
}

#endif