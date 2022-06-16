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
    TOKEN_KIND_STRING,
    TOKEN_KIND_INTEGER,
    TOKEN_KIND_LBRACE,
    TOKEN_KIND_RBRACE,
    TOKEN_KIND_LBRACK,
    TOKEN_KIND_RBRACK,
    TOKEN_KIND_LPAREN,
    TOKEN_KIND_RPAREN,
    TOKEN_KIND_COLON,
    TOKEN_KIND_SEMICOLON,
    TOKEN_KIND_DOT,
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
    TYPE_KIND_ARRAY,
    TYPE_KIND_CUSTOM,
};

enum BuiltinTypeKind {
    BUILTIN_TYPE_KIND_U8,
    BUILTIN_TYPE_KIND_U16,
    BUILTIN_TYPE_KIND_U32,
    BUILTIN_TYPE_KIND_U64,
    BUILTIN_TYPE_KIND_I8,
    BUILTIN_TYPE_KIND_I16,
    BUILTIN_TYPE_KIND_I32,
    BUILTIN_TYPE_KIND_I64,
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
    { "i8", BUILTIN_TYPE_KIND_I8 },
    { "i16", BUILTIN_TYPE_KIND_I16 },
    { "i32", BUILTIN_TYPE_KIND_I32 },
    { "i64", BUILTIN_TYPE_KIND_I64 },
    { "bool", BUILTIN_TYPE_KIND_BOOLEAN },
    { "void", BUILTIN_TYPE_KIND_VOID },
};

struct BuiltinTypePlaceholders {
    Type* uint8;
    Type* uint16;
    Type* uint32;
    Type* uint64;
    Type* int8;
    Type* int16;
    Type* int32;
    Type* int64;
    Type* boolean;
    Type* void_kind;
    Type* void_ptr;
    Type* slice_to_const_u8;
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

// TODO: make this depend on the hardware
const size_t PTR_BYTES = 8;

struct ArrayType {
    Expr* len;
    u64 lennum;
    Type* elem_type;
    
    // automatically set by compiler
    bool constant;  
};

enum CustomTypeKind {
    CUSTOM_TYPE_KIND_STRUCT,
    CUSTOM_TYPE_KIND_SLICE,
};

struct CustomType {
    union {
        Token* identifier;
        struct {
            bool constant;
            Type* child;
        } slice;
    };
    CustomTypeKind kind;
    Stmt* ref;
};

struct Type {
    TypeKind kind;
    Token* main_token;
    union {
        BuiltinType builtin;
        PtrType ptr;
        ArrayType array;
        CustomType custom;
    };
};

enum ExprKind {
    EXPR_KIND_INTEGER,
    EXPR_KIND_CONSTANT,
    EXPR_KIND_ARRAY_LITERAL,
    EXPR_KIND_FIELD_ACCESS,
    EXPR_KIND_SYMBOL,
    EXPR_KIND_FUNCTION_CALL,
    EXPR_KIND_INDEX,
    EXPR_KIND_BINOP,
    EXPR_KIND_UNOP,
    EXPR_KIND_CAST,
    EXPR_KIND_BLOCK,
    EXPR_KIND_IF,
    EXPR_KIND_WHILE,
};

struct IntegerExpr {
    Token* integer;
    bigint* val;
};

enum ConstantKind {
    CONSTANT_KIND_BOOLEAN_TRUE,
    CONSTANT_KIND_BOOLEAN_FALSE,
    CONSTANT_KIND_NULL,
    CONSTANT_KIND_STRING,
};

struct ConstantExpr {
    Token* token;
    ConstantKind kind;
};

struct ArrayLiteralExpr {
    std::vector<Expr*> elems;
};

struct FieldAccessExpr {
    Expr* left;
    Type* left_type;
    Token* right;
    Stmt* rightref;
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

struct IndexExpr {
    Expr* left;
    Expr* idx;
    Type* left_type;
    Token* lbrack;
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

struct WhileExpr {
    Expr* cond;
    Expr* body;
};

struct Expr {
    ExprKind kind;
    Token* main_token;
    Type* type;
    bool constant;
    Stmt* parent_func;
    union {
        IntegerExpr integer;
        ConstantExpr constantexpr;
        ArrayLiteralExpr arraylit;
        FieldAccessExpr fieldacc;
        SymbolExpr symbol;
        FunctionCallExpr function_call;
        IndexExpr index;
        BinopExpr binop;
        UnopExpr unop;
        CastExpr cast;
        BlockExpr block;
        IfExpr iff;
        WhileExpr whilelp;
    };

