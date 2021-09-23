#include "aria.h"
#include "buf.h"
#include "stri.h"

char** aria_keywords;

BuiltinTypeMap builtin_type_map[_BUILTIN_TYPE_KIND_COUNT] = {
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

BuiltinTypePlaceholders builtin_type_placeholders;

static Type* builtin_type_placeholder_new(BuiltinTypeKind kind) {
    return builtin_type_new(null, kind);
}

void init_ds() {
    buf_push(aria_keywords, "fn");
    buf_push(aria_keywords, "let");
    buf_push(aria_keywords, "const");
    buf_push(aria_keywords, "mut");

    builtin_type_placeholders.u8 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U8);
    builtin_type_placeholders.u16 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U16);
    builtin_type_placeholders.u32 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U32);
    builtin_type_placeholders.u64 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_U64);
    builtin_type_placeholders.usize = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_USIZE);
    builtin_type_placeholders.i8 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I8);
    builtin_type_placeholders.i16 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I16);
    builtin_type_placeholders.i32 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I32);
    builtin_type_placeholders.i64 = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_I64);
    builtin_type_placeholders.isize = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_ISIZE);
    builtin_type_placeholders.void_type = builtin_type_placeholder_new(BUILTIN_TYPE_KIND_VOID);
}

bool is_token_lexeme_eq(Token* a, Token* b) {
    if (stri(a->lexeme) == stri(b->lexeme)) {
        return true;
    }
    return false;
}

BuiltinTypeKind builtin_type_str_to_kind(char* str) {
    for (size_t i = 0; i < _BUILTIN_TYPE_KIND_COUNT; i++) {
        if (stri(builtin_type_map[i].k) == stri(str)) {
            return builtin_type_map[i].v;
        }
    }
    return BUILTIN_TYPE_KIND_NONE;
}

char* builtin_type_kind_to_str(BuiltinTypeKind kind) {
    for (size_t i = 0; i < _BUILTIN_TYPE_KIND_COUNT; i++) {
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
            return false;

        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return false;
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
            return false;

        case BUILTIN_TYPE_KIND_I8:
        case BUILTIN_TYPE_KIND_I16:
        case BUILTIN_TYPE_KIND_I32:
        case BUILTIN_TYPE_KIND_I64:
        case BUILTIN_TYPE_KIND_ISIZE:
            return true;
        
        case BUILTIN_TYPE_KIND_BOOLEAN:
        case BUILTIN_TYPE_KIND_VOID:
        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return false;
    }
    return false;
}

size_t builtin_type_bytes(BuiltinTypeKind kind) {
    switch (kind) {
        case BUILTIN_TYPE_KIND_U8:
        case BUILTIN_TYPE_KIND_I8:
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
        case BUILTIN_TYPE_KIND_BOOLEAN:
            return 8;

        case BUILTIN_TYPE_KIND_VOID:
            return 0;

        case BUILTIN_TYPE_KIND_NONE:
            assert(0);
            return 0;
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
    if (type->kind == TYPE_KIND_BUILTIN && builtin_type_is_integer(type->builtin.kind)) {
        return true;
    }
    return false;
}

size_t type_bytes(Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
            return builtin_type_bytes(type->builtin.kind);
        } break;

        case TYPE_KIND_PTR: {
            return PTR_SIZE_BYTES;
        } break;
    }
    assert(0);
    return 0;
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
        Stmt** params, 
        Type* return_type) {
    ALLOC_WITH_TYPE(header, FunctionHeader);
    header->identifier = identifier;
    header->params = params;
    header->return_type = return_type;
    return header;
}

Stmt* function_stmt_new(FunctionHeader* header, Expr* body) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_FUNCTION;
    stmt->main_token = header->identifier;
    stmt->function.header = header;
    stmt->function.body = body;
    stmt->function.stack_vars_size = 0;
    return stmt;
}

Stmt* variable_stmt_new(
        bool constant,
        Token* identifier,
        Type* type,
        Expr* initializer) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_VARIABLE;
    stmt->main_token = identifier;
    stmt->variable.constant = constant;
    stmt->variable.identifier = identifier;
    stmt->variable.type = type;
    stmt->variable.initializer = initializer;
    return stmt;
}

