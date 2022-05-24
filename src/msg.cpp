enum MsgKind {
    MSG_KIND_ROOT_ERROR,
    MSG_KIND_ROOT_NOTE,
    MSG_KIND_ERROR,
    MSG_KIND_WARNING,
    MSG_KIND_NOTE,
    MSG_KIND_ADDINFO,
};

static size_t g_error_count;
static size_t g_warning_count;
static bool g_first_msg = true;

static bool is_root_msg(MsgKind kind) {
    if (kind == MSG_KIND_ROOT_ERROR || 
        kind == MSG_KIND_ROOT_NOTE) {
        return true;
    }
    return false;
}

static void stderr_print_tab() {
    fputs("\x20\x20\x20\x20", stderr);
}

static size_t g_barindent;
static const std::string* last_error_file_path;

template <typename... Args>
void addinfo(
        const std::string& fmt,
        Args... args) {
    for (size_t c = 0; c < g_barindent; c++) {
        fmt::print(stderr, " ");
    }
    fmt::print(stderr, "{}>{} ", g_error_color, g_reset_color);
    fmt::print(stderr, fmt, args...);
    fmt::print(stderr, "\n");
}

template <typename... Args>
void default_msg(
        MsgKind kind, 
        Srcfile* srcfile, 
        size_t line, 
        size_t col, 
        size_t ch_count, 
        const std::string& fmt,
        Args... args) {
    bool same_file_note = (kind == MSG_KIND_NOTE && (*last_error_file_path) == srcfile->handle->path);
    if (!same_file_note && kind != MSG_KIND_ADDINFO) {
        if (g_first_msg) g_first_msg = false;
        else fmt::print(stderr, "\n");
    }
    if (kind == MSG_KIND_ADDINFO) {
        addinfo(fmt, args...);
        return;
    }

    bool root_msg = is_root_msg(kind);
    if (!root_msg) {
        assert(line > 0);
        assert(col > 0);
        assert(ch_count > 0);
    }

    char* sline = null; // srcline
    size_t slinelen = 0;
    size_t col_new = col;
    size_t ch_count_new = ch_count;
    if (!root_msg) {
        sline = get_line_from_file(srcfile->handle, line);
        char* c = sline;
        while (*c != '\n' && *c != '\0') {
            if (*c == '\t' && (size_t)(c - sline) < col) {
                col_new += 3; // tabsize-1 = 4-1 = 3
            }
            c++;
            slinelen++;
        }
        if (slinelen - col_new + 1 < ch_count) {
            ch_count_new = slinelen - col_new + 1;
        }
    }

    if (!root_msg) {
        if (same_file_note) {
            assert(g_barindent >= 5);
            for (size_t c = 0; c < g_barindent-2; c++) {
                fmt::print(stderr, " ");
            }
        }
        else {
            fmt::print(
                    stderr,
                    "{}{}:{}:{}: {}",
                    g_bold_color,
                    srcfile->handle->path,
                    line, 
                    col_new,
                    g_reset_color);
        }
    }

    switch (kind) {
        case MSG_KIND_ROOT_ERROR:
        case MSG_KIND_ERROR:
            fmt::print(stderr, "{}error: {}", g_error_color, g_reset_color);
            if (kind == MSG_KIND_ERROR)
                last_error_file_path = &srcfile->handle->path;
            g_error_count++;
            break;
        case MSG_KIND_WARNING:
            fmt::print(stderr, "{}warning: {}", g_warning_color, g_reset_color);
            g_warning_count++;
            break;
        case MSG_KIND_ROOT_NOTE:
            fmt::print(stderr, "{}note: {}", g_note_color, g_reset_color);
            break;
        case MSG_KIND_NOTE: {
            // TODO: when compiler reaches multiple files support,
            // test this
            std::string str;
            if (!same_file_note) str = "{}note: {}...";
            else str = "{}note: {}";
            fmt::print(stderr, str, g_note_color, g_reset_color);
        } break;
    }
    fmt::print(stderr, fmt, args...);
    fmt::print(stderr, "\n");

    if (!root_msg) {
        char* sline_to_print = sline;
        char* beg_of_sline = sline;
        
        // TODO: check if this works
        std::string lineno_fmt = "{:>6} | ";
        fmt::print(stderr, lineno_fmt, line);
        g_barindent = fmt::formatted_size(lineno_fmt, line) - 2;
        /* int g_barindent = fprintf(stderr, "%6lu | ", line) - 2; */

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

        while (*sline_to_print != '\n' && 
               *sline_to_print != '\0') {
            if ((size_t)(sline_to_print - beg_of_sline) == 
                    (col - 1)) {
                fmt::print(stderr, "{}", color);
            } 
            if (*sline_to_print == '\t') 
                stderr_print_tab();
            else
                fmt::print(stderr, "{}", *sline_to_print);
            sline_to_print++;
            if ((size_t)(sline_to_print - beg_of_sline) == 
                (col + ch_count - 1)) {
                fmt::print(stderr, "{}", g_reset_color);
            }
        }

        fmt::print(stderr, "\n");
        for (size_t c = 0; c < g_barindent; c++) {
            fmt::print(stderr, " ");
        }
        fmt::print(stderr, "| ");
        for (size_t c = 0; c < col_new - 1; c++) {
            fmt::print(stderr, " ");
        }
        fmt::print(stderr, "{}", color);
        for (size_t c = 0; c < ch_count_new; c++) {
            fmt::print(stderr, "^");
        }
        fmt::print(stderr, "{}\n", g_reset_color);
    }
}

// TODO: find out where this should go...
void terminate_compilation() {
    default_msg(
            MSG_KIND_ROOT_NOTE, 
            null, 
            0, 
            0, 
            0, 
            "{}{} error(s){}, {}{} warning(s){}; aborting compilation",
            g_error_count > 0 ? g_error_color : "",
            g_error_count, 
            g_reset_color,
            g_warning_count > 0 ? g_warning_color : "",
            g_warning_count,
            g_reset_color);
    // TODO: don't exit with error code `g_error_count`.
    // instead make it fixed, and when the user needs the error_count
    // they can get it using a command line switch.
    std::exit(g_error_count);
}

template <typename... Args>
void default_msg_from_tok(
        MsgKind kind,
        Token* token,
        const std::string& fmt,
        Args... args) {
    default_msg(
            kind,
            token->srcfile,
            token->line,
            token->col,
            token->ch_count,
            fmt, 
            args...);
}

template <typename... Args>
void error(
        Token* token,
        const std::string& fmt,
        Args... args) {
    default_msg(
            MSG_KIND_ERROR,
            token->srcfile,
            token->line,
            token->col,
            token->ch_count,
            fmt, 
            args...);
}

template <typename... Args>
void fatal_error(
        Token* token,
        const std::string& fmt,
        Args... args) {
    default_msg(
            MSG_KIND_ERROR,
            token->srcfile,
            token->line,
            token->col,
            token->ch_count,
            fmt,
            args...);
    terminate_compilation();
}

template <typename... Args>
void warning(
        Token* token,
        const std::string& fmt,
        Args... args) {
    default_msg(
            MSG_KIND_WARNING,
            token->srcfile,
            token->line,
            token->col,
            token->ch_count,
            fmt, 
            args...);
}

template <typename... Args>
void note(
        Token* token,
        const std::string& fmt,
        Args... args) {
    default_msg(
            MSG_KIND_NOTE,
            token->srcfile,
            token->line,
            token->col,
            token->ch_count,
            fmt, 
            args...);
}

