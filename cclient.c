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

#define MAXBUF 1024
#define DEBUG_FLAG 1
#define MAX_HANDLES 9
#define MAX_HANDLE_LENGTH 100

// ----- Lab Functions -----
void clientControl(char *handle, int socketNum); 
void processStdin(char *handle, int socketNum); 
void processMsgFromServer(int socketNum); 
int readFromStdin(uint8_t *buffer);
void sendToServer(int socketNum);
void checkArgs(int argc, char *argv[]);

char handleNames[MAX_HANDLES][MAX_HANDLE_LENGTH];
static bool waitForServerResponse = false;
static bool displayPrompt = true;


// ----- Chat Functions -----
void initialPacket(int socketNum, char * handle);
void sendMessage(char *handle, int socketNum, char* destination, char *message); 
void broadcast(char* handle, int socketNum, char *message); 
void sendMulticast(char *handle, int socketNum, int numHandles, char * message); 
void ccList(char *handle, int socketNum); 

// ----- helper Functions -----
void processCommand(char *handle, int socketNum, char cmdChar, char *text);
bool parseM(char *data, char *destinationHandle, char *message); 
int parseC(char *data, char *message);  
void processRecvMessage(uint8_t *pdu, int pduLen, int offset); 
void processHandleError(uint8_t *pdu, int pduLen, int offset) ;
void processCount(uint8_t *pdu, int pduLen, int offset);
void processHandle(uint8_t *pdu, int pduLen, int offset); 
void processHandleReject(uint8_t *pdu, int pduLen, int offset, int socketNum);

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
	addToPollSet(socketNum); 	// Monitor server messages

	while(1){
        if (waitForServerResponse) {
            printf("$: "); // Display the prompt only if not awaiting response
            fflush(stdout); // Flush the output buffer
        }		
		
		int socketNumber = pollCall(-1); 
		// printf("pollCall returned socketNumber: %d\n", socketNumber);

		if(socketNumber < 0){
			printf("pollCall failed\n"); 
			exit(-1); 
		}

    // ----- Read STDIN -----
        if (socketNumber == STDIN_FILENO) {
            // printf("Processing stdin...\n");
            processStdin(handle, socketNum); // Pass the server socket to send user input
        }
    // ----- Message From Server -----
        else if (socketNumber == socketNum) {
            // printf("Processing server message...\n");
            processMsgFromServer(socketNum);
        }
	}
}


void initialPacket(int socketNum, char * handle){
    uint8_t handleLength = strlen(handle);
    uint16_t totalLength = 2 + handleLength;  // 1 bytes flag + 1 byte handle length + handle
    uint8_t pdu[MAXBUF];
    int pduLen = 0;

// ----- Insert Flag -----
    pdu[pduLen] = FLAG_CLIENT_TO_SEVER_INITIAL; // Use the correct flag for initial connection
	pduLen++;

// ------ Insert: Handle Length,  Handle -----
    pdu[pduLen] = handleLength;
	pduLen++;
    memcpy(pdu + pduLen, handle, handleLength);

// ----- Send the PDU -----
    if (sendPDU(socketNum, pdu, totalLength) < 0) {
        perror("Failed to send initial connection packet");
        exit(-1);
    }
}


void processStdin(char * handle, int socketNum){
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	sendLen = readFromStdin(sendBuf);
	sendBuf[sendLen - 1] = '\0'; // Remove the trailing newline added by readFromStdin

	// ----- Check Input -----
    if(sendBuf[1] == 'l' || sendBuf[1] == 'L'){
        ccList(handle, socketNum);
    } else if (sendBuf[0] == '%' && sendBuf[2] == ' ') {
		// printf("Command detected.\n");
        char cmdChar = sendBuf[1]; // Assuming command is always one character
        char *message = (char *)sendBuf + 3; // Skipping "%X " to start text after space
        // Process command
        processCommand(handle, socketNum, cmdChar, message);
    } else {
        printf("No command detected.\n");
    }
}


void processCommand(char * handle, int socketNum, char cmdChar, char *data) {
char message[MAXBUF];

    switch (cmdChar) {
        case 'M':
        case 'm':
            char destinationHandle[100];
            if (parseM(data, destinationHandle, message)) {
                printf("Dest Handle: %s\t Message: %s\n", destinationHandle, message);
                sendMessage(handle, socketNum, destinationHandle, message);
            }
            break;

        case 'C':
        case 'c':
            int numHandles = parseC(data, message); 
            if (numHandles > 0) {
                printf("Parsed data successfully. Number of handles: %d\n", numHandles);
                printf("Handles:\n");
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
                displayPrompt = false;
            }
            break;

        default:
            printf("Invalid command: %c\n", cmdChar);
            break;
    }
}

