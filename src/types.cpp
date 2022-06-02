struct Token;
struct Type;
struct Expr;
struct Stmt;

struct Srcfile {
    File* handle;
    std::vector<Token*> tokens;
    std::vector<Stmt*> stmts;
};

enum TokenKind {
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_KEYWORD,
    TOKEN_KIND_INTEGER,
    TOKEN_KIND_LBRACE,
    TOKEN_KIND_RBRACE,
    TOKEN_KIND_LPAREN,
    TOKEN_KIND_RPAREN,
    TOKEN_KIND_COLON,
    TOKEN_KIND_SEMICOLON,
    TOKEN_KIND_COMMA,
    TOKEN_KIND_EQUAL,
    TOKEN_KIND_DOUBLE_EQUAL,
    TOKEN_KIND_BANG,
    TOKEN_KIND_BANG_EQUAL,
    TOKEN_KIND_LANGBR,
    TOKEN_KIND_LANGBR_EQUAL,
    TOKEN_KIND_RANGBR,
    TOKEN_KIND_RANGBR_EQUAL,
    TOKEN_KIND_AMP,
    TOKEN_KIND_DOUBLE_AMP,
    TOKEN_KIND_PLUS,
    TOKEN_KIND_MINUS,
    TOKEN_KIND_STAR,
    TOKEN_KIND_FSLASH,
    TOKEN_KIND_EOF,
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    char* start, *end;
    Srcfile* srcfile;
    size_t line, col, ch_count;
    union {
        struct {
            bigint val;
        } integer;
    };
};

enum TypeKind {
    TYPE_KIND_BUILTIN,
    TYPE_KIND_PTR,
};

// TODO: remove usize and isize, and make them aliases of an integer type
// whose size of machine ptr size-dependent.
enum BuiltinTypeKind {
    BUILTIN_TYPE_KIND_U8,
    BUILTIN_TYPE_KIND_U16,
    BUILTIN_TYPE_KIND_U32,
    BUILTIN_TYPE_KIND_U64,
    BUILTIN_TYPE_KIND_USIZE,
    BUILTIN_TYPE_KIND_I8,
    BUILTIN_TYPE_KIND_I16,
    BUILTIN_TYPE_KIND_I32,
    BUILTIN_TYPE_KIND_I64,
    BUILTIN_TYPE_KIND_ISIZE,
    BUILTIN_TYPE_KIND_BOOLEAN,
    BUILTIN_TYPE_KIND_VOID,
    BUILTIN_TYPE_KIND_APINT,
    BUILTIN_TYPE_KIND_NONE,
    BUILTIN_TYPE_KIND__COUNT,
};

struct BuiltinTypeMap {
    std::string k;
    BuiltinTypeKind v;
};
BuiltinTypeMap builtin_type_map[BUILTIN_TYPE_KIND__COUNT] = {
    { "u8", BUILTIN_TYPE_KIND_U8 },
    { "u16", BUILTIN_TYPE_KIND_U16 },
    { "u32", BUILTIN_TYPE_KIND_U32 },
    { "u64", BUILTIN_TYPE_KIND_U64 },
    { "usize", BUILTIN_TYPE_KIND_USIZE },
    { "i8", BUILTIN_TYPE_KIND_I8 },
    { "i16", BUILTIN_TYPE_KIND_I16 },
    { "i32", BUILTIN_TYPE_KIND_I32 },
    { "i64", BUILTIN_TYPE_KIND_I64 },
    { "isize", BUILTIN_TYPE_KIND_ISIZE },
    { "bool", BUILTIN_TYPE_KIND_BOOLEAN },
    { "void", BUILTIN_TYPE_KIND_VOID },
};

struct BuiltinTypePlaceholders {
    Type* uint8;
    Type* uint16;
    Type* uint32;
    Type* uint64;
    Type* usize;
    Type* int8;
    Type* int16;
    Type* int32;
    Type* int64;
    Type* isize;
    Type* boolean;
    Type* void_kind;
    Type* void_ptr;
};
BuiltinTypePlaceholders builtin_type_placeholders;

