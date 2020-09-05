#include <lexer.h>
#include <token.h>
#include <compiler.h>
#include <arpch.h>
#include <error_value.h>
#include <error_msg.h>

static bool error_read;

int main(int argc, char** argv) {
    error_read = false;

	if (argc < 2) {
		fatal_error_common("no input files specified; aborting");
	}

    for (int f = 1; f < argc; f++) {
        const char* srcfile_name = argv[f];
        Compiler compiler = compiler_new(srcfile_name);
        switch (compiler_run(&compiler)) {
            case ERROR_READ: error_read = true; break;
        }
    }

    if (error_read) {
        fatal_error_common("one or more files were not read: aborting compilation");
    }
}

