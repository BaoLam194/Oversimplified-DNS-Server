#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

struct DNSResponse // Default should be network order byte
{
    // Header
    uint16_t transactionId;
    uint16_t flags;
    uint16_t quesCount;
    uint16_t ansCount;
    uint16_t authCount;
    uint16_t addiCount;
};