struct BuiltinType {
    Token* token;
    BuiltinTypeKind kind;
    union {
        bigint apint;
    };
};

struct PtrType {
    bool constant;
    Type* child;
};

const size_t PTR_BYTES = 8;

struct Type {
    TypeKind kind;
    Token* main_token;
    union {
        BuiltinType builtin;
        PtrType ptr;
    };
};

enum ExprKind {
    EXPR_KIND_INTEGER,
    EXPR_KIND_CONSTANT,
    EXPR_KIND_SYMBOL,
    EXPR_KIND_FUNCTION_CALL,
    EXPR_KIND_BINOP,
    EXPR_KIND_UNOP,
    EXPR_KIND_CAST,
    EXPR_KIND_BLOCK,
    EXPR_KIND_IF,
};

struct IntegerExpr {
    Token* integer;
    bigint* val;
};

enum ConstantKind {
    CONSTANT_KIND_BOOLEAN_TRUE,
    CONSTANT_KIND_BOOLEAN_FALSE,
    CONSTANT_KIND_NULL,
};

struct ConstantExpr {
    Token* keyword;
    ConstantKind kind;
};

struct SymbolExpr {
    Token* identifier;
    Stmt* ref;
};

struct FunctionCallExpr {
    Expr* callee;
    std::vector<Expr*> args;
    Token* rparen;
};

struct BinopExpr {
    Expr* left;
    Expr* right;
    Token* op;
    Type* left_type;
    Type* right_type;
    bool folded;
};

struct UnopExpr {
    Expr* child;
    Token* op;
    Type* child_type;
    bool folded;
};

struct CastExpr {
    Expr* left;
    Type* left_type;
    Token* op;
    Type* to;
};

struct BlockExpr {
    Expr* value;
    Token* rbrace;
    std::vector<Stmt*> stmts;
};

enum IfBranchKind {
    IF_BRANCH_IF,
    IF_BRANCH_ELSEIF,
    IF_BRANCH_ELSE,
};

struct IfBranch {
    Expr* cond;
    Expr* body;
    IfBranchKind kind;
};

struct IfExpr {
    IfBranch* ifbr;
    std::vector<IfBranch*> elseifbr;
    IfBranch* elsebr;
};

struct Expr {
    ExprKind kind;
    Token* main_token;
    Type* type;
    Stmt* parent_func;
    union {
        IntegerExpr integer;
        ConstantExpr constant;
        SymbolExpr symbol;
        FunctionCallExpr function_call;
        BinopExpr binop;
        UnopExpr unop;
        CastExpr cast;
        BlockExpr block;
        IfExpr iff;
    };

    Expr() {}
};

enum StmtKind {
    STMT_KIND_FUNCTION,
    STMT_KIND_VARIABLE,
    STMT_KIND_PARAM,
    STMT_KIND_WHILE,
    STMT_KIND_ASSIGN,
    STMT_KIND_RETURN,
    STMT_KIND_EXPR,
};

struct FunctionHeader {
    Token* identifier;
    std::vector<Stmt*> params;
    Type* return_type;
    bool is_extern;
    LLVMValueRef llvmvalue;
};

struct FunctionStmt {
    FunctionHeader* header;
    Expr* body;
    std::vector<Stmt*> locals;
    size_t stack_vars_size;
    size_t ifidx;
    size_t whileidx;
};

struct VariableStmt {
    bool constant;
    bool is_extern;
    bool global;
    Token* identifier;
    Type* type;
    Type* initializer_type;
    Expr* initializer;
    // TODO: clean `stack_offset` variable
    size_t stack_offset;
    LLVMValueRef llvmvalue;
};

struct ParamStmt {
    Token* identifier;
    Type* type;
    size_t idx;
    size_t stack_offset;
    LLVMValueRef llvmvalue;
};

struct WhileStmt {
    Expr* cond;
    Expr* body;
};

