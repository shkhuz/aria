#include <aria_core.cpp>
#include <aria.cpp>
#include <error_msg.cpp>
#include <lex.cpp>
#include <parse.cpp>
#include <resolve.cpp>
#include <check.cpp>

char* g_exec_path;

Srcfile* read_srcfile_or_error(
        char* path,
        msg::MsgKind error_kind,
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
        msg::default_msg(
                error_kind,
                error_srcfile,
                line,
                column,
                char_count,
                "cannot read source file `", path, "`");
    } else if (opt_handle.status == fio::FileOpenOperation::directory) {
        msg::default_msg(
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
    init_builtin_types();

    if (argc < 2) {
        msg::default_msg(
                msg::MsgKind::root_err,
                nullptr,
                0,
                0,
                0,
                "no input files");
        msg::terminate_compilation();
    }

    std::vector<Srcfile*> srcfiles;
    bool read_error = false;
    for (int i = 1; i < argc; i++) {
        Srcfile* srcfile = read_srcfile_or_error(
                argv[i], 
                msg::MsgKind::root_err,
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
    if (read_error) msg::terminate_compilation();

    bool parsing_error = false;
    for (auto& srcfile: srcfiles) {
        Lexer lexer(srcfile);
        lexer.run();
        if (lexer.error && !parsing_error) parsing_error = true;

        Parser parser(srcfile);
        parser.run();
        if (parser.error && !parsing_error) parsing_error = true;
    }
    if (parsing_error) msg::terminate_compilation();

    bool resolving_error = false;
    for (auto& srcfile: srcfiles) {
        Resolver resolver(srcfile);
        resolver.run();
        if (resolver.error && !resolving_error) resolving_error = true;
    }
    if (resolving_error) msg::terminate_compilation();

    bool checking_error = false;
    for (auto& srcfile: srcfiles) {
        Checker checker(srcfile);
        checker.run();
        if (checker.error && !checking_error) checking_error = true;
    }
    if (checking_error) msg::terminate_compilation();
}
