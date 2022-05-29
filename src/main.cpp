#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <unistd.h>
#include <stdio.h>

#include "core.cpp"
#include "bigint.cpp"
#include "file_io.cpp"
#include "types.cpp"
#include "msg.cpp"
#include "lex.cpp"
#include "parse.cpp"
#include "resolve.cpp"
#include "type_chk.cpp"
#include "cg_llvm.cpp"
#include <getopt.h>

std::string g_exec_path;

static Srcfile* read_srcfile(
        const std::string& path, 
        MsgKind error_kind,
        Srcfile* error_srcfile,
        size_t error_line,
        size_t error_col,
        size_t error_ch_count) {
    FileOrError meta_file = read_file(path);
    switch (meta_file.status) {
        case FileOpenOp::success: {
            ALLOC_WITH_TYPE(srcfile, Srcfile);
            srcfile->handle = meta_file.handle;
            return srcfile;
        } break;

        case FileOpenOp::failure: {
            default_msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_col,
                    error_ch_count,
                    "cannot read source file `{}{}{}`",
                    g_bold_color,
                    path,
                    g_reset_color);
        } break;

        case FileOpenOp::directory: {
            default_msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_col,
                    error_ch_count,
                    "`{}{}{}` is a directory",
                    g_bold_color,
                    path,
                    g_reset_color);
        } break;
    }
    return null;
}

static void tests() {
    assert(align_to_pow2(7, 8) == 8);
    assert(align_to_pow2(1, 16) == 16);
    assert(align_to_pow2(8, 8) == 8);
    assert(align_to_pow2(0, 4) == 0);
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);
    if (rv)
        perror(fpath);
    return rv;
}

int rmrf(char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

int main(int argc, char* argv[]) {
    g_exec_path = argv[0];
    init_core();
    init_types();
    init_lex();
    tests();
    
    char* outpath = "a.out";
    option options[] = {
        { "output", required_argument, 0, 'o' },
        { 0, 0, 0, 0 },
    };

    while (true) {
        int longopt_idx = 0;
        int c = getopt_long(argc, argv, "o:", options, &longopt_idx);
        if (c == -1) break;

        switch (c) {
            case 'o': {
                outpath = optarg;
            } break;
            
            case '?': {
                exit(1);
            } break;

            default: {
            } break;
        }
    }

    if (optind == argc) {
        default_msg(
                MSG_KIND_ROOT_ERROR,
                null, 
                0,
                0,
                0,
                "no input files");
        terminate_compilation();
    }

    std::vector<Srcfile*> srcfiles;
    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        Srcfile* srcfile = read_srcfile(
                argv[i],
                MSG_KIND_ROOT_ERROR,
                null,
                0,
                0,
                0);
        if (!srcfile) {
            read_error = true;
            continue;
        }
        else {
            srcfiles.push_back(srcfile);
        }
    }
    if (read_error) terminate_compilation();

    bool parsing_error = false;
    for (Srcfile* srcfile: srcfiles) {
        LexContext l;
        l.srcfile = srcfile;
        lex(&l);
        if (l.error) {
            parsing_error = true;
            continue;
        }
        /* else { */
        /*     for (Token* token : srcfiles[i]->tokens) { */
        /*         fmt::print("token: {}\n", *token); */
        /*     } */
        /* } */

        ParseContext p;
        p.srcfile = srcfile;
        parse(&p);
        if (p.error) parsing_error = true;
    }
    if (parsing_error) terminate_compilation();
    
    bool resolving_error = false;
    for (Srcfile* srcfile: srcfiles) {
        ResolveContext r;
        r.srcfile = srcfile;
        resolve(&r);
        if (r.error) resolving_error = true;
    }
    if (resolving_error) terminate_compilation();

    bool type_chking_error = false;
    for (Srcfile* srcfile: srcfiles) {
        CheckContext c;
        c.srcfile = srcfile;
        check(&c);
        if (c.error) type_chking_error = true;
    }
    if (type_chking_error) terminate_compilation();
    
    char tmpdir[] = "/tmp/ariac-XXXXXX";
    mkdtemp(tmpdir);

    std::vector<CgContext> cg_ctxs;
    init_cg();
    for (Srcfile* srcfile: srcfiles) {
        CgContext c;
        c.srcfile = srcfile;
        c.tmpdir = fmt::format("{}/", tmpdir);;
        cg(&c);
        cg_ctxs.push_back(c);
    }
    deinit_cg();

    std::string linkcmd = fmt::format("ld -o {}", outpath);
    for (CgContext& cg: cg_ctxs) {
        linkcmd = fmt::format("{} {}", linkcmd, cg.objpath);
    }
    linkcmd = fmt::format("{} {}", linkcmd, "examples/std.asm.o");
    std::cout << "cmd: " << linkcmd << std::endl;
    system(linkcmd.c_str());

    rmrf(tmpdir);
}
