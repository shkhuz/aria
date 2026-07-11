#include "msg.h"
#include "core.h"
#include "compile.h"

Msg _msg_with_span(MsgKind kind, const char* msg, Span span, Srcfile* src) {
    Msg m;
    m.kind = kind;
    m.msg = msg;
    m.span = span;
    m.src = src;
    m.addl_fat = NULL;
    m.addl_thin = NULL;
    return m;
}

Msg msg_with_no_span(MsgKind kind, const char* msg) {
    Msg m;
    m.kind = kind;
    m.msg = msg;
    m.span = (Span){};
    m.src = NULL;
    m.addl_fat = NULL;
    m.addl_thin = NULL;
    return m;
}

void _msg_addl_fat(Msg* m, const char* msg, Span span, Srcfile* src) {
    bufpush(m->addl_fat, (SubMsgFat){ msg, span, src });
}

void msg_addl_thin(Msg* m, const char* msg) {
    bufpush(m->addl_thin, (SubMsgThin){ msg });
}

// NOTE: If this function is modified, then change
// `print_source_line()` too.
SrcLoc compute_srcloc_from_span(Span span, Srcfile* src) {
    const char* contents = src->handle.contents;
    int c = span.start;
    while (c != 0 && contents[c-1] != '\n') c--;

    int col = span.start - c + 1;
    int line = 1;

    // If this is too computationally expensive, we could try embedding the
    // line number into the token itself, which would increase the memory
    // consumption by 8 bytes/token though.
    while (c > 0) {
        c--;
        if (contents[c] == '\n') line++;
    }
    return (SrcLoc){ .line = line, .col = col };
}

// NOTE: If this function is modified, then change
// `compute_srcloc_from_span()` too.
static void print_source_line(
    Span span, 
    Srcfile* src, 
    const char* color, 
    bool print_srcloc
) {
    File* handle = &src->handle;
    const char* contents = handle->contents;
    int beg_of_line, c = span.start;
    int disp_col = 0;
    while (c != 0 && contents[c-1] != '\n') {
        c--;
        if (contents[c] == '\t') disp_col += 3;
    }
    beg_of_line = c;

    int col = span.start - beg_of_line + 1;
    disp_col += col;
    int line = 1;

    // If this is too computationally expensive, we could try embedding the
    // line number into the token itself, which would increase the memory
    // consumption by 8 bytes/token though.
    while (c > 0) {
        c--;
        if (contents[c] == '\n') line++;
    }

    if (print_srcloc) {
        fprintf(
            stderr,
            "  %s--> %s:%d:%d%s\n",
            g_bold_grey_color,
            handle->path,
            line,
            col,
            g_reset_color);
    }

    int indent = snprintf(NULL, 0, " %d", line);
    fprintf(stderr, "%*s %s|%s", indent, "", g_bold_grey_color, g_reset_color);
    fprintf(stderr, "\n%s%*d |%s ", g_bold_grey_color, indent, line, g_reset_color);
    bool multiline_span = false;
    int disp_chcount = 0;

    int end_of_span = span.end;
    for (int i = beg_of_line; i < span.end; i++) {
        if ((contents[i] == '\n' || contents[i] == '\0')
            && span.end-span.start != 1) {
            multiline_span = true;
            while (isspace(contents[i])) i--;
            // Now the point is on the last non-whitespace char.
            // We advance it by one because the end is exclusive.
            i++;
            end_of_span = i;
            break;
        } else if (contents[i] == '\t') {
            if (i >= span.start) disp_chcount += 3;
            fprintf(stderr, "\x20\x20\x20\x20");
        } else {
            fprintf(stderr, "%c", contents[i]);
        }
    }

    disp_chcount += end_of_span - span.start;
    for (int i = end_of_span;
         contents[i] != '\n' && contents[i] != '\0' && i < (int)handle->len;
         i++) {
        fprintf(stderr, "%c", contents[i]);
    }

    fprintf(stderr, "\n%*s %s|%s ", indent, "", g_bold_grey_color, g_reset_color);

    for (int i = 1; i < disp_col; i++) fprintf(stderr, " ");
    fprintf(stderr, "%s", color);
    for (int i = disp_col; i < disp_col + disp_chcount; i++)
        fprintf(stderr, "^");
    if (multiline_span) fprintf(stderr, " ...");
    fprintf(stderr, "%s", g_reset_color);
    fprintf(stderr, "\n");
}

void _msg_emit_no_register(Msg* msg, CompileCtx* compile_ctx) {
    if (!compile_ctx->print_msg_to_stderr) return;

    if (compile_ctx->did_msg) fprintf(stderr, "\n");
    compile_ctx->did_msg = true;

    const char* color = "";
    const char* msg_color = g_bold_cornflower_blue_color;
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

    if (msg->src) {
        print_source_line(msg->span, msg->src, color, true);
    }

    bufloop(msg->addl_fat, i) {
        fprintf(
            stderr,
            "  %snote:%s %s\n",
            g_note_color,
            g_reset_color,
            msg->addl_fat[i].msg);
        print_source_line(
            msg->addl_fat[i].span,
            msg->addl_fat[i].src,
            g_note_color,
            true/*msg->span.span.srcfile != msg->addl_fat[i].span.srcfile*/);
    }

    bufloop(msg->addl_thin, i) {
        fprintf(
            stderr,
            "  %snote:%s %s\n",
            g_note_color,
            g_reset_color,
            msg->addl_thin[i].msg);
    }
}

void _msg_emit(Msg* msg, CompileCtx* compile_ctx) {
    compile_register_msg(compile_ctx, *msg);
    _msg_emit_no_register(msg, compile_ctx);
}
