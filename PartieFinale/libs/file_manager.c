#include "file_manager.h"

FileManager* open_file(const char *filename, const char *mode) {
    FileManager *fm = (FileManager*)malloc(sizeof(FileManager));
    if (fm == NULL) {
        return NULL;
    }

    fm->file = fopen(filename, mode);
    if (fm->file == NULL) {
        free(fm);
        return NULL;
    }

    fm->filename = strdup(filename);
    strncpy(fm->mode, mode, 3);
    fm->mode[3] = '\0';

    return fm;
}

void close_file(FileManager *fm) {
    if (fm != NULL) {
        if (fm->file != NULL) {
            fclose(fm->file);
        }
        if (fm->filename != NULL) {
            free(fm->filename);
        }
        free(fm);
    }
}

char* read_line(FileManager *fm) {
    if (fm == NULL || fm->file == NULL) {
        return NULL;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    read = getline(&line, &len, fm->file);
    if (read == -1) {
        free(line);
        return NULL;
    }

    return line;
}

void write_line(FileManager *fm, const char *line) {
    if (fm == NULL || fm->file == NULL || line == NULL) {
        return;
    }

    fprintf(fm->file, "%s\n", line);
}

char* read_all(FileManager *fm) {
    if (fm == NULL || fm->file == NULL) {
        return NULL;
    }

    fseek(fm->file, 0, SEEK_END);
    long length = ftell(fm->file);
    fseek(fm->file, 0, SEEK_SET);

    char *buffer = (char*)malloc(length + 1);
    if (buffer == NULL) {
        return NULL;
    }

    fread(buffer, 1, length, fm->file);
    buffer[length] = '\0';

    return buffer;
}

// Fonction pour écrire tout le contenu dans un fichier
void write_all(FileManager *fm, const char *content) {
    if (fm == NULL || fm->file == NULL || content == NULL) {
        return;
    }

    fputs(content, fm->file);
}
