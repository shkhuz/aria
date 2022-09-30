#include "cmd.h"
#include "core.h"

char* g_error_color = ANSI_FBOLDRED;
char* g_warning_color = ANSI_FBOLDMAGENTA;
char* g_note_color = ANSI_FBOLDCYAN;
char* g_bold_color = ANSI_FBOLD;
char* g_reset_color = ANSI_RESET;
char* g_fboldcyan_color = ANSI_FBOLDCYAN;
char* g_fcyan_color = ANSI_FCYAN;

void init_cmd() {
    if (!isatty(2)) {
        g_error_color = "";
        g_warning_color = "";
        g_note_color = "";
        g_bold_color = "";
        g_reset_color = "";
        g_fboldcyan_color = "";
        g_fcyan_color = "";
    }
}
