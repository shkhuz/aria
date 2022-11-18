#include "msg.h"
#include "core.h"
#include "cmd.h"
#include "printf.h"

/*
void vmsg(
        MsgKind kind,
        Srcfile* srcfile,
        usize line,
        usize col,
        usize ch_count,
        const char* fmt,
        va_list args)
{
    bool same_file_note =
        kind == MSG_KIND_NOTE
        && last_error_file_path
        && strcmp(last_error_file_path, srcfile->handle.path) == 0;
    same_file_note = false;
    if (kind != MSG_KIND_ADDINFO && kind != MSG_KIND_NOTE) {
        if (g_first_main_msg) g_first_main_msg = false;
        else aria_fprintf(stderr, "\n");
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

    usize disp_slinelen = slinelen;
    usize disp_col = col;
    usize disp_ch_count = ch_count;
    bool multiline_ch_count = false;

    if (!root_msg) {
        sline = file_get_line_ptr(&srcfile->handle, line);
        const char* c = sline;
        while (*c != '\n' && *c != '\0') {
            if (*c == '\t') {
                if ((usize)(c - sline) < col) disp_col += 3; // tabsize-1 = 4-1 = 3
                else disp_ch_count += 3;
                disp_slinelen += 3;
            }
            c++;
            slinelen++;
            disp_slinelen++;
        }
        if (disp_ch_count > disp_slinelen - disp_col + 1) {
            multiline_ch_count = true;
            disp_ch_count = disp_slinelen - disp_col + 1;
        }

        if (same_file_note) {
            // TODO: test this
            aria_fprintf(stderr, " %*s", g_linenumwidth + 1, " ");
            aria_fprintf(stderr, "= ");
        }
    }

    switch (kind) {
        case MSG_KIND_ROOT_ERROR: {
            aria_fprintf(stderr, "error: %s", g_error_color);
            last_error_file_path = srcfile->handle.path;
            g_error_count++;
        } break;
            
        case MSG_KIND_ERROR: {
            aria_fprintf(stderr, "error: ");
            last_error_file_path = srcfile->handle.path;
            g_error_count++;
        } break;

        case MSG_KIND_ROOT_WARNING: {
            aria_fprintf(stderr, "warn: %s", g_warning_color);
            g_warning_count++;
        } break;
            
        case MSG_KIND_WARNING: {
            aria_fprintf(stderr, "warn: ");
            g_warning_count++;
        } break;

        case MSG_KIND_ROOT_NOTE: {
            aria_fprintf(stderr, "note: %s", g_note_color);
        } break;

        case MSG_KIND_NOTE: {
            aria_fprintf(stderr, "note: ");
        } break;
    }

    if (!root_msg && !same_file_note) {
        const char* color = "";
        switch (kind) {
            case MSG_KIND_ERROR: color = g_error_color; break;
            case MSG_KIND_WARNING: color = g_warning_color; break;
        }

        // TODO: add `...` before a note to show that it is a different file note.
        aria_fprintf(
            stderr,
            "(%s%s%s):%lu:%lu: %s",
            (kind == MSG_KIND_ERROR || kind == MSG_KIND_WARNING) ? g_cornflower_blue_color : g_bold_color,
            srcfile->handle.path,
            g_reset_color,
            line,
            disp_col,
            color);
    }

    aria_vfprintf(stderr, fmt, args);
    aria_fprintf(stderr, "%s\n", g_reset_color);

    if (!root_msg) {
        const char* sline_to_print = sline;
        const char* beg_of_sline = sline;

        int digitsinlinenum = 0;
        usize tmpline = line;
        for (digitsinlinenum = 0; tmpline > 0; digitsinlinenum++) tmpline /= 10;

        int linenumwidth = digitsinlinenum;
        g_linenumwidth = linenumwidth;
        aria_fprintf(stderr, " %*lu | ", linenumwidth, line);
        
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

        bool color_resetted = false;
        while (*sline_to_print != '\n' && *sline_to_print != '\0') {
            if ((usize)(sline_to_print - beg_of_sline) == col-1) {
                aria_fprintf(stderr, "%s", color);
            }
            if (*sline_to_print == '\t') write_tab_to_stderr();
            else aria_fprintf(stderr, "%c", *sline_to_print);
            sline_to_print++;

            if ((usize)(sline_to_print - beg_of_sline) == (col + ch_count - 1)) {
                aria_fprintf(stderr, "%s", g_reset_color);
                color_resetted = true;
            }
        }
        if (!color_resetted) aria_fprintf(stderr, "%s", g_reset_color);
        aria_fprintf(stderr, "\n");

        aria_fprintf(stderr, " %*s | ", g_linenumwidth, " ");
        for (usize c = 0; c < disp_col - 1; c++) {
            aria_fprintf(stderr, " ");
        }
        aria_fprintf(stderr, "%s", color);
        for (usize c = 0; c < disp_ch_count; c++) {
            if (kind == MSG_KIND_NOTE) aria_fprintf(stderr, "-");
            else aria_fprintf(stderr, "^");
        }
        if (multiline_ch_count) {

        }
        aria_fprintf(stderr, "%s\n", g_reset_color);
    }
}
*/

