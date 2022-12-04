#ifndef CMD_H
#define CMD_H

#define EC_8BITCOLOR(colorstr, boldstr) \
    "\x1B[" boldstr ";38;5;" colorstr "m"

extern char* g_error_color;
extern char* g_warning_color;
extern char* g_note_color;
extern char* g_bold_color;
extern char* g_reset_color;
extern char* g_cornflower_blue_color;

void init_cmd();

#endif
