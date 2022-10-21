#include "msg.h"
#include "core.h"
#include "cmd.h"
#include "printf.h"

static usize g_error_count;
static usize g_warning_count;
static bool g_first_main_msg = true; // only errors and warnings

static int g_barindent; // used for aligning `|` when printing line numbers
static const char* last_error_file_path;

#define write_tab_to_stderr() afprintf(stderr, "\x20\x20\x20\x20") // as 4 spaces

static bool is_root_msg(MsgKind kind) {
    if (kind == MSG_KIND_ROOT_ERROR ||
        kind == MSG_KIND_ROOT_NOTE) {
        return true;
    }
    return false;
}

void vaddinfo(const char* fmt, va_list args) {
    for (int c = 0; c < g_barindent; c++) {
        afprintf(stderr, " ");
    }
    afprintf(stderr, "%s>%s ", g_error_color, g_reset_color);
    avfprintf(stderr, fmt, args);
    afprintf(stderr, "\n");
}

void addinfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vaddinfo(fmt, args);
    va_end(args);
}

void vmsg(
        MsgKind kind,
        Srcfile* srcfile,
        usize line,
        usize col,
        usize ch_count,
        const char* fmt,
        va_list args) {
    bool same_file_note = kind == MSG_KIND_NOTE && 
        last_error_file_path &&
        strcmp(last_error_file_path, srcfile->handle.path) == 0;
    if (kind != MSG_KIND_ADDINFO && kind != MSG_KIND_NOTE) {
        if (g_first_main_msg) g_first_main_msg = false;
        else afprintf(stderr, "\n");
    }

    if (kind == MSG_KIND_ADDINFO) {
        vaddinfo(fmt, args);
        return;
    }

    bool root_msg = is_root_msg(kind);
    if (!root_msg) {
        assert(line > 0);
        assert(col > 0);
        assert(ch_count > 0);
    }

    const char* sline = NULL; // srcline
    usize slinelen = 0;
    usize col_new = col;
    usize ch_count_new = ch_count;

    if (!root_msg) {
        sline = file_get_line_ptr(&srcfile->handle, line);
        const char* c = sline;
        while (*c != '\n' && *c != '\0') {
            if (*c == '\t' && (usize)(c - sline) < col) 
                col_new += 3; // tabsize-1 = 4-1 = 3
            c++;
            slinelen++;
        }
        if (ch_count > slinelen - col_new + 1) {
            ch_count_new = slinelen - col_new + 1;
        }

        if (same_file_note) {
            assert(g_barindent >= 5);
            for (int n = 0; n < g_barindent; n++) afprintf(stderr, " ");
        }
        else {
            afprintf(
                    stderr,
                    "%s%s:%lu:%lu: %s",
                    g_bold_color,
                    srcfile->handle.path,
                    line,
                    col_new,
                    g_reset_color);
        }
    }

    switch (kind) {
        case MSG_KIND_ROOT_ERROR:
        case MSG_KIND_ERROR: {
            afprintf(stderr, "%serror: %s", g_error_color, g_reset_color);
            if (kind == MSG_KIND_ERROR) last_error_file_path = srcfile->handle.path;
            g_error_count++;
        } break;

        case MSG_KIND_WARNING: {
            afprintf(stderr, "%swarning: %s", g_warning_color, g_reset_color);
            g_warning_color++;
        } break;

        case MSG_KIND_ROOT_NOTE: {
            afprintf(stderr, "%snote: %s", g_note_color, g_reset_color);
        } break;

        case MSG_KIND_NOTE: {
            // TODO: when compiler reaches multiple files support,
            // test this
            const char* str;
            if (same_file_note) str = "%snote: %s";
            else str = "%snote: %s...";
            afprintf(stderr, str, g_note_color, g_reset_color);
        } break;
    }

    avfprintf(stderr, fmt, args);
    afprintf(stderr, "\n");

    if (!root_msg) {
        const char* sline_to_print = sline;
        const char* beg_of_sline = sline;

        g_barindent = afprintf(stderr, "%6lu | ", line) - 2;
        
        const char* color = g_reset_color;
        switch (kind) {
            case MSG_KIND_ERROR:
                color = g_error_color;
                break;
            case MSG_KIND_WARNING:
                color = g_warning_color;
                break;
            case MSG_KIND_NOTE:
                color = g_note_color;
                break;
            default:
                assert(0);
        }

        while (*sline_to_print != '\n' && *sline_to_print != '\0') {
            if ((usize)(sline_to_print - beg_of_sline) == col-1) {
                afprintf(stderr, "%s", color);
            }
            if (*sline_to_print == '\t') write_tab_to_stderr();
            else afprintf(stderr, "%c", *sline_to_print);
            sline_to_print++;

            if ((usize)(sline_to_print - beg_of_sline) == (col + ch_count - 1)) {
                afprintf(stderr, "%s", g_reset_color);
            }
        }
        afprintf(stderr, "\n");
        
        for (int c = 0; c < g_barindent; c++) {
            afprintf(stderr, " ");
        }
        afprintf(stderr, "| ");
        for (usize c = 0; c < col_new - 1; c++) {
            afprintf(stderr, " ");
        }
        afprintf(stderr, "%s", color);
        for (usize c = 0; c < ch_count_new; c++) {
            afprintf(stderr, "^");
        }
        afprintf(stderr, "%s\n", g_reset_color);
    }
}

void msg(
    MsgKind kind,
    Srcfile* srcfile,
    usize line,
    usize col,
    usize ch_count,
    const char* fmt,
    ...) {
    va_list args;
    va_start(args, fmt);
    vmsg(kind, srcfile, line, col, ch_count, fmt, args);
    va_end(args);
}

void note_tok(Token* token, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vmsg(
        MSG_KIND_NOTE,
        token->srcfile,
        token->line,
        token->col,
        token->ch_count,
        fmt,
        args);
    va_end(args);
}

void fatal_error_tok(Token* token, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vmsg(MSG_KIND_ERROR,
         token->srcfile,
         token->line,
         token->col,
         token->ch_count,
         fmt,
         args);
    va_end(args);
    terminate_compilation();
}

void error_tok(Token* token, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vmsg(
        MSG_KIND_ERROR,
        token->srcfile,
        token->line,
        token->col,
        token->ch_count,
        fmt,
        args);
    va_end(args);
}