    Expr() {}
};

enum StmtKind {
    STMT_KIND_STRUCT,
    STMT_KIND_FIELD,
    STMT_KIND_FUNCTION,
    STMT_KIND_VARIABLE,
    STMT_KIND_PARAM,
    STMT_KIND_ASSIGN,
    STMT_KIND_RETURN,
    STMT_KIND_EXPR,
};

struct FieldStmt {
    Token* identifier;
    Type* type;
    size_t idx;
};

struct StructStmt {
    Token* identifier;
    std::vector<Stmt*> fields;
    ssize_t alignment;
    ssize_t bytes_when_packed;
    bool is_slice;
    LLVMTypeRef llvmtype;
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
    LLVMValueRef llvmvalue;
};

struct ParamStmt {
    Token* identifier;
    Type* type;
    size_t idx;
    LLVMValueRef llvmvalue;
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
        StructStmt structure;
        FieldStmt field;
        FunctionStmt function;
        VariableStmt variable;
        ParamStmt param;
        AssignStmt assign;
        ReturnStmt return_stmt;
        ExprStmt expr;
    };

    Stmt() {}
};

std::string aria_keywords[] = {
    "struct",
    "fn",
    "const",
    "var",
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
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
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
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
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
        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
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
        case BUILTIN_TYPE_KIND_BOOLEAN:
            return false;

        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
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

        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
            return kind;
        
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
            return 0;
        case BUILTIN_TYPE_KIND_I8:  return INT8_MIN;
        case BUILTIN_TYPE_KIND_I16: return INT16_MIN;
        case BUILTIN_TYPE_KIND_I32: return INT32_MIN;
        case BUILTIN_TYPE_KIND_I64: return INT64_MIN;

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

        case BUILTIN_TYPE_KIND_I8:  return INT8_MAX;
        case BUILTIN_TYPE_KIND_I16: return INT16_MAX;
        case BUILTIN_TYPE_KIND_I32: return INT32_MAX;
        case BUILTIN_TYPE_KIND_I64: return INT64_MAX;

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

bool type_is_slice(Type* type) {
    if (type->kind == TYPE_KIND_CUSTOM &&
            type->custom.kind == CUSTOM_TYPE_KIND_SLICE) {
        return true;
    }
    return false;
}

// literal comparison, no cast
// The `cast_to_const` parameter should be set to true if the function should
// cast the `from` type to `to` type taking into account their constness (ex:
// from `*u8` to `*const u8`. If set to false, the function will do a literal
// comparison.
bool type_is_equal(Type* from, Type* to, bool cast_to_const) {
    if (from->kind == to->kind) {
        switch (from->kind) {
            case TYPE_KIND_BUILTIN: {
                return from->builtin.kind == to->builtin.kind;
            } break;

            case TYPE_KIND_PTR: {
                if (cast_to_const) {
                    if (!from->ptr.constant || to->ptr.constant) {
                        return type_is_equal(from->ptr.child, to->ptr.child, cast_to_const);
                    }
                }
                else {
                    if (from->ptr.constant == to->ptr.constant) {
                        return type_is_equal(from->ptr.child, to->ptr.child, cast_to_const);
                    }
                }
            } break;

            case TYPE_KIND_ARRAY: {
                assert(from->array.len->kind == EXPR_KIND_INTEGER && to->array.len->kind == EXPR_KIND_INTEGER);
                if (bigint_cmp_mag(from->array.len->integer.val, to->array.len->integer.val) == BIGINT_ORD_EQ &&
                    from->array.len->integer.val->sign == to->array.len->integer.val->sign) {
                    if (cast_to_const) {
                        if (!from->array.constant || to->array.constant) {
                            return type_is_equal(from->array.elem_type, to->array.elem_type, cast_to_const);
                        }
                    }
                    else {
                        if (from->array.constant == to->array.constant) {
                            return type_is_equal(from->array.elem_type, to->array.elem_type, cast_to_const);
                        }
                    }
                }
            } break;

            case TYPE_KIND_CUSTOM: {
                if (from->custom.kind == to->custom.kind) {
                    switch (from->custom.kind) {
                        case CUSTOM_TYPE_KIND_STRUCT: {
                            return is_token_lexeme_eq(from->custom.identifier, to->custom.identifier);
                        } break;

                        case CUSTOM_TYPE_KIND_SLICE: {
                            if (cast_to_const) {
                                if (!from->custom.slice.constant || to->custom.slice.constant) {
                                    return type_is_equal(from->custom.slice.child, to->custom.slice.child, cast_to_const);
                                }
                            } 
                            else {
                                if (from->custom.slice.constant == to->custom.slice.constant) {
                                    return type_is_equal(from->custom.slice.child, to->custom.slice.child, cast_to_const);
                                }
                            }
                        } break;
                    }
                }
            } break;
        }
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

        case TYPE_KIND_ARRAY: {
            return type->array.lennum * type_bytes(type->array.elem_type);
        } break;

        /* case TYPE_KIND_SLICE: { */
        /*     return 2 * 8; */
        /* } break; */

        case TYPE_KIND_CUSTOM: {
            if (type->custom.ref->structure.bytes_when_packed == -1) {
                size_t bytes = 0;
                for (Stmt* field: type->custom.ref->structure.fields) {
                    bytes += type_bytes(field->field.type);
                }
                type->custom.ref->structure.bytes_when_packed = bytes;
                return bytes;
            }
            else return type->custom.ref->structure.bytes_when_packed;
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

Token* token_new(
    TokenKind kind,
    const std::string& lexeme,
    char* start,
    char* end,
    Srcfile* srcfile,
    size_t line,
    size_t col,
    size_t ch_count) {
    ALLOC_WITH_TYPE(token, Token);
    token->kind = kind;
    token->lexeme = lexeme;
    token->start = start;
    token->end = end;
    token->srcfile = srcfile;
    token->line = line;
    token->col = col;
    token->ch_count = ch_count;
    return token;
}

inline Token* token_identifier_placeholder_new(const std::string& lexeme) {
    return token_new(
            TOKEN_KIND_IDENTIFIER,
            lexeme,
            null,
            null,
            null,
            0,
            0,
            0);
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

Type* array_type_new(Expr* len, Type* elem_type, Token* lbrack) {
    ALLOC_WITH_TYPE(type, Type);
    type->kind = TYPE_KIND_ARRAY;
    type->main_token = lbrack;
    type->array.len = len;
    type->array.lennum = 0;
    type->array.elem_type = elem_type;
    type->array.constant = true;
    return type;
}

Type* array_type_placeholder_new(u64 len, Type* elem_type) {
    Type* ty = array_type_new(null, elem_type, null);
    ty->array.lennum = len;
    return ty;
}

Type* slice_type_new(Token* lbrack, bool constant, Type* child) {
    ALLOC_WITH_TYPE(type, Type);
    type->kind = TYPE_KIND_CUSTOM;
    type->main_token = lbrack;
    type->custom.kind = CUSTOM_TYPE_KIND_SLICE;
    type->custom.slice.constant = constant;
    type->custom.slice.child = child;
    type->custom.ref = null;
    return type;
}

Type* custom_type_new(Token* identifier) {
    ALLOC_WITH_TYPE(type, Type);
    type->kind = TYPE_KIND_CUSTOM;
    type->main_token = identifier;
    type->custom.kind = CUSTOM_TYPE_KIND_STRUCT;
    type->custom.identifier = identifier;
    type->custom.ref = null;
    return type;
}

Stmt* field_stmt_new(Token* identifier, Type* type, size_t idx) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_FIELD;
    stmt->main_token = identifier;
    stmt->parent_func = null;
    stmt->field.identifier = identifier;
    stmt->field.type = type;
    stmt->field.idx = idx;
    return stmt;
}

Stmt* struct_stmt_new(
        Token* identifier,
        std::vector<Stmt*> fields,
        bool is_slice) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_STRUCT;
    stmt->main_token = identifier;
    stmt->parent_func = null;
    stmt->structure.identifier = identifier;
    stmt->structure.fields = std::move(fields);
    stmt->structure.alignment = -1;
    stmt->structure.bytes_when_packed = -1;
    stmt->structure.is_slice = is_slice;
    stmt->structure.llvmtype = null;
    return stmt;
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
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_FUNCTION;
    stmt->main_token = header->identifier;
    stmt->parent_func = null;
    stmt->function.header = header;
    stmt->function.body = body;
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
    stmt->param.llvmvalue = null;
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
    expr->constant = true;
    expr->parent_func = null;
    expr->integer.integer = integer;
    expr->integer.val = val;
    return expr;
}

Expr* constant_expr_new(Token* token, ConstantKind kind) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_CONSTANT;
    expr->main_token = token;
    expr->type = null;
    expr->constant = true;
    expr->parent_func = null;
    expr->constantexpr.token = token;
    expr->constantexpr.kind = kind;
    return expr;
}

Expr* array_literal_expr_new(std::vector<Expr*> elems, Token* lbrack) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_ARRAY_LITERAL;
    expr->main_token = lbrack;
    expr->type = null;
    expr->constant = true;
    expr->parent_func = null;
    expr->arraylit.elems = std::move(elems);
    return expr;
}

Expr* field_access_expr_new(Expr* left, Token* right, Token* dot) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_FIELD_ACCESS;
    expr->main_token = dot;
    expr->type = null;
    expr->constant = true;
    expr->parent_func = null;
    expr->fieldacc.left = left;
    expr->fieldacc.left_type = null;
    expr->fieldacc.right = right;
    expr->fieldacc.rightref = null;
    return expr;
}

Expr* symbol_expr_new(Token* identifier) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_SYMBOL;
    expr->main_token = identifier;
    expr->type = null;
    expr->constant = true;
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
    expr->constant = true;
    expr->parent_func = null;
    expr->function_call.callee = callee;
    expr->function_call.args = std::move(args);
    expr->function_call.rparen = rparen;
    return expr;
}

Expr* index_expr_new(Expr* left, Expr* idx, Token* lbrack) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_INDEX;
    expr->main_token = left->main_token;
    expr->type = null;
    expr->constant = true;
    expr->parent_func = null;
    expr->index.left = left;
    expr->index.idx = idx;
    expr->index.left_type = null;
    expr->index.lbrack = lbrack;
    return expr;
}

