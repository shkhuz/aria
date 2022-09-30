#ifndef CMD_H
#define CMD_H

#define ANSI_FBOLD          "\x1B[1m"
#define ANSI_FBOLDRED       "\x1B[1;31m"
#define ANSI_FBOLDGREEN     "\x1B[1;32m"
#define ANSI_FBOLDMAGENTA   "\x1B[1;35m"
#define ANSI_FBOLDCYAN      "\x1B[1;36m"
#define ANSI_FYELLOW        "\x1B[33m"
#define ANSI_FBLUE          "\x1B[34m"
#define ANSI_FCYAN          "\x1B[36m"
#define ANSI_RESET          "\x1B[0m"

extern char* g_error_color;
extern char* g_warning_color;
extern char* g_note_color;
extern char* g_bold_color;
extern char* g_reset_color;
extern char* g_fcyan_color;
extern char* g_fcyann_color;

void init_cmd();

#endif
