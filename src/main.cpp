/////////////////////////////////////////////
//////////       server side        /////////
/////////////////////////////////////////////
// Acts as an intermediate server to forward query to higher level server.
#include <iostream>
#include <cstring>
#include "netstruct.hpp"
#include <fstream>
#include <vector>

// Global variable
sockaddr_in resolver; // The ultimate higher level resolver, set differently each time run for flexibility and test case?

bool parse_ip_address(uint32_t &dst_ip, uint16_t &dst_port, std::string src, std::string &error_mes);
void str_cli(int sockfd);

int main(int argc, char **argv)
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Disable output buffering
    setbuf(stdout, NULL);

    std::cout << "Starting the server..." << std::endl;
    // Initialize the upstream resolver
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
    resolver = {
        .sin_family = AF_INET,
        .sin_port = htons(resolver_port),
        .sin_addr = {resolver_ip},
    };

    // Create socket
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
        str_cli(udpSocket);
    }

    close(udpSocket);

    return 0;
}
void str_cli(int sockfd)
{
    std::cout << "Waiting to receive RFC1035 format DNS query." << std::endl;
    // Receive data
    sockaddr_in clientAddress;
    socklen_t clientAddrLen = sizeof(clientAddress);
    int bytesRead;
    char buffer[512];
    bytesRead = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                         reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddrLen);
    if (bytesRead == -1)
    {
        std::cerr << "Error receiving data" << std::endl;
        return;
    }
    buffer[bytesRead] = '\0';
    // Read the receive into a file for debugging
    std::cout << "Received a " << bytesRead << "-byte query, stored in src/clientQuery.txt" << std::endl;
    std::ofstream file("./src/clientQuery.txt", std::ios::out | std::ios::trunc);
    file.write(buffer, bytesRead);
    file.close();

    // Parse the buffer into query
    DNSMessage query;
    size_t bufOffset = 0;
    parseDNSMessage(query, buffer, bufOffset);
    if (ntohs(query.header.flags) & (1 << 15)) // if flag bit is reply, then wrong
    {
        std::cerr << "Expected a query, received reply." << strerror(errno) << std::endl;
        return;
    }
    std::cout << "Querying to upstream resolver..." << std::endl;

    // Split query if mutiple questions to forward
    DNSMessage splitForwardQuery, response;
    int count = ntohs(query.header.qdCount);
    splitForwardQuery.header = query.header; // Header always the same for each seperate question

    splitForwardQuery.header.flags |= htons(1 << 8);
    // This line is depend on query, but on test case with 1.1.1.1, it requires you to activate this bit

    splitForwardQuery.header.qdCount = htons(1); // IMPORTANT!!!

    // Construct response
    response.header = query.header;
    response.header.anCount = response.header.qdCount;
    response.header.flags = query.header.flags | htons(1 << 15); // Set it as response
    for (int i = 0; i < count; i++)
    {
        // Construct query for resolver and send
        while (splitForwardQuery.questions.size())
        {
            splitForwardQuery.questions.pop_back();
        }
        splitForwardQuery.questions.push_back(query.questions[i]);
        response.questions.push_back(query.questions[i]);

        char sendTempBuf[512];
        size_t offsetTemp = 0;
        serializeDNSMessage(sendTempBuf, splitForwardQuery, offsetTemp);
        if (sendto(sockfd, sendTempBuf, offsetTemp, 0, reinterpret_cast<sockaddr *>(&resolver), sizeof(resolver)) == -1)
        {
            std::cerr << "Send data fails. Please try again!" << std::endl;
        }
        // Receive and add into real query
        char recvTempBuf[512];
        size_t offsetRcvTemp = 0;
        sockaddr_in recvAddr;
        socklen_t recvAddrLen = sizeof(sockaddr_in);
        int bytesReceived = recvfrom(sockfd, recvTempBuf, sizeof(recvTempBuf), 0, reinterpret_cast<sockaddr *>(&recvAddr), &recvAddrLen);
        if (bytesReceived == -1)
        {
            std::cerr << "Error receiving dns response! " << std::endl;
            return;
        }
        DNSMessage temp;
        parseDNSMessage(temp, recvTempBuf, offsetRcvTemp);
        response.answers.push_back(temp.answers[0]);
    }

    char sendBuf[512];
    size_t sendOffset = 0;
    serializeDNSMessage(sendBuf, response, sendOffset);
    if (sendto(sockfd, sendBuf, sendOffset, 0, reinterpret_cast<sockaddr *>(&clientAddress), clientAddrLen) == -1)
    {
        std::cerr << "Send data fails. Please try again!" << std::endl;
    }
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