void broadcast(char* handle, int socketNum, char *message){
    uint8_t handleLength = strlen(handle);
    uint8_t pdu[MAXBUF];
    int pduLen = 0;

    printf("Broadcasting message from handle: %s\n", handle);

// ----- Insert Flag -----
    pdu[0] = FLAG_BROADCAST; 
    pduLen++; 

// ----- Sender: Handle Length, Handle Name -----
    pdu[pduLen] = handleLength;
	pduLen++;
    memcpy(pdu + pduLen, handle, handleLength);
    pduLen += handleLength; 

// ----- Insert Message -----
    int messageLength = strlen(message); // Get message length
    memcpy(pdu + pduLen, message, messageLength); 
    pduLen += messageLength; 

// ----- sendPDU -----
    if (sendPDU(socketNum, pdu, pduLen) < 0) {
        perror("Failed to send PDU");
        exit(-1);
    }
}


void ccList(char *handle, int socketNum){
    uint8_t handleLength = strlen(handle);
    uint8_t pdu[MAXBUF];
    int pduLen = 0;

// ----- Insert Flag -----
    pdu[0] = FLAG_LIST; 
    pduLen++; 

// ----- Sender: Handle Length, Handle Name -----
    pdu[pduLen] = handleLength;
	pduLen++;
    memcpy(pdu + pduLen, handle, handleLength);
    pduLen += handleLength; 

// ----- sendPDU -----
    if (sendPDU(socketNum, pdu, pduLen) < 0) {
        perror("Failed to send PDU");
        exit(-1);
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
            strncpy(message, token, MAXBUF - 1);
            message[MAXBUF - 1] = '\0';
        } else {
            printf("Error: Message is missing.\n");
            return 0;
        }
    }

    return numHandles; // Return the number of handles parsed
}

void sendMulticast(char *handle, int socketNum, int numHandles, char * message){
    uint8_t handleLength = strlen(handle);
    uint8_t pdu[MAXBUF];
    int pduLen = 0;

// ----- Insert Flag -----
    pdu[0] = FLAG_MULTICAST; 
    pduLen++; 

// ----- Sender: Handle Length, Handle Name -----
    pdu[pduLen] = handleLength;
	pduLen++;
    memcpy(pdu + pduLen, handle, handleLength);
    pduLen += handleLength; 

// ----- Number of Handles -----
    pdu[pduLen] = numHandles;
    pduLen++;

// ----- Handle Length and Handle Names  -----
    int destinationHandleLength = 0; 
    for(int i = 0; i < numHandles; i++){
        destinationHandleLength = strlen(handleNames[i]);
        pdu[pduLen] = destinationHandleLength; 
        pduLen++; 
        memcpy(pdu + pduLen, handleNames[i], destinationHandleLength); 
        pduLen += destinationHandleLength; 
        printf("Handle %d: %s (Length: %d)\n", i + 1, handleNames[i], destinationHandleLength);
    }

// ----- Message -----
    int messageLength = strlen(message); 
    memcpy(pdu + pduLen, message, messageLength); 
    pduLen += messageLength; 
    pdu[pduLen] = '\0';

// ----- sendPDU -----
    if (sendPDU(socketNum, pdu, pduLen) < 0) {
        perror("Failed to send PDU");
        exit(-1);
    }
}


bool parseM(char *data, char *destinationHandle, char *message){
    // Calculate the destination handle length
    char *firstSpace = strchr(data, ' ');
    if (!firstSpace) {
        printf("Error: Message must contain a space to separate destination handle from message.\n");
        return false;
    }

    // Ensure the handle length does not exceed maximum size
    int destinationHandleLength = firstSpace - data;
    if (destinationHandleLength >= 99) {  // Leave room for null terminator
        printf("Error: Destination handle is too long.\n");
        return false;
    }

    // Copy the destination handle
    memcpy(destinationHandle, data, destinationHandleLength);
    destinationHandle[destinationHandleLength] = '\0';

    // Skip the space and copy the message
    firstSpace++; // Move past the space
    int messageLength = strlen(firstSpace);
    if (messageLength >= MAXBUF - 1) {  // Leave room for null terminator
        printf("Error: Message is too long.\n");
        return false;
    }

    memcpy(message, firstSpace, messageLength);
    message[messageLength] = '\0';
    return true; 
}



