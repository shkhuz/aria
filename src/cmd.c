#include "cmd.h"
#include "core.h"

char* g_green_color = EC_8BITCOLOR("3", "0");
char* g_error_color = EC_8BITCOLOR("196", "1");
char* g_warning_color = EC_8BITCOLOR("184", "1"); // 208
char* g_note_color = "\x1B[1m";
char* g_bold_color = "\x1B[0;1m";
char* g_reset_color = "\x1B[0m";
char* g_cornflower_blue_color = EC_8BITCOLOR("69", "1");

void init_cmd() {
    if (!isatty(2)) {
        g_green_color = "";
        g_error_color = "";
        g_warning_color = "";
        g_note_color = "";
        g_bold_color = "";
        g_reset_color = "";
        g_cornflower_blue_color = "";
    }
}
