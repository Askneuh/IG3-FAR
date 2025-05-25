#include "users.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>  // For inet_ntop
#include <netinet/in.h> // For struct sockaddr_in
#define FILENAME "users.txt"
#define INET_ADDRSTRLEN 16
int loadUsers(const char* filename, User* users, int* nbUsers) {
    FILE* f = fopen(filename, "r");
    if (!f) return -1;
    *nbUsers = 0;
    while (fscanf(f, "%s %s %d", users[*nbUsers].username, users[*nbUsers].password, &users[*nbUsers].is_admin) == 3) {
        (*nbUsers)++;
    }
    fclose(f);
    return 0;
}

int saveUsers(const char* filename, User* users, int nbUsers) {
    FILE* f = fopen(filename, "w");
    if (!f) return -1;
    for (int i = 0; i < nbUsers; i++) {
        fprintf(f, "%s %s %d\n", users[i].username, users[i].password, users[i].is_admin);
    }
    fclose(f);
    return 0;
}

int findUser(User* users, int nbUsers, const char* username) {
    for (int i = 0; i < nbUsers; i++) {
        if (strcmp(users[i].username, username) == 0) return i;
    }
    return -1;
}

int checkPassword(User* users, int idx, const char* password) {
    return strcmp(users[idx].password, password) == 0;
}

User* addPortToUser(User* users, int idx, int port, struct sockaddr_in* aC) {
    FILE* f = fopen(FILENAME, "r+");
    if (f) {
        char buffer[256];
        int currentLine = 0;

        while (currentLine < idx && fgets(buffer, sizeof(buffer), f) != NULL) {
            currentLine++;
        }

        if (fgets(buffer, sizeof(buffer), f) != NULL) {
            // Convert the IP address to a string
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(aC->sin_addr), ip_str, INET_ADDRSTRLEN);
            
            // Parse existing line
            char username[100], password[100];
            int is_admin;
            sscanf(buffer, "%s %s %d", username, password, &is_admin);
            
            // Go back to beginning of the line
            fseek(f, -strlen(buffer), SEEK_CUR);
            
            // Write updated user data with port and address
            fprintf(f, "%s %s %d %d %s\n", username, password, is_admin, port, ip_str); 
        }
    }
    close(f);
    return users;
}

User* addUser(User* users, int* nbUsers, const char* username, const char* password, int port ,struct sockaddr_in* aC) {
    if (*nbUsers < MAX_USERS) {
        strcpy(users[*nbUsers].username, username);
        strcpy(users[*nbUsers].password, password);
        users[*nbUsers].is_admin = 0; 
        (*nbUsers)++;
        FILE* f = fopen(FILENAME, "r+");
        if (f) {
            fseek(f, 0, SEEK_END); // Go to end of file
            // Convert the IP address to a string
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(aC->sin_addr), ip_str, INET_ADDRSTRLEN);
            fprintf(f, "%s %s %d\n", users[*nbUsers-1].username, users[*nbUsers-1].password, users[*nbUsers-1].is_admin, port, ip_str);
            fclose(f);
        }
        close(f);
        return users;
    }
}

