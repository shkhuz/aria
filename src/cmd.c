#include "cmd.h"
#include "core.h"

char* g_green_color = EC_8BITCOLOR("3", "0");
char* g_bold_green_color = EC_8BITCOLOR("3", "1");
char* g_red_color = EC_8BITCOLOR("196", "0");
char* g_bold_red_color = EC_8BITCOLOR("196", "1");
char* g_error_color = EC_8BITCOLOR("196", "1");
char* g_warning_color = EC_8BITCOLOR("184", "1"); // 208
char* g_note_color = "\x1B[1m";
char* g_bold_color = "\x1B[0;1m";
char* g_bold_grey_color = EC_8BITCOLOR("244", "1");
char* g_grey_color = EC_8BITCOLOR("247", "0");
char* g_reset_color = "\x1B[0m";
char* g_bold_cornflower_blue_color = EC_8BITCOLOR("69", "1");

void init_cmd() {
    if (!isatty(2)) {
        g_green_color = "";
        g_bold_green_color = "";
        g_red_color = "";
        g_bold_red_color = "";
        g_error_color = "";
        g_warning_color = "";
        g_note_color = "";
        g_bold_color = "";
        g_bold_grey_color = "";
        g_grey_color = "";
        g_reset_color = "";
        g_bold_cornflower_blue_color = "";
    }
}
