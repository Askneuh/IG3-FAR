#ifndef USERS_H
#define USERS_H

#define MAX_USERS 100
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int is_admin;
} User;

int loadUsers(const char* filename, User* users, int* nbUsers);
int saveUsers(const char* filename, User* users, int nbUsers);
int findUser(User* users, int nbUsers, const char* username);
int checkPassword(User* users, int idx, const char* password);

#endif