struct AssignStmt {
    Expr* left;
    Expr* right;
    Type* left_type;
    Type* right_type;
    Token* op;
};

struct ReturnStmt {
    Expr* child;
    Stmt* parent_func;
};

struct ExprStmt {
    Expr* child;
};

struct Stmt {
    StmtKind kind;
    Token* main_token;
    Stmt* parent_func;
    union {
        FunctionStmt function;
        VariableStmt variable;
        ParamStmt param;
        WhileStmt whilelp;
        AssignStmt assign;
        ReturnStmt return_stmt;
        ExprStmt expr;
    };

    Stmt() {}
};

std::string aria_keywords[] = {
    "fn",
    "let",
    "const",
    "mut",
    "extern",
    "if",
    "else",
    "while",
    "true",
    "false",
    "null",
    "as",
};

bool is_token_lexeme_eq(Token* a, Token* b) {
    if (a->lexeme == b->lexeme) {
        return true;
    }
    return false;
}

BuiltinTypeKind builtin_type_str_to_kind(const std::string& str) {
    for (size_t i = 0; i < BUILTIN_TYPE_KIND__COUNT; i++) {
        if (builtin_type_map[i].k == str) {
            return builtin_type_map[i].v;
        }
    }
    return BUILTIN_TYPE_KIND_NONE;
}

std::string builtin_type_kind_to_str(BuiltinTypeKind kind) {
    if (kind == BUILTIN_TYPE_KIND_APINT) return "{integer}";
    for (size_t i = 0; i < BUILTIN_TYPE_KIND__COUNT; i++) {
        if (builtin_type_map[i].v == kind) {
            return builtin_type_map[i].k;
        }
    }
    assert(0);
    return "";
}

bool builtin_type_is_integer(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:
        case BUILTIN_TYPE_KIND_U16:
        case BUILTIN_TYPE_KIND_U32:
        case BUILTIN_TYPE_KIND_U64:
        case BUILTIN_TYPE_KIND_USIZE:
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
        case BUILTIN_TYPE_KIND_ISIZE:
            return true;
        
        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_VOID:
        case BUILTIN_TYPE_KIND_APINT:
            return false;

        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return false;
    }
    return false;
}

bool builtin_type_is_void(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:
        case BUILTIN_TYPE_KIND_U16:
        case BUILTIN_TYPE_KIND_U32:
        case BUILTIN_TYPE_KIND_U64:
        case BUILTIN_TYPE_KIND_USIZE:
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
        case BUILTIN_TYPE_KIND_ISIZE:
        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_APINT:
            return false;
        
        case BUILTIN_TYPE_KIND_VOID:
            return true;

        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return false;
    }
    return false;
}

bool builtin_type_is_apint(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:
        case BUILTIN_TYPE_KIND_U16:
        case BUILTIN_TYPE_KIND_U32:
        case BUILTIN_TYPE_KIND_U64:
        case BUILTIN_TYPE_KIND_USIZE:
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
        case BUILTIN_TYPE_KIND_ISIZE:
        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_VOID:
            return false;

        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return false;
        
        case BUILTIN_TYPE_KIND_APINT:
            return true;
    }
    return false;
}

bool builtin_type_is_signed(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:
        case BUILTIN_TYPE_KIND_U16:
        case BUILTIN_TYPE_KIND_U32:
        case BUILTIN_TYPE_KIND_U64:
        case BUILTIN_TYPE_KIND_USIZE:
        case BUILTIN_TYPE_KIND_BOOLEAN:
            return false;

        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
        case BUILTIN_TYPE_KIND_ISIZE:
            return true;
        
        case BUILTIN_TYPE_KIND_APINT:
        case BUILTIN_TYPE_KIND_VOID:
        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return false;
    }
    return false;
}

