#include <arpch.h>

void fatal_error(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	fprintf(stderr, "aria: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	va_end(ap);
	exit(1);
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fatal_error("no input files specified; aborting");
	}

	const char* srcfile_name = argv[1];
	File* srcfile = file_read(srcfile_name);
	if (!srcfile) {
		fatal_error("cannot read `%s`", srcfile_name);
	}
}
