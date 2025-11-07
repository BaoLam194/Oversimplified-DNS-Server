/////////////////////////////////////////////
//////////       client side        /////////
/////////////////////////////////////////////
#include <iostream>
#include <cstring>
#include "netstruct.hpp"
int main(int argc, char **argv) // Take in a ipv4 address argument and send to the server a dns query
{
    if (argc != 2)
    {
        std::cout << "Your are giving " << argc - 1 << " arguments, 1 expected" << std::endl;
        exit(0);
    }
    // Convert the argument into network ip address
    uint32_t ser_ip;
    if (!inet_pton(AF_INET, argv[1], &ser_ip))
    {
        std::cerr << "Not a correct IPv4 address!: " << strerror(errno) << std::endl;
        return 1;
    }
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Disable output buffering
    setbuf(stdout, NULL);
    int udpSocket;

    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1)
    {
        std::cerr << "Socket creation failed: " << strerror(errno) << "..." << std::endl;
        return 1;
    }
    struct sockaddr_in serverAddress = {
        .sin_family = AF_INET,
        .sin_port = htons(2053),
        .sin_addr.s_addr = (in_addr_t)ser_ip,
    };
}