BuiltinTypeKind builtin_type_convert_to_llvm_type(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:
            return BUILTIN_TYPE_KIND_I8;
        case BUILTIN_TYPE_KIND_U16:
            return BUILTIN_TYPE_KIND_I16;
        case BUILTIN_TYPE_KIND_U32:
            return BUILTIN_TYPE_KIND_I32;
        case BUILTIN_TYPE_KIND_U64:
            return BUILTIN_TYPE_KIND_I64;
        case BUILTIN_TYPE_KIND_USIZE:
            return BUILTIN_TYPE_KIND_I64;

        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
            return kind;
        case BUILTIN_TYPE_KIND_ISIZE:
            return BUILTIN_TYPE_KIND_I64;
        
        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_APINT:
        case BUILTIN_TYPE_KIND_VOID:
        case BUILTIN_TYPE_KIND_NONE:
            break;
    }
    assert(0);
    return BUILTIN_TYPE_KIND_NONE;
}

size_t builtin_type_bytes(BuiltinType* type) {
    switch (type->kind) {
        case BUILTIN_TYPE_KIND_U8:
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_BOOLEAN:
            return 1;

        case BUILTIN_TYPE_KIND_U16:
        case BUILTIN_TYPE_KIND_I16:
            return 2;

        case BUILTIN_TYPE_KIND_U32:
        case BUILTIN_TYPE_KIND_I32:
            return 4;

        case BUILTIN_TYPE_KIND_U64:
        case BUILTIN_TYPE_KIND_I64:
        case BUILTIN_TYPE_KIND_USIZE:
        case BUILTIN_TYPE_KIND_ISIZE:
            return 8;

        case BUILTIN_TYPE_KIND_VOID:
            return 0;

        case BUILTIN_TYPE_KIND_APINT: {
            u64 n = bigint_get_lsd(&type->apint);
            size_t bits = get_bits_for_value(n);
            if (bits <= 8) bits = 8;
            else if (bits <= 16) bits = 16;
            else if (bits <= 32) bits = 32;
            else if (bits <= 64) bits = 64;
            return bits / 8;
        } break;

        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return 0;
    }
    return 0;
}

i64 builtin_type_get_min_val(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8: 
        case BUILTIN_TYPE_KIND_U16: 
        case BUILTIN_TYPE_KIND_U32: 
        case BUILTIN_TYPE_KIND_U64: 
        case BUILTIN_TYPE_KIND_USIZE: 
            return 0;
        case BUILTIN_TYPE_KIND_I8:  return INT8_MIN;
        case BUILTIN_TYPE_KIND_I16: return INT16_MIN;
        case BUILTIN_TYPE_KIND_I32: return INT32_MIN;
        case BUILTIN_TYPE_KIND_I64: return INT64_MIN;
        case BUILTIN_TYPE_KIND_ISIZE: return INT64_MIN;

        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_VOID:
        case BUILTIN_TYPE_KIND_APINT:
        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            break;
    }
    return 0;
}

u64 builtin_type_get_max_val(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:  return UINT8_MAX;
        case BUILTIN_TYPE_KIND_U16: return UINT16_MAX;
        case BUILTIN_TYPE_KIND_U32: return UINT32_MAX;
        case BUILTIN_TYPE_KIND_U64: return UINT64_MAX;
        case BUILTIN_TYPE_KIND_USIZE: return UINT64_MAX;

        case BUILTIN_TYPE_KIND_I8:  return INT8_MAX;
        case BUILTIN_TYPE_KIND_I16: return INT16_MAX;
        case BUILTIN_TYPE_KIND_I32: return INT32_MAX;
        case BUILTIN_TYPE_KIND_I64: return INT64_MAX;
        case BUILTIN_TYPE_KIND_ISIZE: return INT64_MAX;

        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_VOID:
        case BUILTIN_TYPE_KIND_APINT:
        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            break;
    }
    return 0;
}

Type* type_get_child(Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN:
            return null;

        case TYPE_KIND_PTR:
            return type->ptr.child;
    }
    return null;
}

bool type_is_integer(Type* type) {
    if (type->kind == TYPE_KIND_BUILTIN && 
            builtin_type_is_integer(type->builtin.kind)) {
        return true;
    }
    return false;
}

