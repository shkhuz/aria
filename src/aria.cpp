std::string keywords[] = {
    "fn",
    "let",
    "mut",
    "const",
    "return",
};

struct Type;
struct Expr;
struct Stmt;
struct Srcfile;

enum class TokenKind {
    identifier,
    keyword,
    number,
    lparen,
    rparen,
    lbrace,
    rbrace,
    colon,
    double_colon,
    semicolon,
    comma,
    plus, 
    minus,
    star,
    fslash,
    equal,
    eof,
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    char* start, *end;
    Srcfile* srcfile;
    size_t line, column, char_count;
    union {
        struct {
            bigint* val;
        } number;
    };

    static bool is_lexeme_eq(Token* a, Token* b) {
        if (a->lexeme == b->lexeme) return true;
        return false;
    }
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
    Type** u8;
    Type** u16;
    Type** u32;
    Type** u64;
    Type** usize;
    Type** i8;
    Type** i16;
    Type** i32;
    Type** i64;
    Type** isize;
    Type** boolean;
    Type** void_kind;
} builtin_type_placeholders;

struct BuiltinType {
    Token* identifier;
    BuiltinTypeKind kind;
};

struct PtrType {
    bool constant;
    Type** child;
};

struct Type {
    TypeKind kind;
    Token* main_token;
    union {
        BuiltinType builtin;
        PtrType ptr;
    };

    Type(TypeKind kind, Token* main_token)
        : kind(kind), main_token(main_token) {
    }

    Type** get_child() {
        switch (this->kind) {
            case TypeKind::builtin:
                return nullptr;

            case TypeKind::ptr:
                return ptr.child;

            default:
                assert(0); 
                return nullptr;
        }
        return nullptr;
    }
};

enum class ExprKind {
    symbol,
    scoped_block,
    binop,
};

struct StaticAccessor {
    std::vector<Token*> accessors;
    bool from_global_scope;
    Token* head;
};

struct Symbol {
    StaticAccessor static_accessor;
    Token* identifier;
    Stmt* ref;
};

struct ScopedBlock {
    Token* lbrace;
    std::vector<Stmt*> stmts;
    Expr* value;
};

struct BinaryOp {
    Expr* left, *right;
    Token* op;
};

struct Expr {
    ExprKind kind;
    Token* main_token;
    union {
        Symbol symbol;
        ScopedBlock scoped_block;
        BinaryOp binop;
    };

    Expr(ExprKind kind, Token* main_token) 
        : kind(kind), main_token(main_token) {
    }
};

enum class StmtKind {
    function,
    variable,
    param,
    ret,
    expr_stmt,
};

struct FunctionHeader {
    Token* identifier;
    std::vector<Stmt*> params;
    Type** ret_type;
};

struct Function {
    FunctionHeader header;
    Expr* body;
};

struct Variable {
    bool constant;
    Token* identifier;
    Type** type;
    Expr* initializer;
};

struct Param {
    Token* identifier;
    Type** type;
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
        Variable variable;
        Param param;
        Return ret;
        ExprStmt expr_stmt;
    };

    Stmt(StmtKind kind, Token* main_token)
        : kind(kind), main_token(main_token) {
    }

    static Type** get_type(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::variable: {
                return stmt->variable.type;
            } break;

            case StmtKind::param: {
                return stmt->param.type;
            } break;

            case StmtKind::function: {
                return stmt->function.header.ret_type;
            } break;

            default: {
                assert(0);
            } break;
        }
        return nullptr;
    }
};

struct Srcfile {
    fio::File* handle;
    std::vector<Token*> tokens;
    std::vector<Stmt*> stmts;
};

