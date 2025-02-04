/******************************************************************************
* cclient.c
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
#include <stdbool.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "pollLib.h"

#define SEND_MAXBUF 200
#define RECV_MAXBUF 1500
#define MAX_INPUT_SIZE 1400
#define DEBUG_FLAG 1
#define MAX_HANDLES 9
#define MAX_HANDLE_LENGTH 100
#define MAX_MESSAGE_SIZE 199

// ----- Lab Functions -----
void clientControl(char *handle, int socketNum); 
void processStdin(char *handle, int socketNum); 
void processMsgFromServer(int socketNum); 
int readFromStdin(uint8_t *buffer);
void sendToServer(int socketNum);
void checkArgs(int argc, char *argv[]);

char handleNames[MAX_HANDLES][MAX_HANDLE_LENGTH];
bool shouldDisplayPrompt = true; 
// static bool waitForServerResponse = false;
// static bool displayPrompt = true;


// ----- Chat Functions -----
void initialPacket(int socketNum, char * handle);
void sendMessage(char *handle, int socketNum, char* destination, char *message); 
void broadcast(char* handle, int socketNum, char *message); 
void sendMulticast(char *handle, int socketNum, int numHandles, char * message); 
void ccList(char *handle, int socketNum); 

// ----- helper Functions -----
bool parseM(char *data, char *destinationHandle, char *message); 
int parseC(char *data, char *message);  

void processCommand(char *handle, int socketNum, char cmdChar, char *text);
void processRecvMessage(uint8_t *pdu, int pduLen, int offset); 
void processHandleError(uint8_t *pdu, int pduLen, int offset) ;
void processCount(uint8_t *pdu, int pduLen, int offset);
void processHandle(uint8_t *pdu, int pduLen, int offset); 
void processHandleReject(uint8_t *pdu, int pduLen, int offset, int socketNum);
void processMultiCast(uint8_t *pdu, int pduLen, int offset);
void processBroadcast(uint8_t *pdu, int pduLen, int offset); 


int main(int argc, char * argv[])
{
	int socketNum = 0;
	checkArgs(argc, argv);
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	clientControl(argv[1], socketNum);	
	close(socketNum);
	return 0;
}


void clientControl(char * handle, int socketNum){
	initialPacket(socketNum, handle);

	setupPollSet();
	addToPollSet(STDIN_FILENO);	// Monitor user input
	addToPollSet(socketNum); 	// Monsitor server messages

	while(1){
        printf("clientControl\n"); 
        if (shouldDisplayPrompt) {
            printf("$: "); // Display the prompt only if not awaiting response
            fflush(stdout); // Flush the output buffer
            shouldDisplayPrompt = false; 
        }		
		
		int socketNumber = pollCall(-1); 
		printf("pollCall returned socketNumber: %d\n", socketNumber);

		if(socketNumber < 0){
			printf("pollCall failed\n"); 
			exit(-1); 
		}
// ----- Read STDIN -----
        if (socketNumber == STDIN_FILENO) {
            printf("Processing stdin...\n");
            processStdin(handle, socketNum); // Pass the server socket to send user input
        }
// ----- Message From Server -----
        else if (socketNumber == socketNum) {
            printf("Processing server message...\n");
            processMsgFromServer(socketNum);
        }
	}
}


void initialPacket(int socketNum, char * handle){
    uint8_t handleLength = strlen(handle);
    uint8_t pdu[SEND_MAXBUF];
    int pduLen = 0;
    pdu[pduLen++] = FLAG_CLIENT_TO_SEVER_INITIAL; // Use the correct flag for initial connection
// ------ Insert: Handle Length,  Handle -----
    pdu[pduLen++] = handleLength;
    memcpy(pdu + pduLen, handle, handleLength);
    pduLen += handleLength; 
// ----- Send the PDU -----
    if (sendPDU(socketNum, pdu, pduLen) < 0) {
        perror("Failed to send initial connection packet");
        exit(-1);
    }
}


void processStdin(char * handle, int socketNum){
    uint8_t sendBuf[RECV_MAXBUF];   // Adjust this if needed to MAX_INPUT_SIZE
    int sendLen = 0;
    sendLen = readFromStdin(sendBuf);

    if ((sendBuf[1] == 'l' || sendBuf[1] == 'L') && sendBuf[0] == '%') {
    ccList(handle, socketNum);
    }
    else if (sendLen > 1) { // Check for more than just a newline
        sendBuf[sendLen - 1] = '\0'; // Remove the trailing newline added by readFromStdin

        if (sendBuf[0] == '%' && sendBuf[2] == ' ') {
            char cmdChar = sendBuf[1];
            char *message = (char *)sendBuf + 3; // Skipping "%X " to start text after space
            processCommand(handle, socketNum, cmdChar, message);
        } else {
            printf("No command detected.\n");
        }
    } else {
        printf("No command detected.\n");
    }
}

// ----- Parse Functions -----

int readFromStdin(uint8_t * buffer)
{
    char aChar = 0;
    int inputLen = 0;        

    // Important you don't input more characters than you have space 
    buffer[0] = '\0';
    while (inputLen < (MAX_INPUT_SIZE - 1) && (aChar = getchar()) != '\n' && aChar != EOF) {
        if (aChar != '\n') {
            buffer[inputLen++] = aChar;
        }
    }
    
    // Null terminate the string
    buffer[inputLen] = '\0';
    return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle server-host-name server-port-number \n", argv[0]);
		exit(1);
	}

	if(strlen(argv[1]) > 100){
		printf("Invalid handle, handle longer than 100 characters: <%s>\n", argv[1]);
		exit(1);
	}

}

int parseC(char *data, char *message){
    int numHandles = 0;
    const char* token = strtok(data, " "); // First token, assumed to be the number of handles

    if (token != NULL) {
        numHandles = atoi(token); // Convert to integer
        if (numHandles > MAX_HANDLES) {
            printf("Error: Number of handles exceeds the maximum allowed.\n");
            return 0;
        }
        if(numHandles < 2){
            printf("Error: More than one handle is allowed.\n"); 
            return 0; 
        }

        // Extract handle names
        for (int i = 0; i < numHandles; ++i) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                strncpy(handleNames[i], token, MAX_HANDLE_LENGTH - 1);
                handleNames[i][MAX_HANDLE_LENGTH - 1] = '\0'; // Ensure null termination
            } else {
                printf("Error: Not enough handle names provided.\n");
                return 0;
            }
        }

        // Extract the remaining part as the message
        token = strtok(NULL, ""); // Get the rest of the input as the message
        if (token != NULL) {
            strncpy(message, token, SEND_MAXBUF - 1);
            message[SEND_MAXBUF - 1] = '\0';
        } else {
            printf("Error: Message is missing.\n");
            return 0;
        }
    }
    return numHandles; // Return the number of handles parsed
}

bool parseM(char *data, char *destinationHandle, char *message){
    // Calculate the destination handle length
    char *firstSpace = strchr(data, ' ');
    if (!firstSpace) {
        printf("Error: Message must contain a space to separate destination handle from message.\n");
        return false;
    }
    int destinationHandleLength = firstSpace - data;
    if (destinationHandleLength > 100) {  // Leave room for null terminator
        printf("Error: Destination handle is too long.\n");
        return false;
    }

    memcpy(destinationHandle, data, destinationHandleLength);
    destinationHandle[destinationHandleLength] = '\0';

    firstSpace++; // Move past the space
    int messageLength = strlen(firstSpace);
    if (messageLength >= SEND_MAXBUF - 1) {  // Leave room for null terminator
        printf("Error: Message is too long.\n");
        return false;
    }
    memcpy(message, firstSpace, messageLength);
    message[messageLength] = '\0';
    return true; 
}

void processCommand(char * handle, int socketNum, char cmdChar, char *data) {
    char message[SEND_MAXBUF];      
    char destinationHandle[100];
    int numHandles = 0; 
    switch (cmdChar) {
        case 'M':
        case 'm':
            if (parseM(data, destinationHandle, message)) {
                printf("Dest Handle: %s\t Message: %s\n", destinationHandle, message);
                sendMessage(handle, socketNum, destinationHandle, message);
            }
            break;
        case 'C':
        case 'c':
            numHandles = parseC(data, message); 
            if (numHandles > 0) {
                for (int i = 0; i < numHandles; ++i) {
                    printf("%s\n", handleNames[i]);
                }
                printf("Message: %s\n", message);
                sendMulticast(handle, socketNum, numHandles, message);
            }
            break;
        case 'B':
        case 'b':
            if(data[0] == ' ') {
                printf("Error: No message provided for broadcast.\n");
             } else {
                broadcast(handle, socketNum, data);
                shouldDisplayPrompt = true; 
            }
            break;
        default:
            printf("Invalid command: %c\n", cmdChar);
            break;
    }
}

// ----- Send Functions -----

void broadcast(char* handle, int socketNum, char *message){
    uint8_t messageLength = strlen(message); 
    uint8_t handleLength = strlen(handle);
    int offset = 0; 
    while(offset < messageLength){
        uint8_t pdu[SEND_MAXBUF];
        int pduLen = 0;
        pdu[pduLen++] = FLAG_BROADCAST;  

// ----- Sender: Handle Length, Handle Name -----
        pdu[pduLen++] = handleLength;
        memcpy(pdu + pduLen, handle, handleLength);
        pduLen += handleLength; 
// ----- Insert Message -----
        int currentLength = (messageLength - offset > MAX_MESSAGE_SIZE ) ? MAX_MESSAGE_SIZE : messageLength - offset; 
        memcpy(pdu + pduLen, message + offset, currentLength); 
        pduLen += currentLength; 
// ----- sendPDU -----
        if (sendPDU(socketNum, pdu, pduLen) < 0) {
            perror("Failed to send PDU");
            exit(-1);
        }
        offset += currentLength;

    }
}


void ccList(char *handle, int socketNum){
    uint8_t handleLength = strlen(handle);
    uint8_t pdu[SEND_MAXBUF];
    int pduLen = 0;
// ----- Insert Flag -----
    pdu[pduLen++] = FLAG_LIST; 
// ----- Sender: Handle Length, Handle Name -----
    pdu[pduLen++] = handleLength;
    memcpy(pdu + pduLen, handle, handleLength);
    pduLen += handleLength; 
// ----- sendPDU -----
    if (sendPDU(socketNum, pdu, pduLen) < 0) {
        perror("Failed to send PDU");
        exit(-1);
    }
}

void sendMulticast(char *handle, int socketNum, int numHandles, char * message){
    uint8_t handleLength = strlen(handle);
    uint8_t messageLength = strlen(message);
    uint8_t offset = 0; 
// ----- Fragmentation -----
    while(offset < messageLength){
        uint8_t pdu[SEND_MAXBUF]; 
        int pduLen = 0; 
// ----- Insert Flag ----- Sender: Handle Length, Handle Name -----
        pdu[pduLen++] = FLAG_MULTICAST;    
        pdu[pduLen++] = handleLength;
        memcpy(pdu + pduLen, handle, handleLength);
        pduLen += handleLength;   
// ----- Number of Handles -----
        pdu[pduLen++] = numHandles;      
        for(int i = 0; i < numHandles; i++){
            if(strlen(handleNames[i]) > MAX_HANDLE_LENGTH){
                printf("Error: Handle %s exceeds maximum length of %d characters.\n", handleNames[i], MAX_HANDLE_LENGTH);
                return; 

            }
            int destinationHandleLength = strlen(handleNames[i]);
            pdu[pduLen++] = destinationHandleLength; 
            memcpy(pdu + pduLen, handleNames[i], destinationHandleLength); 
            pduLen += destinationHandleLength; 
            printf("Handle %d: %s (Length: %d)\n", i + 1, handleNames[i], destinationHandleLength);
        }
// ----- Message -----
        int currentLength = (messageLength - offset > MAX_MESSAGE_SIZE ) ? MAX_MESSAGE_SIZE : messageLength - offset; 
        memcpy(pdu + pduLen, message + offset, currentLength); 
        pduLen += currentLength; 

// ----- sendPDU -----
        if (sendPDU(socketNum, pdu, pduLen) < 0) {
            perror("Failed to send PDU");
            exit(-1);
        }
        offset += currentLength; 
    }
}


void sendMessage(char *handle, int socketNum, char *destinationHandle, char *message) {
    printf("sendMessage\n");
    uint8_t handleLength = strlen(handle);
    uint8_t destinationHandleLength = strlen(destinationHandle); 
    uint8_t messageLength = strlen(message); 
    uint8_t offset = 0; 

// ----- Fragmentation -----
    while(offset < messageLength){
        uint8_t pdu[SEND_MAXBUF];
        int pduLen = 0;

// ----- Insert Flag -----
        pdu[pduLen++] = FLAG_MESSAGE;  

// ----- Sender: Handle Length, Handle Name -----
        pdu[pduLen++] = handleLength;
        memcpy(pdu + pduLen, handle, handleLength);
        pduLen += handleLength; 
        pdu[pduLen++] = 1;  // num bit

// ----- Destination:  Handle Length, Handle Name -----
        pdu[pduLen++] = destinationHandleLength; 
        memcpy(pdu + pduLen, destinationHandle, destinationHandleLength); 
        pduLen  += destinationHandleLength; 

// ----- Message -----
        int currentLength = (messageLength - offset > MAX_MESSAGE_SIZE ) ? MAX_MESSAGE_SIZE : messageLength - offset; 
        memcpy(pdu + pduLen, message + offset, currentLength); 
        pduLen += currentLength; 
        pdu[pduLen] = '\0';

// ----- sendPDU -----
        if (sendPDU(socketNum, pdu, pduLen) < 0) {
            perror("Failed to send PDU");
            exit(-1);
        }
        offset += currentLength; 
    }
}


void processMsgFromServer(int socketNum){
	uint8_t pdu[SEND_MAXBUF];
    uint8_t offset = 0; 
	int pduLen = 0;
	pduLen = recvPDU(socketNum, pdu, SEND_MAXBUF);
    uint8_t flag = pdu[offset++];
    if (pduLen == 0) {  // Server closed the connection
        printf("\n---Server Terminated---\n");
        close(socketNum);
        exit(0);
    } else if (pduLen < 0) {
        perror("recvPDU failed");
        close(socketNum);
        return; 
    }
    switch(flag){
        case FLAG_HANDLE_CONFIRM:
            printf("---Valid Username---\n"); 
            break; 
        case FLAG_HANDLE_REJECT:
            processHandleReject(pdu, pduLen, offset, socketNum);
            break; 
        case FLAG_MULTICAST:
            processMultiCast(pdu, pduLen, offset); 
            break; 
        case FLAG_MESSAGE:
            processRecvMessage(pdu, pduLen, offset); 
            break; 
        case FLAG_BROADCAST:
            processBroadcast(pdu, pduLen, offset); 
            break; 
        case FLAG_HANDLE_ERROR:
            processHandleError(pdu, pduLen, offset); 
            break; 
        case FLAG_LIST_COUNT:
            processCount(pdu, pduLen, offset); 
            shouldDisplayPrompt = false;
            break; 
        case FLAG_LIST_HANDLE:
            processHandle(pdu, pduLen, offset); 
            shouldDisplayPrompt = false;
            break; 
        case FLAG_LIST_END:
            break; 
        default:
            printf("I don't know you!\n"); 
            break; 
    }
}

void processBroadcast(uint8_t *pdu, int pduLen, int offset){
        // ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset++];
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    senderHandle[senderHandleLength] = '\0';
    offset += senderHandleLength; 
    printf("Broadcast from [%s] (Length: %u)\n", senderHandle, senderHandleLength);

    // ----- Message -----
    uint8_t message[100];    
    int messageLength = pduLen - offset;
    memcpy(message, pdu + offset, messageLength); 
    message[messageLength] = '\0'; 
    printf("Message received: %s\n", message);

    printf("%s: %s\n", senderHandle, message); // Print sender and message

}


void processHandleReject(uint8_t *pdu, int pduLen, int offset, int socketNum){
    uint8_t handleLen = pdu[offset++]; 
    uint8_t handle[100];
    memcpy(handle, pdu + offset, handleLen); 
    handle[handleLen] = '\0'; 
    printf("Handle already in use: %s\n", handle); 
    close(socketNum);
    exit(0);
}

void processCount(uint8_t *pdu, int pduLen, int offset){
    uint32_t len = 0; 
    len  = ntohl(*(uint32_t *)(pdu + offset));  // Convert from network byte order
    printf("Number of Clients: %d\n", len); 
}

void processHandle(uint8_t *pdu, int pduLen, int offset){
    uint8_t handleLen = pdu[offset++]; 
    uint8_t handle[100];
    memcpy(handle, pdu + offset, handleLen); 
    handle[handleLen] = '\0'; 
    printf("\t%s\n", handle); 
}

void processMultiCast(uint8_t *pdu, int pduLen, int offset){
    if (offset >= pduLen) {
        printf("Error: PDU length is too short to even read sender handle length.\n");
        return;
    }

// ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset++];
    uint8_t senderHandle[100]; 
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    printf("Offset: %d, pdu: %c\n", offset, pdu[offset]); 

    senderHandle[senderHandleLength] = '\0';
    offset += senderHandleLength;

// ----- Number of handles -----
    int numHandles = pdu[offset++];
    for (int i = 0; i < numHandles; i++) {
        uint8_t destinationHandleLength = pdu[offset++];
        offset += destinationHandleLength; // Move past the destination handle

    }
    
// ----- Message -----
    char message[1024]; // Adjust message buffer size as needed
    int messageLength = pduLen - offset;
    printf("NumHandles: %d, Offset: %d, pduLen: %d\n", numHandles, offset, pduLen); 
    memcpy(message, pdu + offset, messageLength);
    message[messageLength] = '\0'; // Ensure null termination
    printf("%s: %s\n", senderHandle, message); // Print sender and message
}

void processRecvMessage(uint8_t *pdu, int pduLen, int offset){
    printf("Message Recieved\n"); 
// ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset++]; 
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    senderHandle[senderHandleLength] = '\0'; 
    offset += senderHandleLength; 
// ----- M and C bit -----
    offset++; 
// ----- Destination:  Handle Length, Handle Name -----
    uint8_t destinationHandleLength = pdu[offset++];
    offset += destinationHandleLength; 
// ----- Message -----
    printf("Offset: %d\tpduLen: %d", offset, pduLen); 
    uint8_t message[RECV_MAXBUF]; 
    memcpy(message, pdu + offset, pduLen - offset); 
    message[pduLen - offset] = '\0'; // Always NUll
    printf("\n%s: %s\n", senderHandle, message); 
}


void processHandleError(uint8_t *pdu, int pduLen, int offset){
    int handleLength = pdu[offset++];
    uint8_t handle[100];
    memcpy(handle, pdu + offset, handleLength); 
    handle[handleLength] = '\0'; 
    printf("Client with handle <%s> does not exist\n", handle); 
}