bool type_is_boolean(Type* type) {
    if (type->kind == TYPE_KIND_BUILTIN &&
            type->builtin.kind == BUILTIN_TYPE_KIND_BOOLEAN) {
        return true;
    }
    return false;
}

bool type_is_void(Type* type) {
    if (type->kind == TYPE_KIND_BUILTIN && 
            builtin_type_is_void(type->builtin.kind)) {
        return true;
    }
    return false;
}

bool type_is_apint(Type* type) {
    if (type->kind == TYPE_KIND_BUILTIN && 
            builtin_type_is_apint(type->builtin.kind)) {
        return true;
    }
    return false;
}

size_t type_bytes(Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
            return builtin_type_bytes(&type->builtin);
        } break;

        case TYPE_KIND_PTR: {
            return PTR_BYTES;
        } break;
    }
    assert(0);
    return 0;
}

Type* stmt_get_type(Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_VARIABLE: {
            return stmt->variable.type;
        } break;

        case STMT_KIND_PARAM: {
            return stmt->param.type;
        } break;
        
        case STMT_KIND_FUNCTION: {
            return stmt->function.header->return_type;
        } break;
    }
    assert(0);
    return null;
}

bool token_is_cmp_op(Token* token) {
    switch (token->kind) {
        case TOKEN_KIND_DOUBLE_EQUAL:
        case TOKEN_KIND_BANG_EQUAL:
        case TOKEN_KIND_LANGBR:
        case TOKEN_KIND_LANGBR_EQUAL:
        case TOKEN_KIND_RANGBR:
        case TOKEN_KIND_RANGBR_EQUAL:
            return true;
        default:
            break;
    }
    return false;
}

bool token_is_magnitude_cmp_op(Token* token) {
    switch (token->kind) {
        case TOKEN_KIND_LANGBR:
        case TOKEN_KIND_LANGBR_EQUAL:
        case TOKEN_KIND_RANGBR:
        case TOKEN_KIND_RANGBR_EQUAL:
            return true;
        default:
            break;
    }
    return false;
}

Type* builtin_type_new(Token* token, BuiltinTypeKind kind) {
    ALLOC_WITH_TYPE(type, Type);
    type->kind = TYPE_KIND_BUILTIN;
    type->main_token = token;
    type->builtin.token = token;
    type->builtin.kind = kind;
    return type;
}

Type* ptr_type_new(Token* star, bool constant, Type* child) {
    ALLOC_WITH_TYPE(type, Type);
    type->kind = TYPE_KIND_PTR;
    type->main_token = star;
    type->ptr.constant = constant;
    type->ptr.child = child;
    return type;
}

FunctionHeader* function_header_new(
        Token* identifier, 
        std::vector<Stmt*> params, 
        Type* return_type,
        bool is_extern) {
    ALLOC_WITH_TYPE(header, FunctionHeader);
    header->identifier = identifier;
    header->params = std::move(params);
    header->return_type = return_type;
    header->is_extern = is_extern;
    header->llvmvalue = null;
    return header;
}

Stmt* function_stmt_new(FunctionHeader* header, Expr* body) {
    Stmt* stmt = (Stmt*)calloc(1, sizeof(Stmt));
    stmt->kind = STMT_KIND_FUNCTION;
    stmt->main_token = header->identifier;
    stmt->parent_func = null;
    stmt->function.header = header;
    stmt->function.body = body;
    stmt->function.stack_vars_size = 0;
    stmt->function.ifidx = 0;
    stmt->function.whileidx = 0;
    return stmt;
}

Stmt* variable_stmt_new(
        bool constant,
        bool is_extern,
        Token* identifier,
        Type* type,
        Expr* initializer) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_VARIABLE;
    stmt->main_token = identifier;
    stmt->parent_func = null;
    stmt->variable.constant = constant;
    stmt->variable.is_extern = is_extern;
    stmt->variable.global = false;
    stmt->variable.identifier = identifier;
    stmt->variable.type = type;
    stmt->variable.initializer_type = null;
    stmt->variable.initializer = initializer;
    stmt->variable.stack_offset = 0;
    stmt->variable.llvmvalue = null;
    return stmt;
}

