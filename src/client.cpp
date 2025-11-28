/////////////////////////////////////////////
//////////       client side        /////////
/////////////////////////////////////////////
#include <iostream>
#include <cstring>
#include "netstruct.hpp"
#include <fstream>
#include <vector>
int main(int argc, char **argv) // Take in a ipv4 address argument and send to the server a dns query
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    // Disable output buffering
    setbuf(stdout, NULL);

    if (argc != 2)
    {
        std::cerr << "Your are giving " << argc - 1 << " arguments, 1 expected!" << std::endl;
        return 1;
    }
    // Convert the argument into network ip address
    uint32_t ser_ip;
    if (!inet_pton(AF_INET, argv[1], &ser_ip))
    {
        std::cerr << "Not a correct IPv4 address!" << std::endl;
        return 1;
    }
    int udpSocket;
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1)
    {
        std::cerr << "Socket creation failed. Please try again!" << std::endl;
        return 1;
    }
    sockaddr_in serverAddress = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr = {ser_ip},
    };
    // Construct a normal dns query
    DNSMessage req;
    req.header.transactionId = htons(5555);
    req.header.flags = 0;
    req.header.qdCount = htons(2);
    req.header.anCount = 0;
    req.header.nsCount = 0;
    req.header.arCount = 0;

    DNSQuestion q1;
    q1.qName = "codecrafters.io";
    q1.qType = htons(1);
    q1.qClass = htons(1);
    req.questions.push_back(q1);
    DNSQuestion q2;
    q2.qName = "baolam.top";
    q2.qType = htons(1);
    q2.qClass = htons(1);
    req.questions.push_back(q2);
    // Serialize everything into a buffer:
    char sendBuf[512];
    size_t offset = 0;

    serializeDNSMessage(sendBuf, req, offset);
    // Send to serverAddress a dns query
    if (!sendto(udpSocket, sendBuf, offset, 0, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)))
    {
        std::cerr << "Send data fails. Please try again!" << std::endl;
        return 1;
    }

    // Hear back the dns response from server
    char buffer[512];
    sockaddr_in recvAddr;
    socklen_t recvAddrLen = sizeof(sockaddr_in);
    int bytesReceived;
    bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&recvAddr), &recvAddrLen);
    if (bytesReceived == -1)
    {
        std::cerr << "Error receiving dns response! " << std::endl;
        return 1;
    }
    // Write back the dns response into a file
    std::ofstream file("./src/serverResponse.txt", std::ios::out | std::ios::trunc);
    file.write(buffer, bytesReceived);
    file.close();
    std::cout << "Received a dns response, stored in src/serverResponse.txt." << std::endl;
    close(udpSocket);
}