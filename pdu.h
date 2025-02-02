/*  
*/

#ifndef __pdu__
#define __pdu__

#include <stdint.h>


// ----- Flags -----
typedef enum uint8_t {
    FLAG_CLIENT_TO_SEVER_INITIAL = 1, 
    FLAG_HANDLE_CONFIRM = 2,
    FLAG_HANDLE_REJECT = 3,
    FLAG_BROADCAST = 4,
    FLAG_MESSAGE = 5,
    FLAG_MULTICAST = 6,
    FLAG_HANDLE_ERROR = 7,
    FLAG_RESERVED = 8,
    FLAG_RESERVED2 = 9,
    FLAG_LIST = 10,
    FLAG_LIST_COUNT = 11,
    FLAG_LIST_HANDLE = 12,
    FLAG_LIST_END = 13,
} flagType;

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData); 
int recvPDU(int socketNumber, uint8_t * dataBuffer, int lengthOfData); 


#endif