Stmt* param_stmt_new(Token* identifier, Type* type, size_t idx) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_PARAM;
    stmt->main_token = identifier;
    stmt->parent_func = null;
    stmt->param.identifier = identifier;
    stmt->param.type = type;
    stmt->param.idx = idx;
    stmt->param.stack_offset = 0;
    stmt->param.llvmvalue = null;
    return stmt;
}

Stmt* while_stmt_new(Token* while_keyword, Expr* cond, Expr* body) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_WHILE;
    stmt->main_token = while_keyword;
    stmt->parent_func = null;
    stmt->whilelp.cond = cond;
    stmt->whilelp.body = body;
    return stmt;
}

Stmt* assign_stmt_new(Expr* left, Expr* right, Token* op) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_ASSIGN;
    stmt->main_token = op;
    stmt->parent_func = null;
    stmt->assign.left = left;
    stmt->assign.right = right;
    stmt->assign.op = op;
    return stmt;
}

Stmt* expr_stmt_new(Expr* child) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_EXPR;
    stmt->main_token = child->main_token;
    stmt->parent_func = null;
    stmt->expr.child = child;
    return stmt;
}

Expr* integer_expr_new(Token* integer, bigint* val) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_INTEGER;
    expr->main_token = integer;
    expr->type = null;
    expr->parent_func = null;
    expr->integer.integer = integer;
    expr->integer.val = val;
    return expr;
}

Expr* constant_expr_new(Token* keyword, ConstantKind kind) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_CONSTANT;
    expr->main_token = keyword;
    expr->type = null;
    expr->parent_func = null;
    expr->constant.keyword = keyword;
    expr->constant.kind = kind;
    return expr;
}

Expr* symbol_expr_new(Token* identifier) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_SYMBOL;
    expr->main_token = identifier;
    expr->type = null;
    expr->parent_func = null;
    expr->symbol.identifier = identifier;
    expr->symbol.ref = null;
    return expr;
}

Expr* function_call_expr_new(Expr* callee, std::vector<Expr*> args, Token* rparen) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_FUNCTION_CALL;
    expr->main_token = callee->main_token;
    expr->type = null;
    expr->parent_func = null;
    expr->function_call.callee = callee;
    expr->function_call.args = std::move(args);
    expr->function_call.rparen = rparen;
    return expr;
}

Expr* binop_expr_new(Expr* left, Expr* right, Token* op) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_BINOP;
    expr->main_token = op;
    expr->type = null;
    expr->parent_func = null;
    expr->binop.left = left;
    expr->binop.right = right;
    expr->binop.op = op;
    expr->binop.left_type = null;
    expr->binop.right_type = null;
    expr->binop.folded = false;
    return expr;
}

Expr* unop_expr_new(Expr* child, Token* op) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_UNOP;
    expr->main_token = op;
    expr->type = null;
    expr->parent_func = null;
    expr->unop.child = child;
    expr->unop.op = op;
    expr->unop.child_type = null;
    expr->unop.folded = false;
    return expr;
}

Expr* cast_expr_new(Expr* left, Type* to, Token* op) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_CAST;
    expr->main_token = op;
    expr->type = null;
    expr->parent_func = null;
    expr->cast.left = left;
    expr->cast.left_type = null;
    expr->cast.op = op;
    expr->cast.to = to;
    return expr;
}

Expr* block_expr_new(
        std::vector<Stmt*> stmts, 
        Expr* value, 
        Token* lbrace, 
        Token* rbrace) {
    Expr* expr = (Expr*)calloc(1, sizeof(Expr));
    expr->kind = EXPR_KIND_BLOCK;
    expr->main_token = lbrace;
    expr->type = null;
    expr->parent_func = null;
    expr->block.stmts = std::move(stmts);
    expr->block.value = value;
    expr->block.rbrace = rbrace;
    return expr;
}

