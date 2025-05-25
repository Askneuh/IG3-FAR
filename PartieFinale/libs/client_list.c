#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_list.h"
#include <stdbool.h>

ClientNode* addClient(ClientNode* head, struct client newClient) {
    ClientNode* newNode = malloc(sizeof(ClientNode));
    if (!newNode) {
        perror("malloc");
        return head;
    }

    strcpy(newNode->data.username, newClient.username); 
    newNode->data.adClient = newClient.adClient;
    newNode->data.port = newClient.port;
    newNode->data.dSClient = newClient.dSClient;
    newNode->next = NULL;

    if (head == NULL) return newNode;

    ClientNode* temp = head;
    while (temp->next != NULL) temp = temp->next;
    temp->next = newNode;
    return head;
}

void printClients(ClientNode* head) {
    ClientNode* current = head;
    while (current != NULL) {
        printf("Client: %s, Port: %d\n", current->data.username, current->data.port);
        current = current->next;
    }
}

void freeClients(ClientNode* head) {
    ClientNode* tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        free(tmp);
    }
}

bool clientAlreadyExists(ClientNode* head, struct client c) {
    ClientNode* current = head;
    while (current != NULL) {
        if (strcmp(current->data.username, c.username) == 0 && current->data.port == c.port) {
            return true; 
        }
        current = current->next;
    }
    return false;
}

ClientNode* findClientByName(ClientNode* head, const char* username) {
    ClientNode* current = head;
    while (current != NULL) {
        if (strcmp(current->data.username, username) == 0) {
            return current; 
        }
        current = current->next;
    }
    return NULL; 
}

bool removeClient(ClientNode** head, char* username) {
    if (head == NULL || *head == NULL) {
        return false;
    }

    ClientNode* current = *head;
    ClientNode* prev = NULL;

    if (strcmp(current->data.username, username) == 0) {
        *head = current->next;
        free(current);
        return true;
    }

    while (current != NULL && strcmp(current->data.username, username) != 0) {
        prev = current;
        current = current->next;
    }

    if (current != NULL) {
        prev->next = current->next;
        free(current);
        return true;
    }

    return false;
}

int countClients(ClientNode* head) {
    int count = 0;
    while (head != NULL) {
        count++;
        head = head->next;
    }
    return count;
}



