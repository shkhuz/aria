#include "msg.h"

static size_t g_error_count;
static size_t g_warning_count;

static bool is_root_msg(MsgKind kind) {
    if (kind == MSG_KIND_ROOT_ERROR || kind == MSG_KIND_ROOT_NOTE) {
        return true;
    }
    return false;
}

static void stderr_print_tab() {
    for (int i = 0; i < TAB_SIZE; i++) {
        fputc(' ', stderr);
    }
}

void vdefault_msg(
        MsgKind kind, 
        Srcfile* srcfile, 
        size_t line, 
        size_t column, 
        size_t char_count, 
        const char* fmt,
        va_list ap) {
    va_list aq;
    va_copy(aq, ap);

    if (!is_root_msg(kind)) {
        assert(line > 0);
        assert(column > 0);
        assert(char_count > 0);
    }

    char* src_line = null;
    size_t src_line_length = 0;
    size_t column_new = column;
    size_t char_count_new = char_count;
    if (!is_root_msg(kind)) {
        src_line = get_line_from_file(srcfile->handle, line);
        char* c = src_line;
        while (*c != '\n' && *c != '\0') {
            if (*c == '\t' && (size_t)(c - src_line) < column) {
                column_new += TAB_SIZE - 1;
            }
            c++;
            src_line_length++;
        }
        if (src_line_length - column_new + 1 < char_count) {
            char_count_new = src_line_length - column_new + 1;
        }
    }

    if (!is_root_msg(kind)) {
        fprintf(
                stderr,
                "%s%s:%lu:%lu: %s",
                g_bold_color,
                srcfile->handle->path,
                line, 
                column_new,
                g_reset_color);
    }

    switch (kind) {
        case MSG_KIND_ROOT_ERROR:
        case MSG_KIND_ERROR:
            fprintf(stderr, "%serror: %s", g_error_color, g_reset_color);
            g_error_count++;
            break;
        case MSG_KIND_WARNING:
            fprintf(stderr, "%swarning: %s", g_warning_color, g_reset_color);
            g_warning_count++;
            break;
        case MSG_KIND_ROOT_NOTE:
        case MSG_KIND_NOTE:
            fprintf(stderr, "%snote: %s", g_note_color, g_reset_color);
            break;
    }
    aria_vfprintf(stderr, fmt, ap);

    fprintf(stderr, "\n");
    if (!is_root_msg(kind)) {
        char* src_line_to_print = src_line;
        char* beg_of_src_line = src_line;
        int indent = fprintf(stderr, "%6lu | ", line) - 2;

        char* color = g_reset_color;
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

        while (*src_line_to_print != '\n' && 
               *src_line_to_print != '\0') {
            if ((size_t)(src_line_to_print - beg_of_src_line) == 
                    (column - 1)) {
                fprintf(stderr, "%s", color);
            } 
            if (*src_line_to_print == '\t') 
                stderr_print_tab();
            else
                fprintf(stderr, "%c", *src_line_to_print);
            src_line_to_print++;
            if ((size_t)(src_line_to_print - beg_of_src_line) == 
                (column + char_count - 1)) {
                fprintf(stderr, "%s", g_reset_color);
            }
        }

        fprintf(stderr, "\n");
        for (int c = 0; c < indent; c++) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "| ");
        for (size_t c = 0; c < column_new - 1; c++) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "%s", color);
        for (size_t c = 0; c < char_count_new; c++) {
            fprintf(stderr, "^");
        }
        fprintf(stderr, "%s\n", g_reset_color);
    }
    va_end(aq);
}

void default_msg(
        MsgKind kind, 
        Srcfile* srcfile, 
        size_t line, 
        size_t column, 
        size_t char_count, 
        const char* fmt,
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vdefault_msg(
            kind,
            srcfile,
            line,
            column,
            char_count,
            fmt,
            ap);
    va_end(ap);
}

// TODO: find out where should this go...
void terminate_compilation() {
    default_msg(
            MSG_KIND_ROOT_NOTE, 
            null, 
            0, 
            0, 
            0, 
            "{s}{qu} error(s){s}, {s}{qu} warning(s){s}; aborting compilation",
            g_error_count > 0 ? g_error_color : "",
            g_error_count, 
            g_reset_color,
            g_warning_count > 0 ? g_warning_color : "",
            g_warning_count,
            g_reset_color);
    // TODO: don't exit with error code `g_error_count`.
    // instead make it fixed, and when the user needs the error_count
    // they can get it using a command line switch.
    exit(g_error_count);
}

void default_msg_from_tok(
        MsgKind kind,
        Token* token,
        const char* fmt,
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vdefault_msg(
            kind,
            token->srcfile,
            token->line,
            token->column,
            token->char_count,
            fmt, ap);
    va_end(ap);
}

void error(
        Token* token,
        const char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vdefault_msg(
            MSG_KIND_ERROR,
            token->srcfile,
            token->line,
            token->column,
            token->char_count,
            fmt, 
            ap);
    va_end(ap);
}

void fatal_error(
        Token* token,
        const char* fmt,
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vdefault_msg(
            MSG_KIND_ERROR,
            token->srcfile,
            token->line,
            token->column,
            token->char_count,
            fmt,
            ap);
    terminate_compilation();
}

void warning(
        Token* token,
        const char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vdefault_msg(
            MSG_KIND_WARNING,
            token->srcfile,
            token->line,
            token->column,
            token->char_count,
            fmt, 
            ap);
    va_end(ap);
}

void note(
        Token* token,
        const char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vdefault_msg(
            MSG_KIND_NOTE,
            token->srcfile,
            token->line,
            token->column,
            token->char_count,
            fmt, 
            ap);
    va_end(ap);
}