Msg msg_with_span(MsgKind kind, const char* msg, Span span) {
    Msg m;
    m.kind = kind;
    m.msg = msg;
    m.span = span_some(span);
    return m;
}

Msg msg_with_no_span(MsgKind kind, const char* msg) {
    Msg m;
    m.kind = kind;
    m.msg = msg;
    m.span = span_none();
    return m;
}

void msg_emit(Msg* msg) {
    const char* color = "";
    switch (msg->kind) {
        case MSG_KIND_ERROR: {
            color = g_error_color;
            aria_fprintf(stderr, "error: %s", color);
        } break;

        case MSG_KIND_WARNING: {
            color = g_warning_color;
            aria_fprintf(stderr, "warning: %s", color);
        } break;

        case MSG_KIND_NOTE: {
            color = g_note_color;
            aria_fprintf(stderr, "note: %s", color);
        } break;
    }
    aria_fprintf(stderr, msg->msg);
    aria_fprintf(stderr, "%s\n", g_reset_color);

    if (msg->span.exists) {
        Span span = msg->span.span;
        const char* contents = span.srcfile->handle.contents;
        
        usize beg_of_line, c = span.start;
        while (c != 0 && contents[c-1] != '\n') {
            c--;
        }
        beg_of_line = c;

        usize line = 1;
        while (c > 0) {
            c--;
            if (contents[c] == '\n') line++;
        }

        usize col = span.start - beg_of_line + 1;
        aria_fprintf(
            stderr,
            "  --> %s:%lu:%lu\n",
            span.srcfile->handle.path,
            line,
            col);
        
        int indent = aria_fprintf(stderr, "  %lu | ", line) - 2;
        usize span_end_on_same_line = span.end;
        bool multiline_span = false;
        for (usize i = beg_of_line; i < span.end; i++) {
            char ch = span.srcfile->handle.contents[i];
            // print tab as 4 spaces
            if (ch == '\n') {
                span_end_on_same_line = i;
                multiline_span = true;
                break;
            }
            aria_fprintf(stderr, "%c", ch);
        }

        aria_fprintf(stderr, "\n%*s", indent, "");
        aria_fprintf(stderr, "| ");
        for (usize i = 1; i < col; i++) aria_fprintf(stderr, " "); 
        aria_fprintf(stderr, "%s", color);
        for (usize i = span.start; i < span_end_on_same_line; i++)
            aria_fprintf(stderr, "^");
        if (multiline_span) aria_fprintf(stderr, " \u00B7\u00B7\u00B7");
        aria_fprintf(stderr, "%s\n", g_reset_color);
    }
}
