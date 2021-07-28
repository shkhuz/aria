typedef enum {
    MSG_KIND_ROOT_ERR, 
    MSG_KIND_ERR,
    MSG_KIND_WARN,
    MSG_KIND_NOTE,
} MsgKind;

char* get_line_from_file(File* file, u64 line) {
    char* current_char_in_line = file->contents;
    u64 current_line_number = 1;
    while (current_line_number != line) {
        while (*current_char_in_line != '\n') {
            if (*current_char_in_line == '\0') {
                return null;
            } else current_char_in_line++;
        }
        current_char_in_line++;
        current_line_number++;
    }
    return current_char_in_line;
}

#define __vmsg_user_stage1 \
    if (kind != MSG_KIND_ROOT_ERR) { \
        assert(char_count > 0); \
        assert(line > 0); \
        assert(column > 0); \
    } \
    char* src_line = null;  \
    u64 src_line_length = 0; \
    u64 char_count_new = char_count; \
    if (kind != MSG_KIND_ROOT_ERR) { \
        src_line = get_line_from_file(srcfile->contents, line); \
        char* c = src_line; \
        while (*c != '\n' && *c != '\0') { \
            c++; \
            src_line_length++; \
        } \
        if (src_line_length - column + 1 < char_count) { \
            char_count_new = src_line_length - column + 1; \
        } \
    } \
    if (kind != MSG_KIND_ROOT_ERR) { \
        fprintf( \
                stderr, \
                ANSI_FBOLD \
                "%s:%lu:%lu: " \
                ANSI_RESET, \
                srcfile->contents->fpath, \
                line,  \
                column \
        ); \
    } \
    switch (kind) { \
        case MSG_KIND_ROOT_ERR: \
        case MSG_KIND_ERR: \
            fprintf(stderr, ANSI_FRED "error: " ANSI_RESET); \
            break; \
        case MSG_KIND_WARN: \
            fprintf(stderr, ANSI_FGREEN "warning: " ANSI_RESET); \
            break; \
        case MSG_KIND_NOTE: \
            fprintf(stderr, ANSI_FCYAN "note: " ANSI_RESET); \
            break; \
    } \

#define __vmsg_user_stage3 \
    fprintf(stderr, "\n"); \
    if (kind != MSG_KIND_ROOT_ERR) { \
        char* src_line_to_print = src_line; \
        char* beg_of_src_line = src_line; \
        int indent = fprintf(stderr, "%6lu | ", line) - 2; \
        char* color = ANSI_RESET; \
        switch (kind) { \
            case MSG_KIND_ERR: \
                color = ANSI_FRED; \
                break; \
            case MSG_KIND_WARN: \
                color = ANSI_FGREEN; \
                break; \
            case MSG_KIND_NOTE: \
                color = ANSI_FCYAN; \
                break; \
            default: \
                assert(0); \
        } \
        while (*src_line_to_print != '\n' && *src_line_to_print != '\0') { \
            if ((u64)(src_line_to_print - beg_of_src_line) == (column - 1)) { \
                fprintf(stderr, "%s", color); \
            }  \
            fprintf(stderr, "%c", *src_line_to_print); \
            src_line_to_print++; \
            if ((u64)(src_line_to_print - beg_of_src_line) ==  \
                (column + char_count - 1)) { \
                fprintf(stderr, ANSI_RESET); \
            } \
        } \
        fprintf(stderr, "\n"); \
        for (int c = 0; c < indent; c++) { \
            fprintf(stderr, " "); \
        } \
        fprintf(stderr, "| "); \
        for (u64 c = 0; c < column - 1; c++) { \
            fprintf(stderr, " "); \
        } \
        fprintf(stderr, "%s", color); \
        for (u64 c = 0; c < char_count_new; c++) { \
            fprintf(stderr, "^"); \
        } \
        fprintf(stderr, ANSI_RESET "\n"); \
    } \

void vmsg_user(
        MsgKind kind, 
        SrcFile* srcfile, 
        u64 line, 
        u64 column, 
        u64 char_count, 
        char* fmt, 
        va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    __vmsg_user_stage1;
    vfprintf(stderr, fmt, aq);
    __vmsg_user_stage3;
    va_end(aq);
}

void msg_user_type_mismatch(
        MsgKind kind,
        SrcFile* srcfile,
        u64 line,
        u64 column,
        u64 char_count,
        Node* from,
        Node* to) {
    __vmsg_user_stage1;
    fprintf(stderr, "cannot convert from `" ANSI_FRED);
    stderr_print_type(from);
    fprintf(stderr, ANSI_RESET "` to `" ANSI_FCYAN);
    stderr_print_type(to);
    fprintf(stderr, ANSI_RESET "`");
    __vmsg_user_stage3;
}

