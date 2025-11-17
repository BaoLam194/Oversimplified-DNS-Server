/////////////////////////////////////////////
//////////       server side        /////////
/////////////////////////////////////////////
#include <iostream>
#include <cstring>
#include "netstruct.hpp"
#include <fstream>
#include <vector>

int main()
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Disable output buffering
    setbuf(stdout, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cout << "Logs from your program will appear here!" << std::endl;

    int udpSocket;
    sockaddr_in clientAddress;

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

    int bytesRead;
    char buffer[512];
    socklen_t clientAddrLen = sizeof(clientAddress);

    while (true)
    {
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
            std::cerr << "Expected a query, reply received. " << strerror(errno) << std::endl;
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
                std::cerr << "Expected a 'A' record type and 'IN' record class only." << std::endl;
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
                std::cerr << "Expected a 'A' record type and 'IN' record class onyl." << strerror(errno) << std::endl;
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
/*
Test command
cat input.txt | nc -u 127.0.0.1 2053 > output.txt

hexdump -C output.txt
*/
