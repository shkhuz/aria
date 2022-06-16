#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Utils.h>

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <fcntl.h>

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

inline int rmrf(char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

inline double timediff(timespec tend, timespec tstart) {
    return 
    ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - 
    ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
}

inline void inittimer(timespec* timer) {
    clock_gettime(CLOCK_MONOTONIC_RAW, timer);
}

int main(int argc, char* argv[]) {
    g_exec_path = argv[0];
    init_core();
    init_types();
    init_lex();
    tests();
    
    timespec tstart, tend;
    double compilation_time = 0.0;
    
    inittimer(&tstart);
    
    char* outpath = "a.out";
    char* target_triple = null; // default value in init_cg()
    char* linker = "ld";
    
    option options[] = {
        { "output", required_argument, 0, 'o' },
        { "target", required_argument, 0, 't' },
        { "linker", required_argument, 0, 'l' },
        { "help", no_argument, 0, 'h' },
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
            
            case 't': {
                target_triple = optarg;
            } break;
            
            case 'l': {
                linker = optarg;
            } break;
            
            case 'h': {
                fmt::print(
                           "Aria language compiler\n"
                           "Usage: ariac [options] file...\n"
                           "\n"
                           "Options:\n"
                           "  -o, --output=<file>        Place the output into <file>\n"
                           "  --target=<triple>          Specify a target triple for cross compilation\n"
                           "  --linker=<path>            Specify a path to a linker\n"
                           "  --help                     Display this help and exit\n"
                           "\n"
                           "To report bugs, please see:\n"
                           "<https://github.com/shkhuz/aria/issues/>\n"
                           );
                exit(0);
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
    
    if (target_triple) {
        fmt::print("target: {}\n", target_triple);
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
    
    size_t total_lines_parsed = 0;
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
        /*     for (Token* token : srcfile->tokens) { */
        /*         fmt::print("token: {}\n", *token); */
        /*     } */
        /* } */
        total_lines_parsed += l.lines;
        
        ParseContext p;
        p.srcfile = srcfile;
        parse(&p);
        if (p.error) parsing_error = true;
    }
    if (parsing_error) terminate_compilation();
    fmt::print("Total lines parsed: {}\n", total_lines_parsed);
    
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
    
    inittimer(&tend);
    double frontend_time = timediff(tend, tstart);
    compilation_time += frontend_time;
    printf("Front-end time-taken: %.5fs\n", frontend_time);
    
    inittimer(&tstart);
    if (init_cg(&target_triple)) terminate_compilation();
    
    std::vector<CgContext> cg_ctxs;
    bool gen_error = false;
#if defined (__TERMUX__)
    char tmpdir[] = "/data/data/com.termux/files/usr/tmp/ariac-XXXXXX";
#else
    char tmpdir[] = "/tmp/ariac-XXXXXX";
#endif
    mkdtemp(tmpdir);
    
    for (Srcfile* srcfile: srcfiles) {
        CgContext c;
        c.srcfile = srcfile;
        c.tmpdir = fmt::format("{}/", tmpdir);;
        cg(&c);
        cg_ctxs.push_back(c);
        if (c.error) gen_error = true;
    }
    deinit_cg();
    
    if (gen_error) {
        rmrf(tmpdir);
        terminate_compilation();
    }
    inittimer(&tend);
    double backend_llvm_time = timediff(tend, tstart);
    compilation_time += backend_llvm_time;
    printf("Back-end LLVM time-taken: %.5fs\n", backend_llvm_time);
    
    inittimer(&tstart);
    size_t linkeroptscount = 5 + cg_ctxs.size();
    char** linkeropts = (char**)malloc(linkeroptscount * sizeof(char*));
    linkeropts[0] = linker;
    linkeropts[1] = "-o";
    linkeropts[2] = outpath;
    for (size_t i = 0; i < cg_ctxs.size(); i++) {
        linkeropts[i+3] = (char*)cg_ctxs[i].objpath.c_str();
    }
    
    if (strncmp(target_triple, "x86-64", 6) == 0 ||
        strncmp(target_triple, "x86_64", 6) == 0) {
        linkeropts[cg_ctxs.size()+3] = "std/_start_x86-64.S.o";
    }
    else if (strncmp(target_triple, "aarch64", 7) == 0) {
        linkeropts[cg_ctxs.size()+3] = "std/_start_aarch64.S.o";
    }
    else {
        root_error("{} target is not supported by aria yet", target_triple);
        rmrf(tmpdir);
        terminate_compilation();
    }
    fmt::print("_start file: {}\n", linkeropts[cg_ctxs.size()+3]);
    linkeropts[cg_ctxs.size()+4] = null;
    
    int errdesc[2];
    if (pipe2(errdesc, O_CLOEXEC) == -1) {
        root_error("cannot execute pipe2(): {}", strerror(errno));
        rmrf(tmpdir);
        terminate_compilation();
    }
    
    pid_t linkerproc = fork();
    if (linkerproc == -1) {
        root_error("cannot execute fork(): {}", strerror(errno));
        rmrf(tmpdir);
        terminate_compilation();
    }
    
    if (linkerproc) {
        close(errdesc[1]);
        char buf[2];
        int linkerstatus;
        wait(&linkerstatus);
        
        int bytesread = read(errdesc[0], buf, sizeof(char));
        if (bytesread == 0 && WEXITSTATUS(linkerstatus) != 0) {
            root_error("aborting due to previous linker error");
            rmrf(tmpdir);
            terminate_compilation();
        }
    } else {
        if (execvp(linker, linkeropts) == -1) {
            close(errdesc[0]);
            write(errdesc[1], "F", sizeof(char));
            close(errdesc[1]);
        }
        root_error("cannot execute linker `{}`: No such file or directory", linker);
        rmrf(tmpdir);
        terminate_compilation();
    }
    rmrf(tmpdir);
    inittimer(&tend);
    double linker_time = timediff(tend, tstart);
    compilation_time += linker_time;
    printf("Linker time-taken: %.5fs\n", linker_time);
    printf("Total time-taken: %.5fs\n", compilation_time);    
}
