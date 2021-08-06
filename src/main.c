#include <aria_core.c>
#include <aria.c>
#include <error_msg.c>
#include <lex.c>
#include <parse.c>
#include <resolve.c>
#include <analyze.c>
#include <x64_codegen.c>

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

void codegen_srcfile(SrcFile* srcfile) {
    CodeGenerator codegenerator;
    codegenerator_init(&codegenerator, srcfile);
    codegenerator_gen(&codegenerator);
}

void codegen_srcfiles(SrcFile** srcfiles) {
    buf_loop(srcfiles, i) {
        codegen_srcfile(srcfiles[i]);
    }
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

        char* buffer2 = null;
        buf_push(buffer2, '1');
        buf_printf(buffer2, "%s", "hello world!\0");
        assert(buffer2[5] == 'o');
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
        alloc_with_type(alloc_with_type_test, u8);
        free(alloc_with_type_test);

        // zero_memory()
        u8* buf = malloc(17 * sizeof(u8));
        for (size_t i = 0; i < 16; i++) {
            buf[i] = 0xaa;
        }
        buf[16] = 0xaa;
        zero_memory(buf, 16);
        for (size_t i = 0; i < 16; i++) {
            assert(buf[i] == 0);
        }
        assert(buf[16] == 0xaa);
    }

    {
        printf(":: ULLONG_MAX: %llu\n", ULLONG_MAX);
        printf(":: INT32_MAX:  %lu\n", (u64)INT32_MAX);
        printf(":: INT32_MIN:  %ld\n", (i64)INT32_MIN);
        printf(":: UINT32_MAX: %lu\n", (u64)UINT32_MAX);
        assert(sizeof_in_bits(u64) == 64);
        assert(sizeof_in_bits(u32) == 32);
        assert(sizeof_in_bits(u16) == 16);
        assert(sizeof_in_bits(u8) == 8);
        assert(sizeof_in_bits(i64) == 64);
        assert(sizeof_in_bits(i32) == 32);
        assert(sizeof_in_bits(i16) == 16);
        assert(sizeof_in_bits(i8) == 8);

		u8* buf__1 = null;
		buf_fit(buf__1, 8);
		assert(buf_cap(buf__1) >= 8);
    }

    {
        bigint b;
        bigint_init(&b);

        bigint_set_u64(&b, UINT64_MAX);
        bigint_mul(&b, &b, &b);
        assert(!bigint_fits_u64(&b));

        bigint_set_u64(&b, ((u64)UINT8_MAX));
        assert(bigint_fits_u8(&b));
        bigint_set_u64(&b, ((u64)UINT8_MAX)+1);
        assert(!bigint_fits_u8(&b));

        bigint_set_i64(&b, ((i64)INT8_MAX));
        assert(bigint_fits_i8(&b));
        bigint_set_i64(&b, ((i64)INT8_MIN));
        assert(bigint_fits_i8(&b));

        bigint_set_i64(&b, ((i64)INT8_MAX)+1);
        assert(!bigint_fits_i8(&b));
        bigint_set_i64(&b, ((i64)INT8_MIN)-1);
        assert(!bigint_fits_i8(&b));
    }

    {
		bigint a;
        bigint_init_u64(&a, 5);
        
        bigint b;
        bigint_init_i64(&b, 11);

		bigint c;
		bigint_init(&c);
        bigint_mul(&a, &b, &c);

        bigint d;
        bigint_init_i64(&d, (((i64)INT8_MIN)));
        printf("fits: %u\n", bigint_fits_i8(&d));
    }

    ///// TESTS END /////   

    init_primitive_types();
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

    bool import_files_error = false;
    buf_loop(srcfiles, s) {
        buf_loop(srcfiles[s]->nodes, i) {
            if (srcfiles[s]->nodes[i]->kind == NODE_KIND_IMPLICIT_MODULE) {
                bool parsed = false;
                buf_loop(srcfiles, ss) {
                    if (stri(srcfiles[s]->nodes[i]->implicit_module.file_path) == stri(srcfiles[ss]->contents->fpath)) {
                        srcfiles[s]->nodes[i]->implicit_module.srcfile = srcfiles[ss];
                        parsed = true;
                        break;
                    }
                }

                if (parsed) {
                    continue;
                }

                Token* import = srcfiles[s]->nodes[i]->implicit_module.identifier;
                SrcFile* srcfile = read_srcfile_or_error(
                        srcfiles[s]->nodes[i]->implicit_module.file_path,
                        MSG_KIND_ERR,
                        import->srcfile,
                        import->line,
                        import->column,
                        import->char_count);
                if (!srcfile) {
                    import_files_error = true;
                    continue;
                }
                srcfiles[s]->nodes[i]->implicit_module.srcfile = srcfile;

                bool current_import_file_error = parse_srcfile(srcfile);
                if (!current_import_file_error) {
                    buf_push(srcfiles, srcfile);
                } else {
                    import_files_error = true;
                }
            }
        }
    }

    if (import_files_error) {
        terminate_compilation();
    }

    bool resolving_error = resolve_srcfiles(srcfiles);
    if (resolving_error) {
        terminate_compilation();
    }

    bool analyzing_error = analyze_srcfiles(srcfiles);
    if (analyzing_error) {
        terminate_compilation();
    }

    codegen_srcfiles(srcfiles);
}
