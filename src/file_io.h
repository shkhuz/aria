#ifndef FILE_IO_H
#define FILE_IO_H

#include "core.h"

typedef struct {
    char* path;
    char* abs_path;
    char* contents;
    size_t len;
} File;

typedef struct {
    File* handle;
    enum {
        FILE_ERROR_SUCCESS,
        FILE_ERROR_FAILURE,
        FILE_ERROR_DIR,
    } status;
} FileOrError;

FileOrError read_file(const char* fpath);
bool file_exists(const char* fpath);
char* get_line_from_file(File* handle, size_t line);

#endif
