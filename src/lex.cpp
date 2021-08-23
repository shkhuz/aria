struct Lexer {
    Srcfile* srcfile;
    char* start, *current, *last_newline;
    size_t line;
    bool error;

    Lexer(Srcfile* srcfile)
        : srcfile(srcfile) {
        this->start = srcfile->handle->contents;
        this->current = this->start;
        this->last_newline = this->start;
        this->line = 1;
        this->error = false;
    }

    size_t get_column(char* c) {
        size_t column = (size_t)(c - this->last_newline);
        if (this->line == 1) column++;
        return column;
    }

    size_t get_start_column() {
        return get_column(this->start);
    }

    size_t get_current_column() {
        return get_column(this->current);
    }

    template<typename T, typename... Args>
    void __error(size_t column, size_t char_count, T first, Args... args) {
        this->error = true;
        msg::default_msg(
                msg::MsgKind::err,
                this->srcfile,
                this->line,
                column,
                char_count,
                first, 
                args...);
        msg::terminate_compilation();
    }

    template<typename T, typename... Args>
    void error_current_to_current(T first, Args... args) {
        this->__error(
                this->get_current_column(), 
                1, 
                first, 
                args...);
    }

    template<typename T, typename... Args>
    void error_start_to_current(T first, Args... args) {
        this->__error(
                this->get_start_column(), 
                (size_t)(this->current - this->start), 
                first, 
                args...);
    }

    void push_tok(TokenKind kind) {
        this->srcfile->tokens.push_back(new Token {
            kind,
            (kind == TokenKind::eof ? 
             "EOF" : 
             std::string(this->start, this->current)),
            this->start,
            this->current,
            this->srcfile,
            this->line,
            this->get_start_column(),
            (size_t)(this->current - this->start),
        });
    }

    void push_tok_adv_one(TokenKind kind) {
        this->current++;
        this->push_tok(kind);
    }

    void push_tok_adv_one_cond(
            char c, 
            TokenKind matched, 
            TokenKind not_matched) {
        if (*(this->current + 1) == c) {
            this->current++;
            this->push_tok_adv_one(matched);
        } else {
            this->push_tok_adv_one(not_matched);
        }
    }

    void run() {
        for (;;) {
            this->start = this->current;
            switch (*this->current) {
                case 'a': case 'b': case 'c': case 'd': case 'e':
                case 'f': case 'g': case 'h': case 'i': case 'j':
                case 'k': case 'l': case 'm': case 'n': case 'o':
                case 'p': case 'q': case 'r': case 's': case 't':
                case 'u': case 'v': case 'w': case 'x': case 'y':
                case 'z': case 'A': case 'B': case 'C': case 'D':
                case 'E': case 'F': case 'G': case 'H': case 'I':
                case 'J': case 'K': case 'L': case 'M': case 'N':
                case 'O': case 'P': case 'Q': case 'R': case 'S':
                case 'T': case 'U': case 'V': case 'W': case 'X':
                case 'Y': case 'Z': case '_': {
                    TokenKind kind = TokenKind::identifier;
                    while (std::isalnum(*this->current) || 
                           *this->current == '_') {
                        this->current++;
                    }

                    for (size_t i = 0; i < stack_arr_len(keywords); i++) {
                        if (keywords[i] == 
                                std::string(this->start, this->current)) {
                            kind = TokenKind::keyword;
                            break;
                        }
                    }
                    this->push_tok(kind);
                } break;

                case '(': {
                    this->push_tok_adv_one(TokenKind::lparen);
                } break;

                case ')': {
                    this->push_tok_adv_one(TokenKind::rparen);
                } break;

                case '{': {
                    this->push_tok_adv_one(TokenKind::lbrace);
                } break;

                case '}': {
                    this->push_tok_adv_one(TokenKind::rbrace);
                } break;

                case ':': {
                    this->push_tok_adv_one_cond(
                            ':', 
                            TokenKind::double_colon, 
                            TokenKind::colon);
                } break;

                case ';': {
                    this->push_tok_adv_one(TokenKind::semicolon);
                } break;

                case ',': {
                    this->push_tok_adv_one(TokenKind::comma);
                } break;

                case '*': {
                    this->push_tok_adv_one(TokenKind::star);
                } break;

                case '/': {
                    if (*(this->current + 1) == '/') {
                        while (*this->current != '\n' && 
                               *this->current != '\0') {
                            this->current++;
                        }
                    } else this->push_tok_adv_one(TokenKind::fslash);
                } break;

                case '=': {
                    this->push_tok_adv_one(TokenKind::equal);
                } break;

                case '\n': {
                    this->last_newline = this->current++;
                    this->line++;
                } break;

                case '\t': {
                    error_current_to_current(
                            "aria does not support tabs");
                    msg::terminate_compilation();
                } break;

                case ' ':
                case '\r': {
                    this->current++;
                } break;

                case '\0': {
                    this->push_tok_adv_one(TokenKind::eof);
                } return;

                default: {
                    this->current++;
                    this->error_start_to_current("invalid character");
                } break;
            }
        }
    }
};
