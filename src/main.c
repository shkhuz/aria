#include <aria_core.c>
#include <aria.c>
#include <error_msg.c>
#include <lex.c>
#include <parse.c>
#include <resolve.c>
#include <analyze.c>

char* g_executable_path;

bool parse_srcfile(SrcFile* srcfile) {
    Lexer lexer;
    lexer_init(&lexer, srcfile); 
    lexer_lex(&lexer);
    if (lexer.error) {
        return true;
    }

    /* Token** tokens = srcfile->tokens; */
    /* buf_loop(tokens, j) { */
    /*     printf( */
    /*             "[%u, %lu:%lu:%lu] ", */ 
    /*             tokens[j]->kind, */ 
    /*             tokens[j]->line, */ 
    /*             tokens[j]->column, */ 
    /*             tokens[j]->char_count); */
    /*     for (char* start = tokens[j]->start; start != tokens[j]->end; start++) { */
    /*         printf("%c", *start); */
    /*     } */
    /*     printf("\n"); */
    /* } */

    Parser parser;
    parser_init(&parser, srcfile);
    parser_parse(&parser);
    if (parser.error) {
        return true;
    }

    return false;
}

bool parse_srcfiles(SrcFile** srcfiles) {
    bool error = false;
    buf_loop(srcfiles, i) {
        bool current_error = parse_srcfile(srcfiles[i]);
        if (!error) {
            error = current_error;
        }
    }
    return error;
}

bool resolve_srcfile(SrcFile* srcfile) {
    Resolver resolver;
    resolver_init(&resolver, srcfile);
    resolver_resolve(&resolver);
    if (resolver.error) {
        return true;
    }
    return false;
}

bool resolve_srcfiles(SrcFile** srcfiles) {
    bool error = false;
    buf_loop(srcfiles, i) {
        bool current_error = resolve_srcfile(srcfiles[i]);
        if (!error) {
            error = current_error;
        }
    }
    return error;
}

bool analyze_srcfile(SrcFile* srcfile) {
    Analyzer analyzer;
    analyzer_init(&analyzer, srcfile);
    analyzer_analyze(&analyzer);
    if (analyzer.error) {
        return true;
    }
    return false;
}

bool analyze_srcfiles(SrcFile** srcfiles) {
    bool error = false;
    buf_loop(srcfiles, i) {
        bool current_error = analyze_srcfile(srcfiles[i]);
        if (!error) {
            error = current_error;
        }
    }
    return error;
}

SrcFile* read_srcfile_or_error(
        char* fpath, 
        MsgKind error_kind, 
        SrcFile* error_srcfile, 
        u64 line, 
        u64 column, 
        u64 char_count) {
    FileOrError file = file_read(fpath);
    if (file.status == FILE_ERROR_SUCCESS) {
        alloc_with_type(srcfile, SrcFile);
        srcfile->contents = file.file;
        return srcfile;
    } else if (file.status == FILE_ERROR_ERROR) {
        msg_user(
                error_kind, 
                error_srcfile, 
                line, 
                column, 
                char_count, 
                "cannot read source file `%s`", 
                fpath);
        return null;
    } else if (file.status == FILE_ERROR_DIR) {
        msg_user(
                error_kind, 
                error_srcfile, 
                line, 
                column, 
                char_count, 
                "`%s`: is a directory", 
                fpath);
        return null;
    }
    return null;
}

int main(int argc, char* argv[]) {
    ///// TESTS /////
    {
        u8* buffer = null;
        
        buf_push(buffer, 1);
        buf_push(buffer, 2);
        buf_push(buffer, 3);
        
        assert(buffer[0] == 1);
        assert(buffer[1] == 2);
        assert(buffer[2] == 3);

        assert(buf_len(buffer) == 3);
        buf_remove(buffer, 2);
        assert(buf_len(buffer) == 2);
        buf_remove(buffer, 1);
        assert(buf_len(buffer) == 1);
        buf_remove(buffer, 0);
        assert(buf_len(buffer) == 0);
    }

    {
        char* string1 = "hello";
        char* string2 = "hello";
        assert(stri(string1) == stri(string2));

        char* string3 = "Hello";
        assert(stri(string1) != stri(string3));

        char* empty = "";
        assert(stri("") == stri(empty));
    }

    {
        alloc_with_type(i, u8);
        free(i);    
    }

    {
        printf(":: ULLONG_MAX: %llu\n", ULLONG_MAX);
        printf(":: ASCII value of `0` (dec): %u\n", (unsigned int)'0');
    }
    ///// TESTS END /////   

    init_builtin_types();
    g_executable_path = argv[0];

    if (argc < 2) {
        msg_user(
                MSG_KIND_ROOT_ERR, 
                null, 
                0, 
                0, 
                0, 
                "no input files");
        terminate_compilation();
    }

    // `File` is a regular I/O file
    // `SrcFile` is a wrapper for `File`
    // and the parsing context (symbol table, imports, etc)
    SrcFile** srcfiles = null;
    bool srcfile_error = false;
    for (int i = 1; i < argc; i++) {
        SrcFile* srcfile = read_srcfile_or_error(
                argv[i], 
                MSG_KIND_ROOT_ERR, 
                null, 
                0, 
                0, 
                0);
        if (!srcfile) {
            srcfile_error = true;
            continue;
        }

        buf_push(srcfiles, srcfile);
    }

    if (srcfile_error) {
        terminate_compilation();
    }   

    bool parsing_error = parse_srcfiles(srcfiles);
    if (parsing_error) {
        terminate_compilation();
    }

    /* bool import_files_error = false; */
    /* buf_loop(srcfiles, s) { */
    /*  buf_loop(srcfiles[s]->stmts, i) { */
    /*      if (srcfiles[s]->stmts[i]->ty == ST_IMPORTED_NAMESPACE) { */
    /*          bool parsed = false; */
    /*          buf_loop(srcfiles, ss) { */
    /*              if (stri(srcfiles[s]->stmts[i]->imported_namespace.fpath) == stri(srcfiles[ss]->contents->fpath)) { */
    /*                  srcfiles[s]->stmts[i]->imported_namespace.srcfile = srcfiles[ss]; */
    /*                  parsed = true; */
    /*                  break; */
    /*              } */
    /*          } */

    /*          if (parsed) { */
    /*              continue; */
    /*          } */

    /*          Token* import = srcfiles[s]->stmts[i]->ident; */
    /*          SrcFile* srcfile = read_srcfile_or_error( */
    /*                  srcfiles[s]->stmts[i]->imported_namespace.fpath, */
    /*                  MSG_KIND_ERR, */
    /*                  import->srcfile, */
    /*                  import->line, */
    /*                  import->column, */
    /*                  import->char_count); */
    /*          if (!srcfile) { */
    /*              import_files_error = true; */
    /*              continue; */
    /*          } */
    /*          srcfiles[s]->stmts[i]->imported_namespace.srcfile = srcfile; */

    /*          bool current_import_file_error = parse_srcfile(srcfile); */
    /*          if (!current_import_file_error) { */
    /*              buf_push(srcfiles, srcfile); */
    /*          } else { */
    /*              import_files_error = true; */
    /*          } */
    /*      } */
    /*  } */
    /* } */

    /* if (import_files_error) { */
    /*  terminate_compilation(); */
    /* } */

    bool resolving_error = resolve_srcfiles(srcfiles);
    if (resolving_error) {
        terminate_compilation();
    }

    bool analyzing_error = analyze_srcfiles(srcfiles);
    if (analyzing_error) {
        terminate_compilation();
    }
}
