#ifndef FILE_IO_H
#define FILE_IO_H

#include "core.h"

typedef struct {
    const char* path;
    const char* abs_path;
    const char* contents;
    usize len;
} File;

typedef enum {
    FOO_FAILURE,
    FOO_SUCCESS,
    FOO_DIRECTORY,
} FileOpenOp;

typedef struct {
    File handle;
    FileOpenOp status;
} FileOrError;

int is_dir(const char* path);
FileOrError read_file(const char* path);
const char* file_get_line_ptr(const File* handle, usize line);

#endif
