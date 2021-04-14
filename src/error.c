#include <aria_core.h>

void aria_error(char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

void aria_terminate() {
	aria_error("aria: compilation terminated");
	exit(EXIT_FAILURE);
}
