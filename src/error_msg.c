#define _ERROR_MSG_NO_ERROR_TOKEN_MACRO
#include <error_msg.h>
#undef _ERROR_MSG_NO_ERROR_TOKEN_MACRO
#include <token.h>
#include <arpch.h>

#define ANSI_FRED     "\e[1;31m"
#define ANSI_FGREEN   "\x1b[32m"
#define ANSI_FYELLOW  "\x1b[33m"
#define ANSI_FBLUE    "\x1b[34m"
#define ANSI_FMAGENTA "\x1b[35m"
#define ANSI_FCYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"

static char* get_line_in_file(File* srcfile, u64 line) {
	char* line_start = srcfile->contents;
	u64 line_idx = 1;
	while (line_idx != line) {
		while (*line_start != '\n') {
			if (*line_start == '\0') {
				return null;
			}
			else line_start++;
		}
		line_start++;
		line_idx++;
	}
	return line_start;
}

static void print_tab(void) {
	for (u64 t = 0; t < TAB_COUNT; t++) {
		fprintf(stderr, " ");
	}
}

void error(
        File* srcfile,
        u64 line,
        u64 column,
        u64 char_count,
        const char* fmt,
        va_list ap) {

	va_list aq;
	va_copy(aq, ap);
	fprintf(
		stderr,
		ANSI_FRED "%s" ANSI_RESET ":"
        ANSI_FRED "%lu" ANSI_RESET ":"
        ANSI_FRED "%lu" ANSI_RESET": "
        ANSI_FRED "error" ANSI_RESET ": ",
		srcfile->fpath,
		line,
		column);
	vfprintf(
		stderr,
		fmt,
		aq);
	fprintf(stderr, "\n");

	int bar_indent = fprintf(stderr, "%6lu | ", line) - 2;
	assert(bar_indent > 0);

	char* line_start_store = get_line_in_file(srcfile, line);
	char* line_start = line_start_store;
	while (*line_start != '\n' && *line_start != '\0') {
        u64 color_to_put_to = (u64)(line_start - line_start_store + 1);
        if (color_to_put_to == column) {
            fprintf(stderr, ANSI_FRED);
        }
        if (color_to_put_to == column + char_count) {
            fprintf(stderr, ANSI_RESET);
        }
		if (*line_start == '\t') print_tab();
		else fprintf(stderr, "%c", *line_start);
		line_start++;
	}
	fprintf(stderr, "\n");

	for (int c = 0; c < bar_indent; c++) fprintf(stderr, " ");
	fprintf(stderr, "| ");

	char* beg_of_line = get_line_in_file(srcfile, line);
	for (u64 c = 0; c < column - 1; c++) {
		if (beg_of_line[c] == '\t') print_tab();
		else fprintf(stderr, " ");
	}

	// TODO: check if this works when there are tabs in between
	// error markers
    fprintf(stderr, ANSI_FRED);
	char* column_start = beg_of_line + column - 1;
	for (u64 c = 0; c < char_count; c++) {
		if (column_start[c] == '\t') fprintf(stderr, "^^^^");
		else fprintf(stderr, "^");
	}
    fprintf(stderr, ANSI_RESET);
	fprintf(stderr, "\n");

	va_end(aq);
}

void error_token(
        File* srcfile,
        Token* token,
        const char* fmt,
        ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_token(
            srcfile,
            token,
            fmt,
            ap);
    va_end(ap);
}

void verror_token(
        File* srcfile,
        Token* token,
        const char* fmt,
        va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    error(
            srcfile,
            token->line,
            token->column,
            token->char_count,
            fmt,
            ap);
    va_end(aq);
}

/* shared across source files (no unique source file path) */
static void _error_common(const char* fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);

	fprintf(stderr, "aria: ");
	vfprintf(stderr, fmt, aq);
	fprintf(stderr, "\n");

    va_end(aq);
}

void error_common(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
    _error_common(fmt, ap);
	va_end(ap);
}

void fatal_error_common(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
    _error_common(fmt, ap);
	va_end(ap);
	exit(1);
}

