#include "users.h"
#include <stdio.h>
#include <string.h>

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