void sendMessage(char *handle, int socketNum, char *destinationHandle, char *message) {
    uint8_t handleLength = strlen(handle);
    uint8_t pdu[MAXBUF];
    int pduLen = 0;

// ----- Insert Flag -----
    pdu[0] = FLAG_MESSAGE; 
    pduLen++; 

// ----- Sender: Handle Length, Handle Name -----
    pdu[pduLen++] = handleLength;
    memcpy(pdu + pduLen, handle, handleLength);
    pduLen += handleLength; 

// ----- M and C bit -----
    pdu[pduLen++] = 1;  

// ----- Destination:  Handle Length, Handle Name -----
    uint8_t destinationHandleLength = strlen(destinationHandle); 

    pdu[pduLen] = destinationHandleLength; 
    pduLen++; 
    memcpy(pdu + pduLen, destinationHandle, destinationHandleLength); 
    pduLen  += destinationHandleLength; 

// ----- Message -----
    uint8_t messageLength = strlen(message); 
    memcpy(pdu + pduLen, message, messageLength); 
    pduLen += messageLength; 
    pdu[pduLen] = '\0';

// ----- sendPDU -----
    if (sendPDU(socketNum, pdu, pduLen) < 0) {
        perror("Failed to send PDU");
        exit(-1);
    }
}


int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	// printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
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

void processMsgFromServer(int socketNum){
	uint8_t pdu[MAXBUF] = {0};
	int pduLen = 0;
	pduLen = recvPDU(socketNum, pdu, MAXBUF);
    if (pduLen == 0) {  // Server closed the connection
        printf("\n---Server Terminated---\n");
        close(socketNum);
        exit(0);
    } else if (pduLen < 0) {
        perror("recvPDU failed");
        close(socketNum);
        return; 
    }

    uint8_t flag = pdu[0];
    uint8_t offset = 1; 
    switch(flag){
        case FLAG_HANDLE_CONFIRM:
            printf("---Valid Username---\n"); 
            displayPrompt = true;
            break; 
        case FLAG_HANDLE_REJECT:
            processHandleReject(pdu, pduLen, offset, socketNum);
            displayPrompt = true;
            break; 
        case FLAG_MESSAGE:
            processRecvMessage(pdu, pduLen, offset); 
            displayPrompt = true;
            break; 
        case FLAG_HANDLE_ERROR:
            processHandleError(pdu, pduLen, offset); 
            displayPrompt = true;
            break; 
        case FLAG_LIST_COUNT:
            processCount(pdu, pduLen, offset); 
            displayPrompt = false;
            break; 
        case FLAG_LIST_HANDLE:
            processHandle(pdu, pduLen, offset); 
            displayPrompt = false;
            break; 
        case FLAG_LIST_END:
            displayPrompt = true;
            break; 
        default:
            printf("default"); 
            displayPrompt = true;
            break; 
    }
    if (displayPrompt) {
        printf("$: "); // Display the prompt if allowed
        fflush(stdout);
    }
}

void processHandleReject(uint8_t *pdu, int pduLen, int offset, int socketNum){
    uint8_t handleLen = pdu[offset]; 
    uint8_t handle[100];
    offset++; 
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
    uint8_t handleLen = pdu[offset]; 
    uint8_t handle[100];
    offset++; 
    memcpy(handle, pdu + offset, handleLen); 
    handle[handleLen] = '\0'; 
    printf("\t%s\n", handle); 
}

void processRecvMessage(uint8_t *pdu, int pduLen, int offset){
    // printf("Message Recieved\n"); 
// ----- Sender: Handle Length, Handle name -----
    uint8_t senderHandleLength = pdu[offset];
    offset++; 
    uint8_t senderHandle[100];
    memcpy(senderHandle, pdu + offset, senderHandleLength);
    senderHandle[senderHandleLength] = '\0'; 
    offset += senderHandleLength; 

// ----- M and C bit -----
    // uint8_t mcBit = pdu[offset]; 
    offset++; 

// ----- Destination:  Handle Length, Handle Name -----
    uint8_t destinationHandleLength = pdu[offset];
    offset++; 
    // uint8_t destinationHandle[100];
    // memcpy(destinationHandle, pdu + offset, destinationHandleLength);
    offset += destinationHandleLength; 

// ----- Message -----
    // printf("Offset: %d\tpduLen: %d", offset, pduLen); 
    uint8_t message[100]; 
    memcpy(message, pdu + offset, pduLen - offset); 
    message[pduLen - offset] = '\0'; // Always NUll

    printf("\n%s: %s\n", senderHandle, message); 
}


void processHandleError(uint8_t *pdu, int pduLen, int offset){
    int handleLength = pdu[offset];
    offset++; 
    uint8_t handle[100];
    memcpy(handle, pdu + offset, handleLength); 
    handle[handleLength] = '\0'; 

    printf("Client with handle <%s> does not exist\n", handle); 
}
