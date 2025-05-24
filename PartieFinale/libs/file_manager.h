#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *file;
    char *filename;
    char mode[4]; 
} FileManager;

FileManager* open_file(const char *filename, const char *mode);

void close_file(FileManager *fm);

char* read_line(FileManager *fm);

void write_line(FileManager *fm, const char *line);

char* read_all(FileManager *fm);

void write_all(FileManager *fm, const char *content);

#endif // FILE_MANAGER_H
