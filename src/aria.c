#include <lexer.h>
#include <token.h>
#include <compiler.h>
#include <arpch.h>
#include <error_value.h>
#include <error_msg.h>
#include <keywords.h>
#include <str.h>

static bool error_read = false;
static bool error_lex = false;
static bool error_parse = false;

int main(int argc, char** argv) {
    error_read = false;
    keywords_init();

	if (argc < 2) {
		fatal_error_common("no input files specified; aborting");
	}

    Parser** parsers = null;
    for (int f = 1; f < argc; f++) {
        const char* srcfile_name = argv[f];
        Compiler compiler = compiler_new(srcfile_name);
        CompileOutput output = compiler_run(&compiler);
        switch (output.error) {
            case ERROR_READ: error_read = true; break;
            case ERROR_LEX: error_lex = true; break;
            case ERROR_PARSE: error_parse = true; break;
            default: break;
        }
        if (output.parser) buf_push(parsers, output.parser);
    }

    if (error_read) {
        fatal_error_common("one or more files were not read: aborting compilation");
    }
    else if (error_lex || error_parse) {
        fatal_error_common("aborting compilation");
    }

    buf_loop(parsers, p) {
        const char** imports = parsers[p]->imports;
        buf_loop(imports, i) {
            const char* import_file = imports[i];
            const char* current_file = parsers[p]->srcfile->fpath;
            FindCharResult last_slash_res =
                find_last_of(current_file, '/');
            const char* current_file_dir = substr(
                    current_file,
                    0,
                    last_slash_res.found ?
                    last_slash_res.pos + 1 :
                    last_slash_res.pos
            );
            const char* import_file_rel_to_cur = concat(
                    last_slash_res.found ?
                    current_file_dir :
                    "",
                    import_file
            );
            printf("%s, %s\n", import_file, import_file_rel_to_cur);

            buf_loop(parsers, cp) {
                if (str_intern(parsers[cp]->srcfile->fpath) ==
                    str_intern(import_file_rel_to_cur)) {
                    printf("found\n");
                }
            }
        }
    }
}