IfBranch* if_branch_new(Expr* cond, Expr* body, IfBranchKind kind) {
    ALLOC_WITH_TYPE(br, IfBranch);
    br->cond = cond;
    br->body = body;
    br->kind = kind;
    return br;
}

Expr* if_expr_new(
        Token* if_keyword, 
        IfBranch* ifbr, 
        std::vector<IfBranch*> elseifbr, 
        IfBranch* elsebr) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_IF;
    expr->main_token = if_keyword;
    expr->type = null;
    expr->parent_func = null;
    expr->iff.ifbr = ifbr;
    expr->iff.elseifbr = std::move(elseifbr);
    expr->iff.elsebr = elsebr;
    return expr;
}

Type* builtin_type_placeholder_new(BuiltinTypeKind kind) {
    return builtin_type_new(null, kind);
}

Type* ptr_type_placeholder_new(bool constant, Type* child) {
    return ptr_type_new(null, constant, child); 
}

template <> struct fmt::formatter<Token> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const Token& token, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", token.lexeme);
    }
};

template <> struct fmt::formatter<Type> {
    enum { regular, llvm, nocolor } mode = regular;
    auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it == 'l') {
            mode = llvm;
            it++;
        } 
        else if (it != end && *it == 'n') {
            mode = nocolor;
            it++;
        } 
        return it; 
    }
    
    // if the runtime error:
    //   terminate called after throwing an instance of 'fmt::v8::format_error'
    //   what():  argument not found
    // occurs then remove char* or string literals from the args of fmt::format_to
    // or check if `:` is missing from fmt.
    template <typename FormatContext>
    auto format(const Type& type, FormatContext& ctx) -> decltype(ctx.out()) {
        decltype(ctx.out()) result = ctx.out();
        if (mode == nocolor) {
            g_fcyan_color = "";
            g_reset_color = "";
        }

        switch (type.kind) {
            case TYPE_KIND_BUILTIN: {
                std::string llvm_type;
                if (mode == llvm) {
                    if (type.builtin.kind == BUILTIN_TYPE_KIND_BOOLEAN)
                        llvm_type = "i1";
                    else 
                        llvm_type = builtin_type_kind_to_str(builtin_type_convert_to_llvm_type(type.builtin.kind));
                }
                else 
                    llvm_type = builtin_type_kind_to_str(type.builtin.kind);
                    
                result = fmt::format_to(
                        ctx.out(), 
                        "{}{}{}",
                        g_fcyan_color,
                        llvm_type,
                        g_reset_color);
            } break;

            case TYPE_KIND_PTR: {
                result = fmt::format_to(
                        ctx.out(), 
                        "{}*{}{}{}",
                        g_fcyan_color,
                        type.ptr.constant ? "const " : "",
                        *type.ptr.child,
                        g_reset_color);
            } break;
            
            default: {
                assert(0);
            } break;
        }

        if (mode == nocolor) {
            g_fcyan_color = ANSI_FCYAN;
            g_reset_color = ANSI_RESET;
        }
        return result;
    }
};

void init_types() {
    builtin_type_placeholders.uint8 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U8);
    builtin_type_placeholders.uint16 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U16);
    builtin_type_placeholders.uint32 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U32);
    builtin_type_placeholders.uint64 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U64);
    builtin_type_placeholders.usize = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_USIZE);
    builtin_type_placeholders.int8 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I8);
    builtin_type_placeholders.int16 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I16);
    builtin_type_placeholders.int32 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I32);
    builtin_type_placeholders.int64 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I64);
    builtin_type_placeholders.isize = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_ISIZE);
    builtin_type_placeholders.boolean = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_BOOLEAN);
    builtin_type_placeholders.void_kind = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_VOID);
    builtin_type_placeholders.void_ptr = ptr_type_placeholder_new(
            false, 
            builtin_type_placeholder_new(BUILTIN_TYPE_KIND_VOID));
}

