#include <lexer.h>
#include <token.h>
#include <compiler.h>
#include <arpch.h>
#include <error_value.h>
#include <error_msg.h>
#include <keywords.h>

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
        }
        if (output.parser) buf_push(parsers, output.parser);
    }

    if (error_read) {
        fatal_error_common("one or more files were not read: aborting compilation");
    }
    else if (error_lex || error_parse) {
        fatal_error_common("aborting compilation");
    }
}

