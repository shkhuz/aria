#include "file_io.h"

int is_dir(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

FileOrError read_file(const char* path) {
    // TODO: more thorough error checking
    FILE* raw = fopen(path, "r");
    if (!raw) {
        return (FileOrError){ {}, FOO_FAILURE };
    }

    if (is_dir(path)) {
        return (FileOrError){ {}, FOO_DIRECTORY };
    }

    fseek(raw, 0, SEEK_END);
    usize size = ftell(raw);
    rewind(raw);

    char* contents = (char*)malloc(size + 1);
    fread(contents, sizeof(char), size, raw);
    fclose(raw);
    contents[size] = '\0';

    char abs_path_buf[PATH_MAX + 1];
    char* abs_path = NULL;
    if (realpath(path, abs_path_buf)) {
        usize abs_path_len = strlen(abs_path_buf);
        abs_path = (char*)malloc(abs_path_len + 1);
        memcpy(abs_path, abs_path_buf, abs_path_len+1); // including '\0'
    }

    File handle;
    handle.path = path;
    handle.abs_path = abs_path;
    handle.contents = contents;
    handle.len = size;
    return (FileOrError){ handle, FOO_SUCCESS };
}

const char* file_get_line_ptr(const File* handle, usize line) {
    const char* ptr = handle->contents;
    usize l = 1;
    while (l != line) {
        while (*ptr++ != '\n') {
            if (*ptr == '\0') return NULL;
        }
        l++;
    }
    return ptr;
}
