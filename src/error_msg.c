#include <error_msg.h>
#include <arpch.h>

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
		"%s:%lu:%lu: error: ",
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
	print_file_line(srcfile, line);
	for (int c = 0; c < bar_indent; c++) fprintf(stderr, " ");
	fprintf(stderr, "| ");

	char* beg_of_line = get_line_in_file(srcfile, line);
	for (u64 c = 0; c < column - 1; c++) {
		if (beg_of_line[c] == '\t') print_tab();
		else fprintf(stderr, " ");
	}

	// TODO: check if this works when there are tabs in between
	// error markers
	char* column_start = beg_of_line + column - 1;
	for (u64 c = 0; c < char_count; c++) {
		if (column_start[c] == '\t') fprintf(stderr, "^^^^");
		else fprintf(stderr, "^");
	}
	fprintf(stderr, "\n");

	va_end(aq);
}