Stmt* param_stmt_new(Token* identifier, Type* type) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_PARAM;
    stmt->main_token = identifier;
    stmt->param.identifier = identifier;
    stmt->param.type = type;
    return stmt;
}

Stmt* expr_stmt_new(Expr* child) {
    ALLOC_WITH_TYPE(stmt, Stmt);
    stmt->kind = STMT_KIND_EXPR;
    stmt->main_token = child->main_token;
    stmt->expr.child = child;
    return stmt;
}

Expr* integer_expr_new(Token* integer, bigint* val) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_INTEGER;
    expr->main_token = integer;
    expr->integer.integer = integer;
    expr->integer.val = val;
    return expr;
}

Expr* block_expr_new(Token* lbrace, Stmt** stmts, Expr* value) {
    ALLOC_WITH_TYPE(expr, Expr);
    expr->kind = EXPR_KIND_BLOCK;
    expr->main_token = lbrace;
    expr->block.lbrace = lbrace;
    expr->block.stmts = stmts;
    expr->block.value = value;
    return expr;
}

void _aria_vfprintf(
        const char* calleefile, 
        size_t calleeline, 
        FILE* file, 
        const char* fmt, 
        va_list ap) {
    va_list aq;
    va_copy(aq, ap);

    size_t fmtlen = strlen(fmt);
    for (size_t i = 0; i < fmtlen; i++) {
        if (fmt[i] == '{') {
            bool found_rbrace = false;
            i++;
            switch (fmt[i]) {
                case 'q': {
                    i++;
                    switch (fmt[i]) {
                        case 'i': {
                            fprintf(file, "%li", va_arg(aq, int64_t));
                        } break;

                        case 'u': {
                            fprintf(file, "%lu", va_arg(aq, uint64_t));
                        } break;

                        default: {
                            fprintf(
                                    stderr, 
                                    "aria_fprintf: (%s:%lu) expect [u|i] "
                                    "after `q`\n", 
                                    calleefile, 
                                    calleeline);
                            return;
                        } break;
                    }
                    i++;
                } break;

                case 't': {
                    i++;
                    if (fmt[i] == 'k') {
                        i++;
                        fprintf_token(file, va_arg(aq, Token*));
                    }
                    else {
                        fprintf_type(file, va_arg(aq, Type*));
                    }
                } break;

                case 'c': {
                    i++;
                    fprintf(file, "%c", va_arg(aq, int));
                } break;

                case 's': {
                    i++;
                    fprintf(file, "%s", va_arg(aq, char*));
                } break;

                case '}': {
                    fprintf(file, "%d", va_arg(aq, int));
                    found_rbrace = true; 
                } break;

                default: {
                    fprintf(
                            stderr, 
                            "aria_fprintf: (%s:%lu) unknown specifier `%c`\n", 
                            calleefile, 
                            calleeline, 
                            fmt[i]);
                    return;
                } break;
            }
            
            if (!found_rbrace) {
                if (fmt[i] != '}') {
                    fprintf(
                            stderr, 
                            "aria_fprintf: (%s:%lu) unexpected eos while "
                            "finding `}`\n", 
                            calleefile, 
                            calleeline);
                    return;
                }
            }
        } 
        else {
            putc(fmt[i], file);
        }
    }

    va_end(aq);
}

void _aria_fprintf(
        const char* calleefile, 
        size_t calleeline, 
        FILE* file, 
        const char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);
    _aria_vfprintf(calleefile, calleeline, file, fmt, ap);
    va_end(ap);
}

void fprintf_type(FILE* file, Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
            fprintf(file, "%s", builtin_type_kind_to_str(type->builtin.kind));
        } break;

        case TYPE_KIND_PTR: {
            fputc('*', file);
            if (type->ptr.constant) {
                fprintf(file, "const ");
            }
            fprintf_type(file, type->ptr.child);
        } break;
    }
}

void fprintf_token(FILE* file, Token* token) {
    fprintf(file, "{ %s, %d }", token->lexeme, token->kind);
}
