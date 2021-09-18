#include "file_io.h"
#include "stri.h"

static int is_file_dir(const char* fpath) {
    struct stat fpath_stat;
    stat(fpath, &fpath_stat);
    return S_ISDIR(fpath_stat.st_mode);
}

FileOrError read_file(const char* path) {
    /* TODO: more thorough error checking */
    FILE* fp = fopen(path, "r");
    if (!fp) {
        return (FileOrError){ null, FILE_ERROR_FAILURE };
    }

    if (is_file_dir(path)) {
        return (FileOrError){ null, FILE_ERROR_DIR };
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    char* contents = (char*)aria_malloc(size + 1);
    fread((void*)contents, sizeof(char), size, fp);
    contents[size] = '\0';
    fclose(fp);

    char abs_path[PATH_MAX+1];
    char* abs_path_ptr = realpath(path, abs_path);
    if (!abs_path_ptr) {
        return (FileOrError){ null, FILE_ERROR_FAILURE };
    }

    ALLOC_WITH_TYPE(file, File);
    file->path = stri((char*)path);
    file->abs_path = stri(abs_path_ptr);
    file->contents = contents;
    file->len = size;
    return (FileOrError){ file, FILE_ERROR_SUCCESS };
}

bool file_exists(const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

char* get_line_from_file(File* handle, size_t line) {
    char* ptr = handle->contents;
    size_t cur_line = 1;
    while (cur_line != line) {
        while (*ptr != '\n') {
            if (*ptr == '\0') {
                return null;
            } else ptr++;
        }
        ptr++;
        cur_line++;
    }
    return ptr;
}
