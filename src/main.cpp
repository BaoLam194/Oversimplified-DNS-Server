/////////////////////////////////////////////
//////////       server side        /////////
/////////////////////////////////////////////
#include <iostream>
#include <cstring>
#include "netstruct.hpp"
#include <fstream>

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
        std::cout << "Received " << bytesRead << " bytes: " << buffer << std::endl;
        // Read the receive into a file for debugging

        std::ofstream file("./src/clientQuery.txt", std::ios::out | std::ios::trunc);
        file.write(buffer, bytesRead);
        file.close();
        std::cout << "Just received a dns query, stored in src/clientQuery.txt" << std::endl;

        // Create an empty message and parse the query into response
        DNSMessage response;
        size_t bufOffset = 0;
        parseDNSMessage(response, buffer, bufOffset);

        // Handle question
        response.questions = new DNSQuestion[ntohs(response.header.qdCount)]; // currently have one question
        response.questions->qName = "\x0c"
                                    "codecrafters"
                                    "\x02"
                                    "io";
        response.questions->qName.push_back('\0'); // add terminale byte for name.
        response.questions->qType = htons(1);
        response.questions->qClass = htons(1);

        // Handle answer
        response.answers = new DNSAnswer[ntohs(response.header.anCount)];
        response.answers->name = response.questions->qName;
        response.answers->type = htons(1);
        response.answers->_class = htons(1);
        response.answers->ttl = htons(60);
        response.answers->rdLength = htons(4);
        response.answers->rData = "\x08"
                                  "\x08"
                                  "\x08"
                                  "\x08";

        // Serialize everything into a buffer:
        char sendBuf[512];
        size_t offset = 0;
        serializeDNSMessage(sendBuf, response, offset);
        // Send response

        if (sendto(udpSocket, sendBuf, offset, 0, reinterpret_cast<struct sockaddr *>(&clientAddress), sizeof(clientAddress)) == -1)
        {
            perror("Failed to send response");
        }
        // Free data
        delete[] response.questions;
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
