#include <stdio.h>
#include "./libs/users.h"


void main() {
    const User* users;
    int nbUser;
    loadUsers("users.txt", users, nbUser);
    printf(users[0]);
}