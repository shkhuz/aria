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
    FILEIO_FAILURE,
    FILEIO_SUCCESS,
    FILEIO_DIRECTORY,
} FileOpResult;

typedef struct {
    File handle;
    FileOpResult status;
} FileOrError;

bool file_exists(const char* path);
int is_dir(const char* path);
FileOrError read_file(const char* path);
FileOpResult write_bin_file(const char* path, const char* contents, u64 bytes);
const char* file_get_line_ptr(const File* handle, usize line);

#endif
