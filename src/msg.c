#include "msg.h"
#include "core.h"
#include "cmd.h"
#include "printf.h"
#include "buf.h"
#include "misc.h"

static bool did_msg = false;

Msg msg_with_span(MsgKind kind, const char* msg, Span span) {
    Msg m;
    m.kind = kind;
    m.msg = msg;
    m.span = span_some(span);
    m.addl_fat = NULL;
    m.addl_thin = NULL;
    return m;
}

Msg msg_with_no_span(MsgKind kind, const char* msg) {
    Msg m;
    m.kind = kind;
    m.msg = msg;
    m.span = span_none();
    m.addl_fat = NULL;
    m.addl_thin = NULL;
    return m;
}

void msg_addl_fat(Msg* m, const char* msg, Span span) {
    bufpush(m->addl_fat, (SubMsgFat){ msg, span });
}

void msg_addl_thin(Msg* m, const char* msg) {
    bufpush(m->addl_thin, (SubMsgThin){ msg });
}

static void print_source_line(Span span, const char* color, bool print_srcloc) {
    const char* contents = span.srcfile->handle.contents;
    usize beg_of_line, c = span.start;
    usize disp_col = 0;
    while (c != 0 && contents[c-1] != '\n') {
        c--;
        if (contents[c] == '\t') disp_col += 3;
    }
    beg_of_line = c;

    usize line = 1;
    // If this is too computationally expensive, we could try embedding the
    // line number into the token itself, which would increase the memory
    // consumption by 8 bytes/token though.
    while (c > 0) {
        c--;
        if (contents[c] == '\n') line++;
    }

    usize col = span.start - beg_of_line + 1;
    disp_col += col;
    if (print_srcloc) {
        fprintf(
            stderr,
            "  %s--> %s:%lu:%lu%s\n",
            EC_8BITCOLOR("244", "1"),
            span.srcfile->handle.path,
            line,
            col,
            g_reset_color);
    }

    int indent = fprintf(stderr, "  %lu | ", line) - 2;
    bool multiline_span = false;
    usize disp_chcount = 0;

    for (usize i = beg_of_line; i < span.end; i++) {
        if ((contents[i] == '\n' || contents[i] == '\0') && span.end-span.start != 1) {
            multiline_span = true;
            break;
        }
        else if (contents[i] == '\t') {
            if (i >= span.start) disp_chcount += 3;
            fprintf(stderr, "\x20\x20\x20\x20");
        }
        else fprintf(stderr, "%c", contents[i]);
    }
    disp_chcount += span.end - span.start;
    for (usize i = span.end; contents[i] != '\n' && contents[i] != '\0'; i++) {
        fprintf(stderr, "%c", contents[i]);
    }

    fprintf(stderr, "\n%*s", indent, "");
    fprintf(stderr, "| ");

    for (usize i = 1; i < disp_col; i++) fprintf(stderr, " ");
    fprintf(stderr, "%s", color);
    for (usize i = disp_col; i < disp_col + disp_chcount; i++)
        fprintf(stderr, "^");
    if (multiline_span) fprintf(stderr, " \u00B7\u00B7\u00B7");
    fprintf(stderr, "%s\n", g_reset_color);
}

void _msg_emit(Msg* msg) {
    if (did_msg) printf("\n");
    const char* color = "";
    const char* msg_color = g_cornflower_blue_color;
    switch (msg->kind) {
        case MSG_ERROR: {
            color = g_error_color;
            fprintf(stderr, "%serror%s: %s", color, g_bold_color, msg_color);
        } break;

        case MSG_WARNING: {
            color = g_warning_color;
            fprintf(stderr, "%swarning%s: %s", color, g_bold_color, msg_color);
        } break;

        case MSG_NOTE: {
            color = g_note_color;
            fprintf(stderr, "%snote%s: %s", color, g_bold_color, msg_color);
        } break;
    }
    fprintf(stderr, "%s", msg->msg);
    fprintf(stderr, "%s\n", g_reset_color);

    if (msg->span.exists) {
        print_source_line(msg->span.span, color, true);

        bufloop(msg->addl_fat, i) {
            fprintf(
                stderr,
                "  %s> note:%s %s\n",
                g_note_color,
                g_reset_color,
                msg->addl_fat[i].msg);
            print_source_line(
                msg->addl_fat[i].span,
                g_note_color,
                msg->span.span.srcfile != msg->addl_fat[i].span.srcfile);
        }

        bufloop(msg->addl_thin, i) {
            fprintf(
                stderr,
                "  %s= note:%s %s\n",
                g_note_color,
                g_reset_color,
                msg->addl_thin[i].msg);
        }
    }

    if (!did_msg) {
        did_msg = true;
    }
}
