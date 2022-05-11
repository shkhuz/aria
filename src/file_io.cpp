struct File {
    std::string path;
    std::string abs_path;
    char* contents;
};

enum class FileOpenOp {
    success,
    failure,
    directory,
};

struct FileOrError {
    File* handle;
    FileOpenOp status;
};

int is_dir(const std::string& path) {
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

FileOrError read_file(const std::string& path) {
    FILE* file_handle = fopen(path.c_str(), "r");
    if (!file_handle) {
        return { null, FileOpenOp::failure };
    }

    if (is_dir(path)) {
        return { null, FileOpenOp::directory };
    }

    fseek(file_handle, 0, SEEK_END);
    size_t size = ftell(file_handle);
    rewind(file_handle);

    char* contents = new char[size + 1];
    std::fread(contents, sizeof(char), size, file_handle);
    contents[size] = '\0';
    fclose(file_handle);

    char abs_path[PATH_MAX + 1];
    char* abs_path_ptr = realpath(path.c_str(), abs_path);
    if (!abs_path_ptr) {
        return { nullptr, FileOpenOp::failure };
    }

    File* file = new File;
    file->path = path;
    file->abs_path = std::string(abs_path_ptr);
    file->contents = contents;
    return { file, FileOpenOp::success };
}

char* get_line_from_file(File* handle, size_t line) {
    char* ch = handle->contents;
    size_t nline = 1;
    while (nline != line) {
        while (*ch != '\n') {
            if (*ch == '\0') {
                return nullptr;
            } else ch++;
        }
        ch++;
        nline++;
    }
    return ch;
}
