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
    // uint16_t totalLength = messageLen;
    uint8_t flag = pdu[0];
	int offset = 1;

    // ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset];
    offset++; 
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

        case FLAG_REQUEST_LISTEN:

            break;

        default:
            printf("Invalid flag: %d\n", flag);
            break;
    }    
}

void processMulticast(int clientSocket, uint8_t *pdu, int pduLen){
    // ----- Extract PDU details
	int offset = 1;

    // ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset];
    offset++; 
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    senderHandle[senderHandleLength] = '\0'; // Null-terminate for safety
    offset += senderHandleLength; 
    printf("Sender Handle: %s (Length: %u)\n", senderHandle, senderHandleLength);


    // ----- Number of handles -----
    int numHandles = pdu[offset]; 
    offset++;
    printf("Number of destination handles: %d\n", numHandles);


    // ----- Destination:  Handle Lengths, Handle Names -----
    int destinationHandleLength;
    for (int i = 0; i < numHandles; i++) {
        destinationHandleLength = pdu[offset];
        offset++;
        memcpy(handleNames[i], pdu + offset, destinationHandleLength);
        handleNames[i][destinationHandleLength] = '\0'; // Null-terminate each handle name
        offset += destinationHandleLength;

        printf("Handle %d: %s (Length: %d)\n", i + 1, handleNames[i], destinationHandleLength);
    }

    // ------ Message -----
    uint8_t messageLength = pdu[offset];
    offset++; 
    uint8_t message[100];    
    memcpy(message, pdu + offset, messageLength); 
    message[messageLength] = '\0'; 
    printf("Message: %s (Length: %d)\n", message, messageLength);
}



void processMessage(int clientSocket, uint8_t *pdu, int pduLen){
    // ----- Extract PDU details
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