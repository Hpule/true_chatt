/*

*/

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "safeUtil.h"

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData){
    // length of Data + 2
    int pduLen = lengthOfData + 2; 

    // length -> network order Bufferdata
    uint16_t length_network_order = htons(lengthOfData);

    // data buffer memcpy
    uint8_t * pdu = (uint8_t * )malloc(pduLen);    

    if(pdu == NULL){
        perror("Failed to allocate memory for PDU"); 
        return -1; 
    }
    memcpy(pdu, &length_network_order, sizeof(length_network_order)); 
    memcpy(pdu + 2 , dataBuffer,  lengthOfData);  

    // Safe Send 
    int bytesSent  = safeSend(clientSocket, pdu, pduLen, 0);

    // printf("\npduLen: %d\tpdu: %hhn\tlengthNetworkOrder: %d\tbytesSend: %d\n", pduLen, pdu, length_network_order, bytesSent); 

    if(bytesSent != pduLen ){
        perror("Safe Safe Fail");
        free(pdu); 
        return -1;  
    } 

    free(pdu); 
    // printf("\nsendPDU\nclientSocket: %d\tdataBuffer: %s\tlengthOfData: %d\n", 
        // clientSocket, dataBuffer, lengthOfData); 

    return bytesSent;  
}

int  recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize){
    // ----- First Call receive 2 bytes ----- //
    uint16_t lengthField; 
    int bytesReceived = safeRecv(socketNumber, (uint8_t *)&lengthField, sizeof(lengthField), MSG_WAITALL);
    if(bytesReceived == 0){
        return 0; // Closed by other side
    }

    if(bytesReceived != sizeof(lengthField)){
        fprintf(stderr, "Error: Incomplete length field received\n");
        return -1; // Error
    }

    // Convert length field from network byte order to host byte order
    int length_host_order = ntohs(lengthField);

    // Step 2: Validate the length
    if (length_host_order <= 0 || length_host_order > bufferSize) {
        fprintf(stderr, "Error: Invalid or oversized PDU length: %d\n", length_host_order);
        return -1; // Error
    }

    // ----- Second Call receive rest ----- //
    bytesReceived = safeRecv(socketNumber, dataBuffer, length_host_order, MSG_WAITALL);
    if(bytesReceived == 0){
        return 0; // Closed by other side
    }

    if(bytesReceived != length_host_order){
        fprintf(stderr, "Error: Incomplete PDU received: expected %d bytes, received %d bytes.\n",
                length_host_order, bytesReceived);
        return -1; // Error
    }

    // printf("\nrecvPDU\nclientSocket: %d\tdataBuffer: %s\tlengthOfData: %d\n",
        // socketNumber, dataBuffer, bytesReceived); 

    return bytesReceived ;
}