Expr* binop_expr_new(Expr* left, Expr* right, Token* op) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_BINOP;
    expr->main_token = op;
    expr->type = null;
    expr->constant = true;
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
    expr->constant = true;
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
    expr->constant = true;
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
    expr->constant = true;
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
    expr->constant = true;
    expr->parent_func = null;
    expr->iff.ifbr = ifbr;
    expr->iff.elseifbr = std::move(elseifbr);
    expr->iff.elsebr = elsebr;
    return expr;
}

Expr* while_expr_new(Token* while_keyword, Expr* cond, Expr* body) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_WHILE;
    expr->main_token = while_keyword;
    expr->type = null;
    expr->constant = true;
    expr->parent_func = null;
    expr->whilelp.cond = cond;
    expr->whilelp.body = body;
    return expr;
}

Type* builtin_type_placeholder_new(BuiltinTypeKind kind) {
    return builtin_type_new(null, kind);
}

Type* ptr_type_placeholder_new(bool constant, Type* child) {
    return ptr_type_new(null, constant, child); 
}

Type* slice_type_placeholder_new(bool constant, Type* child) {
    return slice_type_new(null, constant, child); 
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

            case TYPE_KIND_ARRAY: {
                result = fmt::format_to(
                        ctx.out(),
                        "{}[{}]{}{}",
                        g_fcyan_color,
                        type.array.lennum,
                        *type.array.elem_type,
                        g_reset_color);
            } break;
            
            case TYPE_KIND_CUSTOM: {
                switch (type.custom.kind) {
                    case CUSTOM_TYPE_KIND_STRUCT: {
                        result = fmt::format_to(
                                ctx.out(), 
                                "{}{}{}",
                                g_fcyan_color,
                                *type.custom.identifier,
                                g_reset_color);
                    } break;

                    case CUSTOM_TYPE_KIND_SLICE: {
                        result = fmt::format_to(
                                ctx.out(), 
                                "{}[]{}{}{}",
                                g_fcyan_color,
                                type.custom.slice.constant ? "const " : "",
                                *type.custom.slice.child,
                                g_reset_color);
                    } break;
                }
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
    builtin_type_placeholders.int8 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I8);
    builtin_type_placeholders.int16 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I16);
    builtin_type_placeholders.int32 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I32);
    builtin_type_placeholders.int64 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I64);
    builtin_type_placeholders.boolean = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_BOOLEAN);
    builtin_type_placeholders.void_kind = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_VOID);
    builtin_type_placeholders.void_ptr = ptr_type_placeholder_new(
            false, 
            builtin_type_placeholders.void_kind);
    builtin_type_placeholders.slice_to_const_u8 = slice_type_placeholder_new(
            true,
            builtin_type_placeholders.uint8);
}

