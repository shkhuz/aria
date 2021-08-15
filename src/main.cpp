#include <aria_core.cpp>
#include <aria.cpp>
#include <error_msg.cpp>
#include <lex.cpp>

char* g_exec_path;

Srcfile* read_srcfile_or_error(
        char* path,
        MsgKind error_kind,
        Srcfile* error_srcfile,
        size_t line,
        size_t column,
        size_t char_count) {
    auto opt_handle = fio::read(std::string(path));
    if (opt_handle.status == fio::FileOpenOperation::success) {
        Srcfile* srcfile = new Srcfile;
        srcfile->handle = opt_handle.file;
        return srcfile;
    } else if (opt_handle.status == fio::FileOpenOperation::failure) {
        msg_user(
                error_kind,
                error_srcfile,
                line,
                column,
                char_count,
                "cannot read source file `", path, "`");
    } else if (opt_handle.status == fio::FileOpenOperation::directory) {
        msg_user(
                error_kind,
                error_srcfile,
                line,
                column,
                char_count,
                "`", path, "` is a directory");
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    g_exec_path = argv[0];

    if (argc < 2) {
        msg_user(
                MsgKind::root_err,
                nullptr,
                0,
                0,
                0,
                "no input files");
        terminate_compilation();
    }

    std::vector<Srcfile*> srcfiles;
    bool read_error = false;
    for (int i = 1; i < argc; i++) {
        Srcfile* srcfile = read_srcfile_or_error(
                argv[i], 
                MsgKind::root_err,
                nullptr,
                0,
                0,
                0);
        if (!srcfile) {
            read_error = true;
            continue;
        }
        srcfiles.push_back(srcfile);
    }
    if (read_error) terminate_compilation();

    bool parsing_error = false;
    for (auto& srcfile: srcfiles) {
        Lexer lexer(srcfile);
        lexer.run();
        if (lexer.error && !parsing_error) parsing_error = true;
        else {
            for (auto& tok: lexer.srcfile->tokens) {
                stderr_print(*tok, "\n");
            }
        }
    }
    if (parsing_error) terminate_compilation();
}
