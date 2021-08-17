std::string keywords[] = {
    "fn",
    "let",
    "return",
};

struct Type;
struct Expr;
struct Stmt;
struct Srcfile;

enum class TokenKind {
    identifier,
    keyword,
    lparen,
    rparen,
    lbrace,
    rbrace,
    colon,
    semicolon,
    comma,
    star,
    fslash,
    eof,
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    char* start, *end;
    Srcfile* srcfile;
    size_t line, column, char_count;
}; 

enum class TypeKind {
    builtin,
    ptr,
};

enum class BuiltinTypeKind {
    u8,
    u16,
    u32,
    u64,
    usize,
    i8,
    i16,
    i32,
    i64,
    isize,
    boolean,
    void_kind,
    __len,

    not_inferred,
    none,
};

comptime_map(std::string, BuiltinTypeKind) builtin_types_map[] = {
    { "u8", BuiltinTypeKind::u8 },
    { "u16", BuiltinTypeKind::u16 },
    { "u32", BuiltinTypeKind::u32 },
    { "u64", BuiltinTypeKind::u64 },
    { "usize", BuiltinTypeKind::usize },
    { "i8", BuiltinTypeKind::i8 },
    { "i16", BuiltinTypeKind::i16 },
    { "i32", BuiltinTypeKind::i32 },
    { "i64", BuiltinTypeKind::i64 },
    { "isize", BuiltinTypeKind::isize},
    { "bool", BuiltinTypeKind::boolean },
    { "void", BuiltinTypeKind::void_kind },
};

struct {
    Type* u8;
    Type* u16;
    Type* u32;
    Type* u64;
    Type* usize;
    Type* i8;
    Type* i16;
    Type* i32;
    Type* i64;
    Type* isize;
    Type* boolean;
    Type* void_kind;
} builtin_type_placeholders;

struct BuiltinType {
    Token* identifier;
    BuiltinTypeKind kind;
};

struct PtrType {
    bool constant;
    Type* child;
};

struct Type {
    TypeKind kind;
    Token* main_token;
    union {
        BuiltinType builtin;
        PtrType ptr;
    };
};

enum class ExprKind {
    symbol,
    scoped_block,
};

/* struct StaticAccessor { */
/*     std::vector<Token*> accessors; */
/*     bool from_global_scope; */
/* }; */

struct Symbol {
    /* StaticAccessor* static_accessor; */
    Token* identifier;
};

struct ScopedBlock {
    Token* lbrace;
    std::vector<Stmt*> stmts;
    Expr* value;
};

struct Expr {
    ExprKind kind;
    Token* main_token;
    union {
        Symbol symbol;
        ScopedBlock scoped_block;
    };

    Expr(ExprKind kind, Token* main_token) 
        : kind(kind), main_token(main_token) {
    }
};

enum class StmtKind {
    function,
    variable,
    ret,
    expr_stmt,
};

struct Param {
    Token* identifier;
    Type* type;
};

struct FunctionHeader {
    Token* identifier;
    std::vector<Param*> params;
    Type* ret_type;
};

struct Function {
    FunctionHeader header;
    ScopedBlock body;
};

struct Return {
    Expr* child;
    Stmt* function;
};

struct ExprStmt {
    Expr* child;
};

struct Stmt {
    StmtKind kind;
    Token* main_token;
    union {
        Function function;
        Return ret;
        ExprStmt expr_stmt;
    };

    Stmt(StmtKind kind, Token* main_token)
        : kind(kind), main_token(main_token) {
    }
};

struct Srcfile {
    fio::File* handle;
    std::vector<Token*> tokens;
    std::vector<Stmt*> stmts;
};

std::ostream& operator<<(std::ostream& stream, const Token& token) {
    stream << "Token { " << (size_t)token.kind << ", " << token.lexeme <<
        ", " << token.line << ", " << token.column << " }";
    return stream;
}

namespace builtin_type {
    BuiltinTypeKind str_to_kind(const std::string& str) {
        for (size_t i = 0; i < (size_t)BuiltinTypeKind::__len; i++) {
            if (builtin_types_map[i].k == str) {
                return builtin_types_map[i].v;
            }
        }
        return BuiltinTypeKind::none;
    }
}

Type* builtin_type_new(
        Token* identifier,
        BuiltinTypeKind kind) {
    Type* type = new Type;
    type->kind = TypeKind::builtin;
    type->main_token = identifier;
    type->builtin.identifier = identifier;
    type->builtin.kind = kind;
    return type;
}

Type* ptr_type_new(
        Token* star,
        bool constant,
        Type* child) {
    Type* type = new Type;
    type->kind = TypeKind::ptr;
    type->main_token = star;
    type->ptr.child = child;
    return type;
}

Expr* symbol_new(Symbol symbol) {
    Expr* expr = new Expr(ExprKind::symbol, symbol.identifier);
    expr->symbol = symbol;
    return expr;
}

Expr* scoped_block_new(ScopedBlock scoped_block) {
    Expr* expr = new Expr(ExprKind::scoped_block, scoped_block.lbrace);
    expr->scoped_block = scoped_block;
    return expr;
}

Stmt* function_new(Function function) {
    Stmt* stmt = new Stmt(StmtKind::function, function.header.identifier);
    stmt->function = function;
    return stmt;
}

Stmt* return_new(Token* keyword, Return ret) {
    Stmt* stmt = new Stmt(StmtKind::ret, keyword);
    stmt->ret = ret;
    return stmt;
}

Stmt* expr_stmt_new(ExprStmt expr_stmt) {
    Stmt* stmt = new Stmt(
            StmtKind::expr_stmt, 
            (expr_stmt.child ? expr_stmt.child->main_token : nullptr));
    stmt->expr_stmt = expr_stmt;
    return stmt;
}

Type* builtin_type_new_placeholder(BuiltinTypeKind kind) {
    return builtin_type_new(nullptr, kind);
}

void init_builtin_types() {
    builtin_type_placeholders.u8 = 
        builtin_type_new_placeholder(BuiltinTypeKind::u8);
    builtin_type_placeholders.u16 = 
        builtin_type_new_placeholder(BuiltinTypeKind::u16);
    builtin_type_placeholders.u32 = 
        builtin_type_new_placeholder(BuiltinTypeKind::u32);
    builtin_type_placeholders.u64 = 
        builtin_type_new_placeholder(BuiltinTypeKind::u64);
    builtin_type_placeholders.usize = 
        builtin_type_new_placeholder(BuiltinTypeKind::usize);
    builtin_type_placeholders.i8 = 
        builtin_type_new_placeholder(BuiltinTypeKind::i8);
    builtin_type_placeholders.i16 = 
        builtin_type_new_placeholder(BuiltinTypeKind::i16);
    builtin_type_placeholders.i32 = 
        builtin_type_new_placeholder(BuiltinTypeKind::i32);
    builtin_type_placeholders.i64 = 
        builtin_type_new_placeholder(BuiltinTypeKind::i64);
    builtin_type_placeholders.isize = 
        builtin_type_new_placeholder(BuiltinTypeKind::isize);
    builtin_type_placeholders.boolean = 
        builtin_type_new_placeholder(BuiltinTypeKind::boolean);
    builtin_type_placeholders.void_kind = 
        builtin_type_new_placeholder(BuiltinTypeKind::void_kind);
}
