#include "msg.h"

#define TAB_COUNT 4

static size_t g_error_count;
static size_t g_warning_count;

static bool is_root_msg(MsgKind kind) {
    if (kind == MSG_KIND_ROOT_ERROR || kind == MSG_KIND_ROOT_NOTE) {
        return true;
    }
    return false;
}

static void stderr_print_tab() {
    for (int i = 0; i < TAB_COUNT; i++) {
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
                column_new += TAB_COUNT - 1;
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
                ANSI_FBOLD
                "%s:%lu:%lu: "
                ANSI_RESET,
                srcfile->handle->path,
                line, 
                column_new);
    }

    switch (kind) {
        case MSG_KIND_ROOT_ERROR:
        case MSG_KIND_ERROR:
            fprintf(stderr, ANSI_FERROR_COLOR "error: " ANSI_RESET);
            g_error_count++;
            break;
        case MSG_KIND_WARNING:
            fprintf(stderr, ANSI_FWARNING_COLOR "warning: " ANSI_RESET);
            g_warning_count++;
            break;
        case MSG_KIND_ROOT_NOTE:
        case MSG_KIND_NOTE:
            fprintf(stderr, ANSI_FNOTE_COLOR "note: " ANSI_RESET);
            break;
    }
    vfprintf(stderr, fmt, ap);

    fprintf(stderr, "\n");
    if (!is_root_msg(kind)) {
        char* src_line_to_print = src_line;
        char* beg_of_src_line = src_line;
        int indent = fprintf(stderr, "%6lu | ", line) - 2;

        char* color = ANSI_RESET;
        switch (kind) {
            case MSG_KIND_ERROR:
                color = ANSI_FERROR_COLOR;
                break;
            case MSG_KIND_WARNING:
                color = ANSI_FWARNING_COLOR;
                break;
            case MSG_KIND_NOTE:
                color = ANSI_FNOTE_COLOR;
                break;
            default:
                assert(0);
        }

        while (*src_line_to_print != '\n' && 
               *src_line_to_print != '\0') {
            if ((size_t)(src_line_to_print - beg_of_src_line) == 
                    (column - 1)) {
                fprintf(stderr, color);
            } 
            if (*src_line_to_print == '\t') 
                stderr_print_tab();
            else
                fprintf(stderr, "%c", *src_line_to_print);
            src_line_to_print++;
            if ((size_t)(src_line_to_print - beg_of_src_line) == 
                (column + char_count - 1)) {
                fprintf(stderr, ANSI_RESET);
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
        fprintf(stderr, color);
        for (size_t c = 0; c < char_count_new; c++) {
            fprintf(stderr, "^");
        }
        fprintf(stderr, ANSI_RESET "\n");
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
            "%s%lu error(s)%s, %s%lu warning(s)%s; aborting compilation",
            g_error_count > 0 ? ANSI_FERROR_COLOR : "",
            g_error_count, 
            ANSI_RESET,
            g_warning_count > 0 ? ANSI_FWARNING_COLOR : "",
            g_warning_count,
            ANSI_RESET);
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

void _error(
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

/* template<typename T, typename... Args> */
/* void warning( */
/*         Token* token, */
/*         T first, */
/*         Args... args) { */
/*     default_msg( */
/*             MSG_KIND_WARNING, */
/*             token, */
/*             first, */
/*             args...); */
/* } */

/* template<typename T, typename... Args> */
/* void note( */
/*         Token* token, */
/*         T first, */
/*         Args... args) { */
/*     default_msg( */
/*             MSG_KIND_NOTE, */
/*             token, */
/*             first, */
/*             args...); */
/* } */

