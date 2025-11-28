/////////////////////////////////////////////
//////////       server side        /////////
/////////////////////////////////////////////
// Acts as an intermediate server to forward query to higher level server.
#include <iostream>
#include <cstring>
#include "netstruct.hpp"
#include <fstream>
#include <vector>

bool parse_ip_address(uint32_t &dst_ip, uint16_t &dst_port, std::string src, std::string &error_mes);

int main(int argc, char **argv)
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Disable output buffering
    setbuf(stdout, NULL);

    std::cout << "Starting the server..." << std::endl;

    if (argc != 3)
    {
        std::cerr << "Your are giving " << argc - 1 << " arguments, 2 expected!" << std::endl;
        return 1;
    }
    if (std::string(argv[1]) != "--resolver")
    {
        std::cerr << "You are supposed to give flag \"--resolver\", you give \"" << argv[1] << "\"." << std::endl;
        return 1;
    }
    uint32_t resolver_ip;
    uint16_t resolver_port;
    std::string temp; // if wrong, it will be error, if not it is string representation of address
    if (!parse_ip_address(resolver_ip, resolver_port, std::string(argv[2]), temp))
    {
        std::cerr << temp << std::endl;
        return 1;
    }
    std::cout << "Your DNS Server/Forwarder is active and ready to receive packet." << std::endl;
    std::cout << "It will forward your query to the following DNS server:" << std::endl;
    std::cout << "IP = " << temp << std::endl;
    std::cout << "Port = " << resolver_port << std::endl;

    int udpSocket;

    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1)
    {
        std::cerr << "Socket creation failed: " << strerror(errno) << "..." << std::endl;
        return 1;
    }

    // Since the tester restarts your program quite often, setting REUSE_PORT
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "SO_REUSEPORT failed: " << strerror(errno) << std::endl;
        return 1;
    }

    sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(udpSocket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) != 0)
    {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return 1;
    }

    while (true)
    {
        sockaddr_in clientAddress;
        int bytesRead;
        char buffer[512];
        socklen_t clientAddrLen = sizeof(clientAddress);
        // Receive data
        bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddrLen);
        if (bytesRead == -1)
        {
            perror("Error receiving data");
            break;
        }

        buffer[bytesRead] = '\0';
        std::cout << "Received " << bytesRead << " bytes." << std::endl;
        // Read the receive into a file for debugging
        std::ofstream file("./src/clientQuery.txt", std::ios::out | std::ios::trunc);
        file.write(buffer, bytesRead);
        file.close();
        std::cout << "Just received a dns query, stored in src/clientQuery.txt." << std::endl;

        // Parse the buffer into query
        DNSMessage query;
        size_t bufOffset = 0;
        parseDNSMessage(query, buffer, bufOffset);

        // Create an empty message and craft the response with the query
        DNSMessage response;
        // Handle Header
        response.header.transactionId = query.header.transactionId;
        if (ntohs(query.header.flags) & (1 << 15)) // if flag bit is reply, then wrong
        {
            std::cerr << "Expected a query, received reply." << strerror(errno) << std::endl;
            return 1;
        }
        response.header.flags = query.header.flags | htons(1 << 15);
        response.header.qdCount = query.header.qdCount;
        // 0 should be same for both network and host byte.
        response.header.anCount = response.header.qdCount;
        response.header.nsCount = 0;
        response.header.arCount = 0;

        // Handle question
        size_t numQ = ntohs(response.header.qdCount);
        for (int i = 0; i < numQ; i++)
        {
            DNSQuestion q;
            q.qName = query.questions[i].qName;
            if (ntohs(query.questions[i].qType) != 1 || ntohs(query.questions[i].qClass) != 1)
            {
                std::cerr << "Expected a 'A' record type and 'IN' record class for questions only. Wrong index =" << i << std::endl;
                return 1;
            }
            q.qType = query.questions[i].qType;
            q.qClass = query.questions[i].qClass;
            response.questions.push_back(q);
        }

        // Handle answer
        size_t numA = ntohs(response.header.qdCount);
        for (int i = 0; i < numA; i++)
        {
            DNSAnswer a;
            a.name = query.questions[i].qName;
            if (ntohs(query.questions[i].qType) != 1 || ntohs(query.questions[i].qClass) != 1)
            {
                std::cerr << "Expected a 'A' record type and 'IN' record class for answers only. Wrong index =" << i << std::endl;
                return 1;
            }
            a.type = query.questions[i].qType;
            a._class = query.questions[i].qClass;
            a.ttl = htonl(60);
            a.rdLength = htons(4);
            a.rData = "\x08"
                      "\x08"
                      "\x08"
                      "\x08";
            response.answers.push_back(a);
        }

        // Serialize everything into a buffer:
        char sendBuf[512];
        size_t offset = 0;
        serializeDNSMessage(sendBuf, response, offset);
        // Send response

        if (sendto(udpSocket, sendBuf, offset, 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1)
        {
            perror("Failed to send response");
        }
        std::cout << "Send back DNS reply to answer the query" << std::endl;
    }

    close(udpSocket);

    return 0;
}
bool parse_ip_address(uint32_t &dst_ip, uint16_t &dst_port, std::string src, std::string &error_mes)
{
    // Split into both path with the first ":"
    int pos = src.find(":");
    if (pos == std::string::npos)
    {
        error_mes = "The arguement should be in format <ip>:<port>, e.g: 0.0.0.0:80. You give " + src + ".";
        return false;
    }
    std::string ip = src.substr(0, pos);
    if (!inet_pton(AF_INET, ip.data(), &dst_ip))
    {
        error_mes = "Not a correct IPv4 address format! You give " + ip + ", expected a.b.c.d, e.g: 1.2.3.4!";
        return false;
    }
    std::string port = src.substr(pos + 1);
    try
    {
        size_t idx = 0;
        dst_port = std::stoi(port, &idx);

        // Check if the entire string was consumed
        if (idx != port.size())
        {
            throw std::invalid_argument("");
        }
    }
    catch (...)
    {
        error_mes = "Can not convert into integer from \"" + port + "\"!";
        return false;
    }
    error_mes = ip;
    return true;
}

/*
Test command
cat input.txt | nc -u 127.0.0.1 2053 > output.txt

hexdump -C output.txt
*/
