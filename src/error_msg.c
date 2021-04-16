#include <aria_core.h>
#include <aria.h>

void msg_user(MsgType ty, u32 code, char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	fprintf(stderr, ANSI_FBOLD);
	switch (ty) {
		case MSG_TY_ROOT:	fprintf(stderr, "%s: ", executable_path_from_argv); break;
		case MSG_TY_ERR:	fprintf(stderr, "error[%04u]: ", code); break;
	}
	fprintf(stderr, ANSI_RESET);

	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

void terminate_compilation() {
	msg_user(MSG_TY_ROOT, 0, "aborting due to previous error(s)");
	exit(EXIT_FAILURE);
}
