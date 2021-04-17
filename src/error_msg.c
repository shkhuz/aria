#include <aria_core.h>
#include <aria.h>

static char* get_line_from_file(File* file, u64 line) {
	char* current_char_in_line = file->contents;
	u64 current_line_number = 1;
	while (current_line_number != line) {
		while (*current_char_in_line != '\n') {
			if (*current_char_in_line == '\0') {
				return null;
			} else current_char_in_line++;
		}
		current_char_in_line++;
		current_line_number++;
	}
	return current_char_in_line;
}

static void stderr_print_tab() {
	for (int t = 0; t < TAB_COUNT; t++) {
		fprintf(stderr, " ");
	}
}

void vmsg_user(
		MsgType ty, 
		SrcFile* srcfile, 
		u64 line, 
		u64 column, 
		u64 char_count, 
		u32 code, 
		char* fmt, 
		va_list ap) {

	va_list aq;
	va_copy(aq, ap);

	if (ty != MSG_TY_ROOT_ERR) {
		assert(char_count > 0);
		assert(line > 0);
		assert(column > 0);
	}

	char* src_line = null; 
	u64 column_new = column;

	if (ty != MSG_TY_ROOT_ERR) {
		src_line = get_line_from_file(srcfile->contents, line);
		// Checks for tab characters in line
		// and updates the column to match real tab width
		char* c = src_line;
		while (*c != '\n' && *c != '\0') {
			if (*c == '\t') {
				column_new += TAB_COUNT - 1;
			}
			c++;
		}
	}

	if (ty != MSG_TY_ROOT_ERR) {
		fprintf(
				stderr,
				ANSI_FBOLD
				"%s:%lu:%lu: "
				ANSI_RESET,
				srcfile->contents->fpath,
				line, 
				column_new
		);
	}

	switch (ty) {
		case MSG_TY_ROOT_ERR:
		case MSG_TY_ERR:	
			fprintf(stderr, ANSI_FRED "error[%04u]: " ANSI_RESET, code); 
			break;
	}

	vfprintf(stderr, fmt, aq);
	fprintf(stderr, "\n");

	if (ty != MSG_TY_ROOT_ERR) {
		char* src_line_to_print = src_line;
		char* beg_of_src_line = src_line;
		int indent = fprintf(stderr, "%6lu | ", line) - 2;

		char* color = ANSI_RESET;
		switch (ty) {
			case MSG_TY_ERR:
				color = ANSI_FRED;
				break;
			default:
				assert(0);
		}

		// Source line print
		while (*src_line_to_print != '\n' && *src_line_to_print != '\0') {
			if ((u64)(src_line_to_print - beg_of_src_line) == (column - 1)) {
				fprintf(stderr, "%s", color);
			} 
			
			if (*src_line_to_print == '\t') {
				stderr_print_tab();
			} else {
				fprintf(stderr, "%c", *src_line_to_print);
			}
			src_line_to_print++;

			if ((u64)(src_line_to_print - beg_of_src_line) == (column + char_count - 1)) {
				fprintf(stderr, ANSI_RESET);
			}

		}
		fprintf(stderr, "\n");

		// `^^^^` printing
		for (int c = 0; c < indent; c++) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "| ");
		for (u64 c = 0; c < column_new - 1; c++) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "%s", color);
		for (u64 c = 0; c < char_count; c++) {
			fprintf(stderr, "^");
		}
		fprintf(stderr, ANSI_RESET "\n");
	}

	va_end(aq);
}

void msg_user(
		MsgType ty, 
		SrcFile* srcfile, 
		u64 line, 
		u64 column, 
		u64 char_count, 
		u32 code, 
		char* fmt, 
		...) {

	va_list ap;
	va_start(ap, fmt);
	vmsg_user(ty, srcfile, line, column, char_count, code, fmt, ap);
	va_end(ap);
}

void vmsg_user_token(
		MsgType ty,
		Token* token,
		u32 code,
		char* fmt, 
		va_list ap) {
	va_list aq;
	va_copy(aq, ap);
	vmsg_user(
			ty,
			token->srcfile,
			token->line,
			token->column,
			token->char_count, 
			code,
			fmt,
			ap);
}

void msg_user_token(
		MsgType ty,
		Token* token,
		u32 code,
		char* fmt, 
		...) {
	va_list ap;
	va_start(ap, fmt);
	vmsg_user_token(ty, token, code, fmt, ap);
	va_end(ap);
}

void terminate_compilation() {
	msg_user(MSG_TY_ROOT_ERR, null, 0, 0, 0, ERROR_ABORTING_DUE_TO_PREV_ERRORS);
	// TODO: for CI build, this is changed to
	// always return 0
	/* exit(EXIT_FAILURE); */
	exit(0);
}
