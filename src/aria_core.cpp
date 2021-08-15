#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <assert.h>

#ifdef __linux__
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u64 u128 __attribute__((mode(TI)));

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i64 i128 __attribute__((mode(TI)));

#define ANSI_FBOLD    "\x1B[1m"
#define ANSI_FRED     "\x1B[1;31m"
#define ANSI_FGREEN   "\x1B[1;32m"
#define ANSI_FYELLOW  "\x1B[33m"
#define ANSI_FBLUE    "\x1B[34m"
#define ANSI_FMAGENTA "\x1B[35m"
#define ANSI_FCYAN    "\x1B[1;36m"
#define ANSI_RESET    "\x1B[0m"

void stderr_print() {
}

template<typename T, typename... Args>
void stderr_print(T first, Args... args) {
    std::cerr << first;
    stderr_print(args...);
}

namespace fio {
    struct File {
        std::string path;
        std::string abs_path;
        char* contents;
    };

    enum class FileOpenOperation {
        success,
        failure,
        directory,
    };

    struct FileOrError {
        File* file;
        FileOpenOperation status;
    };

    int is_dir(const std::string& path) {
        struct stat path_stat;
        stat(path.c_str(), &path_stat);
        return S_ISDIR(path_stat.st_mode);
    }

    FileOrError read(const std::string& path) {
        FILE* file_handle = fopen(path.c_str(), "r");
        if (!file_handle) {
            return { nullptr, FileOpenOperation::failure };
        }

        if (is_dir(path)) {
            return { nullptr, FileOpenOperation::directory };
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
            return { nullptr, FileOpenOperation::failure };
        }

        File* file = new File;
        file->path = path;
        file->abs_path = std::string(abs_path_ptr);
        file->contents = contents;
        return { file, FileOpenOperation::success };
    }
}

#define stack_arr_len(arr) (sizeof(arr) / sizeof(*arr))
