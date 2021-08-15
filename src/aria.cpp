enum class TokenKind {
    identifier,
    keyword,
    eof,
};

std::string keywords[] = {
    "fn",
    "let",
};

struct Srcfile;

struct Token {
    TokenKind kind;
    std::string lexeme;
    char* start, *end;
    Srcfile* srcfile;
    size_t line, column, char_count;
}; 

std::ostream& operator<<(std::ostream& stream, const Token& token) {
    stream << "Token { " << (size_t)token.kind << ", " << token.lexeme <<
        ", " << token.line << ", " << token.column << " }";
    return stream;
}

struct Srcfile {
    fio::File* handle;
    std::vector<Token*> tokens;
};
