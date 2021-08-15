enum class MsgKind {
    root_err, 
    root_note,
    err,
    warn,
    note,
};

size_t g_error_count = 0;
size_t g_warning_count = 0;

char* get_line_from_file(
        fio::File* handle, 
        size_t line) {
    char* char_in_line = handle->contents;
    size_t line_number = 1;
    while (line_number != line) {
        while (*char_in_line != '\n') {
            if (*char_in_line == '\0') {
                return nullptr;
            } else char_in_line++;
        }
        char_in_line++;
        line_number++;
    }
    return char_in_line;
}

bool is_root_msg(MsgKind kind) {
    if (kind == MsgKind::root_err ||
        kind == MsgKind::root_note) {
        return true;
    }
    return false;
}

template<typename T, typename... Args>
void msg_user(
        MsgKind kind, 
        Srcfile* srcfile, 
        size_t line, 
        size_t column, 
        size_t char_count, 
        T first,
        Args... args) {
    if (!is_root_msg(kind)) {
        assert(line > 0);
        assert(column > 0);
        assert(char_count > 0);
    }

    char* src_line = nullptr;
    size_t src_line_length = 0;
    size_t char_count_new = char_count;
    if (!is_root_msg(kind)) {
        src_line = get_line_from_file(srcfile->handle, line);
        char* c = src_line;
        while (*c != '\n' && *c != '\0') {
            c++;
            src_line_length++;
        }
        if (src_line_length - column + 1 < char_count) {
            char_count_new = src_line_length - column + 1;
        }
    }

    if (!is_root_msg(kind)) {
        stderr_print(
                ANSI_FBOLD,
                srcfile->handle->path,
                ":",
                line, 
                ":",
                column,
                ": ",
                ANSI_RESET
        );
    }

    switch (kind) {
        case MsgKind::root_err:
        case MsgKind::err:
            stderr_print(ANSI_FRED "error: " ANSI_RESET);
            g_error_count++;
            break;
        case MsgKind::warn:
            stderr_print(ANSI_FGREEN "warning: " ANSI_RESET);
            g_warning_count++;
            break;
        case MsgKind::root_note:
        case MsgKind::note:
            stderr_print(ANSI_FCYAN "note: " ANSI_RESET);
            break;
    }
    stderr_print(first);
    stderr_print(args...);

    stderr_print("\n");
    if (!is_root_msg(kind)) {
        auto src_line_to_print = src_line;
        auto beg_of_src_line = src_line;
        int indent = fprintf(stderr, "%6lu | ", line) - 2;

        std::string color = ANSI_RESET;
        switch (kind) {
            case MsgKind::err:
                color = ANSI_FRED;
                break;
            case MsgKind::warn:
                color = ANSI_FGREEN;
                break;
            case MsgKind::note:
                color = ANSI_FCYAN;
                break;
            default:
                assert(0);
        }

        while (*src_line_to_print != '\n' && *src_line_to_print != '\0') {
            if ((size_t)(src_line_to_print - beg_of_src_line) == (column - 1)) {
                stderr_print(color);
            } 
            stderr_print(*src_line_to_print);
            src_line_to_print++;
            if ((size_t)(src_line_to_print - beg_of_src_line) == 
                (column + char_count - 1)) {
                stderr_print(ANSI_RESET);
            }
        }

        stderr_print("\n");
        for (int c = 0; c < indent; c++) {
            stderr_print(" ");
        }
        stderr_print("| ");
        for (size_t c = 0; c < column - 1; c++) {
            stderr_print(" ");
        }
        stderr_print(color);
        for (size_t c = 0; c < char_count_new; c++) {
            stderr_print("^");
        }
        stderr_print(ANSI_RESET "\n");
    }
}

void terminate_compilation() {
    msg_user(
            MsgKind::root_note, 
            nullptr, 
            0, 
            0, 
            0, 
            g_error_count, " error(s), ",
            g_warning_count, " warning(s); aborting compilation");
    // TODO: for CI build, this is changed to
    // always return 0.
    // Uncomment next line.
    // And comment next-to-next line.
    /* exit(EXIT_FAILURE); */
    exit(0);
}

