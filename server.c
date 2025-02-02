/******************************************************************************
* server.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "pollLib.h"
#include "handleTable.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define MAX_HANDLES 9
#define MAX_HANDLE_LENGTH 100

void serverControl(int mainServerSocket); 
void addNewSocket(int socketNumber); 
void processClient(int clientSocket); 
int checkArgs(int argc, char *argv[]);

// ----- Helper Functions ------
int isHandleTaken(HandleNode *head, const uint8_t *handle);
void initialPacket(int clientSocket, uint8_t *pdu, int pduLen); 
void processMessage(int clientSocket, uint8_t *pdu, int pduLen); 
void processMulticast(int clientSocket, uint8_t *pdu, int pduLen); 
void processList(int clientSocket, uint8_t *pdu, int pduLen); 

void multiSend(); 

char handleNames[MAX_HANDLES][MAX_HANDLE_LENGTH];
HandleNode *handleHead = NULL; 


int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
    handleHead = createHandleTable(); 
	mainServerSocket = tcpServerSetup(portNumber);   

    serverControl(mainServerSocket);
    destroyHandleTable(handleHead);
	close(mainServerSocket);

	return 0;
}

void serverControl(int mainServerSocket){
    setupPollSet();
    addToPollSet(mainServerSocket);

    while(1){
        int socketNumber = pollCall(-1); 
        printf("pollCall returned socketNumber: %d\n", socketNumber);

        if(socketNumber < 0){
            printf("pollCall failed\n"); 
            exit(-1); 
        }

        (socketNumber == mainServerSocket) ?  addNewSocket(mainServerSocket) : processClient(socketNumber);
    }
}


void addNewSocket(int socketNumber){
    // Processes a new connection (e.g. accept(), add to pollset())
    int newSocket = tcpAccept(socketNumber, DEBUG_FLAG); 
    addToPollSet(newSocket);
    printf("New client connected: socket  %d\n", newSocket); 
}

void processClient(int clientSocket){
    // Calls recvPDU(), and outputs message
    printf("\nProcessing client on socket: %d\n", clientSocket);
    uint8_t pdu[MAXBUF];
    int pduLen = recvPDU(clientSocket, pdu, MAXBUF);

    if (pduLen < 0) {
        perror("recvPDU call");
        close(clientSocket);
        return;
    } else if (pduLen == 0) {
        printf("Client disconnected: socket %d\n", clientSocket);
        const char *handle = findHandleBySocket(handleHead, clientSocket);
        if(handle != NULL){
            printf("Removing handle: %s\n", handle);
            removeHandle(&handleHead, handle); 
        } 
        removeFromPollSet(clientSocket);
        close(clientSocket);
        return;
    }

// ----- Extract PDU details
    uint8_t flag = pdu[0];
	int offset = 1;

// ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset++];
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    
    switch (flag) {
		case FLAG_CLIENT_TO_SEVER_INITIAL:
            initialPacket(clientSocket, pdu, pduLen); 
            break;

        case FLAG_MESSAGE:
            processMessage(clientSocket, pdu, pduLen); 
            break;

        case FLAG_MULTICAST:
            processMulticast(clientSocket, pdu, pduLen); 
            break; 

        case FLAG_BROADCAST:
            break; 

        case FLAG_LIST:
            processList(clientSocket, pdu, pduLen); 

            break;

        default:
            printf("Invalid flag: %d\n", flag);
            break;
    }    
}

void processList(int clientSocket, uint8_t *pdu, int pduLen){
    int handleCount = getNumHandles(handleHead);
    printf("Starting to process the list of handles. Total handles: %d\n", handleCount);

    // Send the total number of handles
    uint32_t networkHandleCount = htonl(handleCount);  // Convert to network byte order
    uint8_t initialPdu[1024];
    int len = 0;
    initialPdu[len++] = FLAG_LIST_COUNT;  // Flag = 11
    memcpy(initialPdu + len, &networkHandleCount, sizeof(networkHandleCount));
    len += sizeof(networkHandleCount);
    sendPDU(clientSocket, initialPdu, len);
    printf("Sent count of handles to client: %u\n", handleCount);

    // Send each handle name
    uint8_t handlePdu[MAXBUF];
    for (int i = 0; i < handleCount; i++) {
        const char *handle = getHandleByIndex(handleHead, i);
        if (handle) {
            len = 0;
            handlePdu[len++] = FLAG_LIST_HANDLE;  // Flag = 12
            uint8_t handleLen = strlen(handle);
            handlePdu[len++] = handleLen;
            memcpy(handlePdu + len, handle, handleLen);
            len += handleLen;
            sendPDU(clientSocket, handlePdu, len);
            printf("Sent handle [%d]: %s\n", i + 1, handle);
        }
    }

    // Send end of list flag
    uint8_t lastPdu[MAXBUF];
    len = 0;
    lastPdu[len++] = FLAG_LIST_END;  // Flag = 13
    sendPDU(clientSocket, lastPdu, len);  // No additional data needed for this message
    printf("Sent end of handle list signal to client.\n");
}


void processMulticast(int clientSocket, uint8_t *pdu, int pduLen){
	int offset = 1;

// ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset++]; 
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);

    senderHandle[senderHandleLength] = '\0';                
    offset += senderHandleLength; 
    printf("Sender Handle: %s (Length: %u)\n", senderHandle, senderHandleLength);

// ----- Number of handles -----
    int numHandles = pdu[offset++];                           
    printf("Number of destination handles: %d\n", numHandles);

// ----- Destination:  Handle Lengths, Handle Names -----

    int destinationHandleLength;   
    char handleNames[MAX_HANDLES][100];  // Adjust MAX_HANDLES as needed
    int validHandlesCount = 0;

// ----- Validate each destination handle
    for (int i = 0; i < numHandles; i++) {
        uint8_t destinationHandleLength = pdu[offset++];
        char destinationHandle[100];
        memcpy(destinationHandle, pdu + offset, destinationHandleLength);
        destinationHandle[destinationHandleLength] = '\0';
        offset += destinationHandleLength;

        if (findHandle(handleHead, destinationHandle)) {
            // Store valid handle names if necessary
            strcpy(handleNames[validHandlesCount++], destinationHandle);
            printf("Valid handle added: %s\n", destinationHandle);

        } else {
            // Prepare and send an error PDU for the first invalid handle found
            uint8_t errorPDU[MAXBUF];
            int errorPDULen = 0;
            errorPDU[errorPDULen++] = FLAG_HANDLE_ERROR;
            errorPDU[errorPDULen++] = destinationHandleLength;
            memcpy(errorPDU + errorPDULen, destinationHandle, destinationHandleLength);
            errorPDULen += destinationHandleLength;
            sendPDU(clientSocket, errorPDU, errorPDULen);
            printf("Invalid handle found, error PDU sent for: %s\n", destinationHandle);
             // Stop processing further
        }
    }

// ----- Message -----
    uint8_t message[100];    
    memcpy(message, pdu + offset, pduLen - offset); 
    message[pduLen - offset] = '\0'; 
// ----- Send  message to  valid handles -----
    for (int i = 0; i < validHandlesCount; i++) {
        uint8_t handlePdu[MAXBUF];
        int handlePduLen = 0;
        handlePdu[handlePduLen++] = FLAG_MESSAGE;

        // Include the sender handle
        handlePdu[handlePduLen++] = senderHandleLength;
        memcpy(handlePdu + handlePduLen, senderHandle, senderHandleLength);
        handlePduLen += senderHandleLength;

        handlePdu[handlePduLen++] = 1; 

        // Include the destination handle
        destinationHandleLength = strlen(handleNames[i]);
        handlePdu[handlePduLen++] = destinationHandleLength;
        memcpy(handlePdu + handlePduLen, handleNames[i], destinationHandleLength);
        handlePduLen += destinationHandleLength;

        // Include the message
        memcpy(handlePdu + handlePduLen, message, pduLen - offset);
        handlePduLen += pduLen - offset;

        // Send the constructed PDU
        int destSocket = findSocketByHandle(handleHead, handleNames[i]);
        printf("Sending multicast message to: %s (Socket: %d)\n", handleNames[i], destSocket);

        sendPDU(destSocket, handlePdu, handlePduLen);
    }
}



void processMessage(int clientSocket, uint8_t *pdu, int pduLen){
	int offset = 1;

    // ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset];
    offset++; 
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);

    // ----- Sender: Handle Length, Handle name -----
    offset += senderHandleLength; 

    // ----- M and C bit -----
    // uint8_t mcBit = pdu[offset]; 
    offset++; 

    // ----- Destination:  Handle Length, Handle Name -----
    uint8_t destinationHandleLength = pdu[offset];
    offset++; 
    uint8_t destinationHandle[100];
    memcpy(destinationHandle, pdu + offset, destinationHandleLength);
    destinationHandle[destinationHandleLength] = '\0'; // Always NULL
    offset += destinationHandleLength; 

    // ----- Check Destination Handle -----
    const char *handle = findHandle(handleHead, (char *)destinationHandle);
    printf("handle: %s, Dest handle: %s\n", handle, destinationHandle ); 

    if(handle == NULL){
        uint8_t noHandle[MAXBUF];
        int noH_Len = 0;
        noHandle[0] = FLAG_HANDLE_ERROR; 
        noH_Len++; 

        noHandle[noH_Len] = destinationHandleLength; 
        noH_Len++; 
        memcpy(noHandle + noH_Len, destinationHandle, destinationHandleLength); 
        noH_Len += destinationHandleLength; 
        sendPDU(clientSocket, noHandle, noH_Len); 
        // *destinationHandle = 0; 
        return;      
    } else{
        int socket = findSocketByHandle(handleHead, (char *)destinationHandle);
        printf("Destination Found: %s, Socket: %d\n",destinationHandle, socket); 
        sendPDU(socket, pdu, pduLen);                
    }
}


void initialPacket(int clientSocket, uint8_t *pdu, int pduLen){
    int offset = 1;
    uint8_t senderHandleLength = pdu[offset];
    offset++; 
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    senderHandle[senderHandleLength] = '\0'; // Always NULL
    
    if (isHandleTaken(handleHead, senderHandle)) {
        printf("Handle '%s' is already taken\n", senderHandle);
        // Create and send error PDU back to client
        uint8_t rejectBuffer[MAXBUF];
        rejectBuffer[0] = FLAG_HANDLE_REJECT;  // Using same flag for consistency
        rejectBuffer[1] = 0;  // Length of 0 can indicate error
        sendPDU(clientSocket, rejectBuffer, 2);
        
        removeFromPollSet(clientSocket);
        close(clientSocket);
        return;
    }
    
    // If handle is not taken, add it to the table
    addHandle(&handleHead, (char *)senderHandle, clientSocket);
    uint8_t confirmBuffer[MAXBUF];
    confirmBuffer[0] = FLAG_HANDLE_CONFIRM;  // Using same flag for consistency
    confirmBuffer[1] = 0;  // Length of 0 can indicate error
    sendPDU(clientSocket, confirmBuffer, 2);

    printf("Initial packet -- socket %d, handle: %s\n", clientSocket, senderHandle);
}


// Implement the handle check function
int isHandleTaken(HandleNode *head, const uint8_t *handle) {
    HandleNode *current = head;
    while (current != NULL) {
        if (strcmp((char *)handle, current->handle) == 0) {
            return 1;  // Handle is taken
        }
        current = current->next;
    }
    return 0;  // Handle is not taken
}


int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}