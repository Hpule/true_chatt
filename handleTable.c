// handle_table.c
#include "handleTable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Create a new handle table
HandleNode* createHandleTable(void) {
    return NULL;  // Initial table is empty
}

// Add a new handle to the table
bool addHandle(HandleNode **head, const char *handle, int socket) {
    if (!handle) return false;

    // Check if the handle already exists
    if (findHandle(*head, handle) != NULL) {
        return false; // Handle already exists, return false
    }

    HandleNode *newNode = malloc(sizeof(HandleNode));
    if (newNode == NULL) return false;

    newNode->handle = strdup(handle);  // Copies the handle and ensures null-termination
    if (newNode->handle == NULL) {
        free(newNode);
        return false;
    }

    newNode->socket = socket;
    newNode->next = *head;
    *head = newNode;
    return true;
}

// Find a handle in the table
const char* findHandle(HandleNode *head, const char *handle) {
    HandleNode *current = head;
    while (current != NULL) {
        if (strcmp(current->handle, handle) == 0) {
            return current->handle;
        }
        current = current->next;
    }
    return NULL;
}

int findSocketByHandle(HandleNode *head, const char *handle){
    HandleNode *current = head; 
    while(current != NULL){
        if(strcmp(current->handle, handle) == 0){
            return current->socket; 
        }
        current = current->next;
    }
    return -1; 
}

// 
const char* findHandleBySocket(HandleNode *head, int socket) {
    HandleNode *current = head;
    while (current != NULL) {
        if (current->socket == socket) {
            return current->handle;
        }
        current = current->next;
    }
    return NULL;
}

// Remove a handle from the table
bool removeHandle(HandleNode **head, const char *handle) {
    HandleNode *current = *head, *prev = NULL;
    while (current != NULL) {
        if (strcmp(current->handle, handle) == 0) {
            if (prev == NULL) {
                *head = current->next;  // Removing the first node
            } else {
                prev->next = current->next;  // Bypass the current node
            }
            free(current->handle);
            free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

// Destroy the handle table
void destroyHandleTable(HandleNode *head) {
    HandleNode *current = head;
    while (current != NULL) {
        HandleNode *next = current->next;
        free(current->handle);
        free(current);
        current = next;
    }
}

// Function to retrieve a handle by its index in the linked list
const char *getHandleByIndex(HandleNode *head, int index) {
    int currentIndex = 0;
    HandleNode *current = head;
    while (current != NULL) {
        if (currentIndex == index) {
            return current->handle;
        }
        current = current->next;
        currentIndex++;
    }
    return NULL; // Return NULL if the index is out of bounds
}

// Function to get the number of handles in the linked list
int getNumHandles(HandleNode *head) {
    int count = 0;
    HandleNode *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}