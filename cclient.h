#ifndef __CCLIENT_H__
#define __CCLIENT_H__


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

#define MAXBUF 1024
#define DEBUG_FLAG 1

// ----- Lab Functions -----
void clientControl(int socketNum); 
void processStdin(int socketNum);
void processMsgFromServer(int socketNum); 

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);

// ----- helper Functoins -----
void processCommand(int socketNum, char cmdChar, char * text); 


// ----- Commands -----
void sendMessage(int socketNum, char handle, char * text)
void broadcast(char * sendBuf); 
void multicast(int numHandles, char handles[], char * text); 
void ccListen(); 

#endif