std::ostream& operator<<(std::ostream& stream, const Token& token) {
    /* stream << "Token { " << (size_t)token.kind << ", " << token.lexeme << */
    /*     ", " << token.line << ", " << token.column << " }"; */
    stream << token.lexeme;
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Type& type) {
    switch (type.kind) {
        case TypeKind::builtin: {
            stream << *type.builtin.identifier;
        } break;

        case TypeKind::ptr: {
            stream << '*';
            if (type.ptr.constant) {
                stream << "const ";
            }
            stream << **type.ptr.child;
        } break;

        default: {
            assert(0);
        } break;
    }
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

    bool is_integer(BuiltinTypeKind kind) {
        switch (kind) {
            case BuiltinTypeKind::u8:
            case BuiltinTypeKind::u16:
            case BuiltinTypeKind::u32:
            case BuiltinTypeKind::u64:
            case BuiltinTypeKind::usize:
            case BuiltinTypeKind::i8:
            case BuiltinTypeKind::i16:
            case BuiltinTypeKind::i32:
            case BuiltinTypeKind::i64:
            case BuiltinTypeKind::isize:
                return true;
            
            case BuiltinTypeKind::boolean:
            case BuiltinTypeKind::void_kind:
                return false;

            case BuiltinTypeKind::none:
                assert(0);
                return false;
        }
        return false;
    }

    size_t bytes(BuiltinType* type) {
        switch (type->kind) {
            case BuiltinTypeKind::u8:
            case BuiltinTypeKind::i8:
                return 1;

            case BuiltinTypeKind::u16:
            case BuiltinTypeKind::i16:
                return 2;

            case BuiltinTypeKind::u32:
            case BuiltinTypeKind::i32:
                return 4;

            case BuiltinTypeKind::u64:
            case BuiltinTypeKind::i64:
            case BuiltinTypeKind::usize:
            case BuiltinTypeKind::isize:
            case BuiltinTypeKind::boolean:
                return 8;

            case BuiltinTypeKind::void_kind:
                return 0;

            case BuiltinTypeKind::none:
                assert(0);
                return 0;
        }
        return 0;
    }

    bool is_signed(BuiltinTypeKind kind) {
        switch (kind) {
            case BuiltinTypeKind::u8:
            case BuiltinTypeKind::u16:
            case BuiltinTypeKind::u32:
            case BuiltinTypeKind::u64:
            case BuiltinTypeKind::usize:
                return false;

            case BuiltinTypeKind::i8:
            case BuiltinTypeKind::i16:
            case BuiltinTypeKind::i32:
            case BuiltinTypeKind::i64:
            case BuiltinTypeKind::isize:
                return true;

            case BuiltinTypeKind::boolean:
            case BuiltinTypeKind::void_kind:
            case BuiltinTypeKind::none:
                assert(0);
                return false;
        }
        return false;
    }
}

Type** builtin_type_new(
        Token* identifier,
        BuiltinTypeKind kind) {
    Type* type = new Type(TypeKind::builtin, identifier);
    type->builtin.identifier = identifier;
    type->builtin.kind = kind;
    Type** t = (Type**)malloc(sizeof(t));
    *t = type;
    return t;
}

Type** ptr_type_new(
        Token* star,
        bool constant,
        Type** child) {
    Type* type = new Type(TypeKind::ptr, star);
    type->ptr.constant = constant;
    type->ptr.child = child;
    Type** t = (Type**)malloc(sizeof(t));
    *t = type;
    return t;
}

Expr* symbol_new(
        StaticAccessor static_accessor,
        Token* identifier) {
    Expr* expr = new Expr(ExprKind::symbol, 
            static_accessor.head ? static_accessor.head : identifier);
    expr->symbol.static_accessor = static_accessor;
    expr->symbol.identifier = identifier;
    expr->symbol.ref = nullptr;
    return expr;
}

Expr* scoped_block_new(
        Token* lbrace,
        std::vector<Stmt*> stmts,
        Expr* value) {
    Expr* expr = new Expr(ExprKind::scoped_block, lbrace);
    expr->scoped_block.lbrace = lbrace;
    expr->scoped_block.stmts = stmts;
    expr->scoped_block.value = value;
    return expr;
}

Expr* binop_new(
        Expr* left, 
        Expr* right,
        Token* op) {
    Expr* expr = new Expr(ExprKind::binop, op);
    expr->binop.left = left;
    expr->binop.right = right;
    expr->binop.op = op;
    return expr;
}

Stmt* function_new(
        FunctionHeader header,
        Expr* body) {
    Stmt* stmt = new Stmt(StmtKind::function, header.identifier);
    stmt->function.header = header;
    stmt->function.body = body;
    return stmt;
}

Stmt* variable_new(
        bool constant,
        Token* identifier,
        Type** type,
        Expr* initializer) {
    Stmt* stmt = new Stmt(StmtKind::variable, identifier);
    stmt->variable.constant = constant;
    stmt->variable.identifier = identifier;
    stmt->variable.type = type;
    stmt->variable.initializer = initializer;
    return stmt;
}

Stmt* param_new(
        Token* identifier,
        Type** type) {
    Stmt* stmt = new Stmt(StmtKind::param, identifier);
    stmt->param.identifier = identifier;
    stmt->param.type = type;
    return stmt;
}

Stmt* return_new(
        Token* keyword, 
        Expr* child) {
    Stmt* stmt = new Stmt(StmtKind::ret, keyword);
    stmt->ret.child = child;
    stmt->ret.function = nullptr;
    return stmt;
}

Stmt* expr_stmt_new(Expr* child) {
    Stmt* stmt = new Stmt(
            StmtKind::expr_stmt, 
            (child ? child->main_token : nullptr));
    stmt->expr_stmt.child = child;
    return stmt;
}

Type** builtin_type_new_placeholder(BuiltinTypeKind kind) {
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