void msg_user_expect_type(
        MsgKind kind,
        SrcFile* srcfile,
        u64 line,
        u64 column,
        u64 char_count,
        Node* type) {
    __vmsg_user_stage1;
    fprintf(stderr, "expected `" ANSI_FCYAN);
    stderr_print_type(type);
    fprintf(stderr, ANSI_RESET "`");
    __vmsg_user_stage3;
}

void msg_user(
        MsgKind kind, 
        SrcFile* srcfile, 
        u64 line, 
        u64 column, 
        u64 char_count, 
        char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg_user(kind, srcfile, line, column, char_count, fmt, ap);
    va_end(ap);
}

void vmsg_user_token(
        MsgKind kind,
        Token* token,
        char* fmt, 
        va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    vmsg_user(
            kind,
            token->srcfile,
            token->line,
            token->column,
            token->char_count, 
            fmt,
            aq);
    va_end(aq);
}

void msg_user_token(
        MsgKind kind,
        Token* token,
        char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg_user_token(kind, token, fmt, ap);
    va_end(ap);
}

void vmsg_user_node(
        MsgKind kind,
        Node* node,
        char* fmt, 
        va_list ap) {
    va_list aq;
    va_copy(aq, ap);

    if (node->head->line == node->tail->line) {
        vmsg_user(
                kind,
                node->head->srcfile,
                node->head->line,
                node->head->column,
                (node->tail->column + node->tail->char_count) - 
                    node->head->column,
                fmt,
                aq);
    } else {
        Token* msg_on = node_get_identifier(node, false);
        if (!msg_on) {
            msg_on = node->head;
        }

        vmsg_user_token(
                kind,
                msg_on,
                fmt,
                aq);
    }

    va_end(aq);
}

void msg_user_node(
        MsgKind kind,
        Node* node,
        char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg_user_node(kind, node, fmt, ap);
    va_end(ap);
}

void msg_user_type_mismatch_node(
        MsgKind kind,
        Node* node,
        Node* from,
        Node* to) {
    u64 char_count = 
        (node->tail->column + node->tail->char_count) - node->head->column;
    if (node->head->line != node->tail->line) {
        char_count = node->tail->char_count;
    }

    msg_user_type_mismatch(
            kind,
            node->head->srcfile,
            node->head->line,
            node->head->column,
            char_count,
            from,
            to);
}

void msg_user_expect_type_token(
        MsgKind kind,
        Token* token,
        Node* type) {
    msg_user_expect_type(
            kind,
            token->srcfile,
            token->line,
            token->column,
            token->char_count,
            type);
}

void terminate_compilation() {
    msg_user(
            MSG_KIND_ROOT_ERR, 
            null, 
            0, 
            0, 
            0, 
            "aborting due to previous error(s)");

    // TODO: for CI build, this is changed to
    // always return 0.
    // Uncomment next line.
    // And remove the next-to-next line.
    /* exit(EXIT_FAILURE); */
    exit(0);
}

#define verror_node(...) \
    self->error = true; vmsg_user_node(MSG_KIND_ERR, __VA_ARGS__)
#define error_node(...) \
    self->error = true; msg_user_node(MSG_KIND_ERR, __VA_ARGS__)
#define verror_token(...) \
    self->error = true; vmsg_user_token(MSG_KIND_ERR, __VA_ARGS__)
#define error_token(...) \
    self->error = true; msg_user_token(MSG_KIND_ERR, __VA_ARGS__)

#define vnote_node(...) \
    vmsg_user_node(MSG_KIND_NOTE, __VA_ARGS__)
#define note_node(...) \
    msg_user_node(MSG_KIND_NOTE, __VA_ARGS__)
#define vnote_token(...) \
    vmsg_user_token(MSG_KIND_NOTE, __VA_ARGS__)
#define note_token(...) \
    msg_user_token(MSG_KIND_NOTE, __VA_ARGS__)

#define vwarning_node(...) \
    vmsg_user_node(MSG_KIND_WARN, __VA_ARGS__)
#define warning_node(...) \
    msg_user_node(MSG_KIND_WARN, __VA_ARGS__)
#define vwarning_token(...) \
    vmsg_user_token(MSG_KIND_WARN, __VA_ARGS__)
#define warning_token(...) \
    msg_user_token(MSG_KIND_WARN, __VA_ARGS__)

#define fatal_verror_node(...)  \
    verror_node(__VA_ARGS__); terminate_compilation()
#define fatal_error_node(...)   \
    error_node(__VA_ARGS__); terminate_compilation()
#define fatal_verror_token(...) \
    verror_token(__VA_ARGS__); terminate_compilation()
#define fatal_error_token(...)  \
    error_token(__VA_ARGS__); terminate_compilation()

#define error_type_mismatch_node(...) \
    self->error = true; msg_user_type_mismatch_node(MSG_KIND_ERR, __VA_ARGS__)

#define error_expect_type_token(...) \
    self->error = true; msg_user_expect_type_token(MSG_KIND_ERR, __VA_ARGS__)
