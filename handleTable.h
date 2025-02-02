// handle_table.h
#ifndef __HANDLETABLE_H__
#define __HANDLETABLE_H__

#include <stdbool.h>

// Node structure for linked list
typedef struct HandleNode {
    char *handle;
    int socket;
    struct HandleNode *next;
} HandleNode;

// Functions to manage the handle table
HandleNode *createHandleTable(void);
const char *findHandle(HandleNode *head, const char *handle);
const char *findHandleBySocket(HandleNode *head, int socket);
int findSocketByHandle(HandleNode *head, const char *handle); 
bool addHandle(HandleNode **head, const char *handle, int socket); 
bool removeHandle(HandleNode **head, const char *handle);
void destroyHandleTable(HandleNode *head);

const char *getHandleByIndex(HandleNode *head, int index); 
int getNumHandles(HandleNode *head); 



#endif