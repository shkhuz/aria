typedef struct Node Node;
typedef struct SrcFile SrcFile;

#define PROCEDURE_DECL_KEYWORD "proc"
#define VARIABLE_DECL_KEYWORD "let"
#define CONSTANT_DECL_KEYWORD "const"

char* keywords[] = {
    "module",
    "struct",
    "import",
    PROCEDURE_DECL_KEYWORD,
    VARIABLE_DECL_KEYWORD,
    CONSTANT_DECL_KEYWORD,
    "return",
    "true",
    "false",
};

char* directives[] = {
};

typedef enum {
    TYPE_PRIMITIVE_KIND_U8,
    TYPE_PRIMITIVE_KIND_U16,
    TYPE_PRIMITIVE_KIND_U32,
    TYPE_PRIMITIVE_KIND_U64,
    TYPE_PRIMITIVE_KIND_USIZE,
    TYPE_PRIMITIVE_KIND_I8,
    TYPE_PRIMITIVE_KIND_I16,
    TYPE_PRIMITIVE_KIND_I32,
    TYPE_PRIMITIVE_KIND_I64,
    TYPE_PRIMITIVE_KIND_ISIZE,
    TYPE_PRIMITIVE_KIND_BOOL,
    TYPE_PRIMITIVE_KIND_VOID,
    TYPE_PRIMITIVE_KIND_NONE,
} TypePrimitiveKind;

#define PRIMITIVE_TYPE_PTR_SIZE 8

typedef enum {
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_KEYWORD,
    TOKEN_KIND_DIRECTIVE,
    TOKEN_KIND_STRING,
    TOKEN_KIND_NUMBER,
    TOKEN_KIND_LPAREN,
    TOKEN_KIND_RPAREN,
    TOKEN_KIND_LBRACE,
    TOKEN_KIND_RBRACE,
    TOKEN_KIND_SEMICOLON,
    TOKEN_KIND_COLON,
    TOKEN_KIND_DOUBLE_COLON,
    TOKEN_KIND_COMMA,
    TOKEN_KIND_PLUS,
    TOKEN_KIND_MINUS,
    TOKEN_KIND_STAR,
    TOKEN_KIND_FSLASH,
    TOKEN_KIND_EQUAL,
    TOKEN_KIND_AMPERSAND,
    TOKEN_KIND_EOF,
    TOKEN_KIND_NONE,
} TokenKind;

typedef struct {
    TokenKind kind;
    char* lexeme, *start, *end;
    SrcFile* srcfile;
    u64 line, column, char_count;
    union {
        struct {
            bigint* val;
        } number;
    };
} Token;

Token* token_alloc(
        TokenKind kind,
        char* lexeme, 
        char* start,
        char* end,
        SrcFile* srcfile,
        u64 line,
        u64 column,
        u64 char_count) {
    alloc_with_type(token, Token);
    token->kind = kind;
    token->lexeme = lexeme;
    token->start = start;
    token->end = end;
    token->srcfile = srcfile;
    token->line = line;
    token->column = column;
    token->char_count = char_count;
    return token;
}

bool token_lexeme_eq(
        Token* a, 
        Token* b) {
    if (stri(a->lexeme) == stri(b->lexeme)) {
        return true;
    }
    return false;
}

typedef enum {
    NODE_KIND_NUMBER,
    NODE_KIND_BOOLEAN,
    NODE_KIND_TYPE_PRIMITIVE,
    NODE_KIND_TYPE_CUSTOM,
    NODE_KIND_TYPE_PTR,
    NODE_KIND_STATIC_ACCESSOR,
    NODE_KIND_SYMBOL,
    NODE_KIND_PROCEDURE_CALL,
    NODE_KIND_BLOCK,
    NODE_KIND_PARAM,
    NODE_KIND_UNARY,
    NODE_KIND_DEREF,
    NODE_KIND_BINARY,
    NODE_KIND_ASSIGN,
    NODE_KIND_EXPR_STMT,
    NODE_KIND_RETURN,
    NODE_KIND_VARIABLE_DECL,
    NODE_KIND_PROCEDURE_DECL,
    NODE_KIND_IMPLICIT_MODULE,
} NodeKind;

struct Node {
    NodeKind kind;
    Token* head;
    Token* tail;

    union {
        struct {
            Token* number;
            bigint* val;
        } number;

        struct {
            Token* boolean;
        } boolean;

        struct {
            Token* token;
            TypePrimitiveKind kind;
        } type_primitive;

        struct {
            Node* symbol;
        } type_custom;

        struct {
            bool constant;
            Node* right;
        } type_ptr;

        struct {
            Token** accessors;
            bool from_global_scope;
            Token* head;
        } static_accessor;

        // A symbol is an identifier
        // with an optional `::` accessor
        // before it.
        struct {
            Node* static_accessor;
            Token* identifier;
            Node* ref;
        } symbol;

        struct {
            Node* callee;
            Node** args;
        } procedure_call;

        struct {
            Node** nodes;
            Node* return_value;
        } block;

        struct {
            Token* op;
            Node* right;
            bigint* val;
        } unary;

        struct {
            Token* op;
            Node* right;
            bool constant;
        } deref;

        struct {
            Token* op;
            Node* left;
            Node* right;
            bigint* val;
        } binary;

        struct {
            Token* op;
            Node* left;
            Node* right;
        } assign;

        struct {
            Token* identifier;
            Node* type;
        } param;

        struct {
            Node* expr;
        } expr_stmt;

        struct {
            Node* right;
            Node* procedure;
        } return_;

        struct {
            bool constant;
            Token* identifier;
            Node* type;
            Node* initializer;
            bool in_procedure;
        } variable_decl;

        struct {
            Token* identifier;
            Node** params;
            Node* type;
            Node* body;
            int local_vars_bytes;
        } procedure_decl;

        struct {
            Token* identifier;
            char* filename;
            char* file_path;
            SrcFile* srcfile;
        } implicit_module;
    };
};

Node* node_number_new(
        Token* number) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_NUMBER;
    node->head = number;
    node->number.number = number;
    node->number.val = number->number.val;
    node->tail = number;
    return node;
}

Node* node_boolean_new(
        Token* boolean) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_BOOLEAN;
    node->head = boolean;
    node->boolean.boolean = boolean;
    node->tail = boolean;
    return node;
}

Node* node_type_primitive_new(
        Token* token,
        TypePrimitiveKind kind) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE_PRIMITIVE;
    node->head = token;
    node->type_primitive.token = token;
    node->type_primitive.kind = kind;
    node->tail = token;
    return node;
}

Node* node_type_custom_new(
        Node* symbol) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE_CUSTOM;
    node->head = symbol->head;
    node->type_custom.symbol = symbol;
    node->tail = symbol->tail;
    return node;
}

Node* node_type_ptr_new(
        Token* star,
        bool constant,
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE_PTR;
    node->head = star;
    node->type_ptr.constant = constant;
    node->type_ptr.right = right;
    node->tail = right->tail;
    return node;
}

Node* node_static_accessor_new(
        Token** accessors,
        bool from_global_scope,
        Token* head) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_STATIC_ACCESSOR;
    node->head = head;
    node->static_accessor.accessors = accessors;
    node->static_accessor.from_global_scope = from_global_scope;
    node->static_accessor.head = head;
    node->tail = (accessors ? accessors[buf_len(accessors)-1] : head);
    return node;
}

Node* node_symbol_new(
        Node* static_accessor,
        Token* identifier) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_SYMBOL;
    node->head = (static_accessor->head ? static_accessor->head : identifier);
    node->symbol.static_accessor = static_accessor;
    node->symbol.identifier = identifier;
    node->symbol.ref = null;
    node->tail = identifier;
    return node;
}

Node* node_procedure_call_new(
        Node* callee,
        Node** args,
        Token* rparen) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_PROCEDURE_CALL;
    node->head = callee->head;
    node->procedure_call.callee = callee;
    node->procedure_call.args = args;
    node->tail = rparen;
    return node;
}

Node* node_block_new(
        Token* lbrace,
        Node** nodes,
        Node* return_value,
        Token* rbrace) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_BLOCK;
    node->head = lbrace;
    node->block.nodes = nodes;
    node->block.return_value = return_value;
    node->tail = rbrace;
    return node;
}

Node* node_param_new(
        Token* identifier,
        Node* type) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_PARAM;
    node->head = identifier;
    node->param.identifier = identifier;
    node->param.type = type;
    node->tail = type->tail;
    return node;
}

Node* node_expr_unary_new(
        Token* op,
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_UNARY;
    node->head = op;
    node->unary.op = op;
    node->unary.right = right;
    node->unary.val = null;
    node->tail = right->tail;
    return node;
}

Node* node_expr_deref_new(
        Token* op,
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_DEREF;
    node->head = op;
    node->deref.op = op;
    node->deref.right = right;
    node->deref.constant = false;
    node->tail = right->tail;
    return node;
}

Node* node_expr_binary_new(
        Token* op,
        Node* left,
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_BINARY;
    node->head = left->head;
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    node->binary.val = null;
    node->tail = right->tail;
    return node;
}

Node* node_expr_assign_new(
        Token* op,
        Node* left,
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_ASSIGN;
    node->head = left->head;
    node->assign.op = op;
    node->assign.left = left;
    node->assign.right = right;
    node->tail = right->tail;
    return node;
}

Node* node_expr_stmt_new(
        Node* expr,
        Token* semicolon) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_EXPR_STMT;
    node->head = expr->head;
    node->expr_stmt.expr = expr;
    node->tail = semicolon;
    return node;
}

Node* node_return_new(
        Token* keyword,
        Node* right,
        Token* semicolon) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_RETURN;
    node->head = (keyword ? keyword : right->head);
    node->return_.right = right;
    node->return_.procedure = null;
    node->tail = (semicolon ? semicolon : right->tail);
    return node;
}

Node* node_procedure_decl_new(
        Token* keyword,
        Token* identifier,
        Node** params,
        Node* type,
        Node* body) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_PROCEDURE_DECL;
    node->head = keyword;
    node->procedure_decl.identifier = identifier;
    node->procedure_decl.params = params;
    node->procedure_decl.type = type;
    node->procedure_decl.body = body;
    node->procedure_decl.local_vars_bytes = 0;
    // TODO: should we change the `tail` to
    // the `tail` of the type (or the parenthesis)?
    node->tail = body->tail;
    return node;
}

Node* node_implicit_module_new(
        Token* keyword,
        Token* identifier,
        char* filename,
        char* file_path) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_IMPLICIT_MODULE;
    node->head = keyword;
    node->implicit_module.identifier = identifier;
    node->implicit_module.filename = filename;
    node->implicit_module.file_path = file_path;
    node->tail = identifier;
    return node;
}

Node* node_variable_decl_new(
        Token* keyword,
        bool constant, 
        Token* identifier,
        Node* type,
        Node* initializer,
        Token* semicolon,
        bool in_procedure) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_VARIABLE_DECL;
    node->head = keyword;
    node->variable_decl.constant = constant;
    node->variable_decl.identifier = identifier;
    node->variable_decl.type = type;
    node->variable_decl.initializer = initializer;
    node->variable_decl.in_procedure = in_procedure;
    // TODO: should we change the `tail` to 
    // the `tail` of the type (or the identifier 
    // before the colon)?
    node->tail = semicolon;
    return node;
}

Token* node_get_main_token(Node* node, bool assert_on_erroneous_node) {
    switch (node->kind) {
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_RETURN:
        case NODE_KIND_TYPE_PRIMITIVE:
		case NODE_KIND_TYPE_CUSTOM:
		case NODE_KIND_TYPE_PTR:
        case NODE_KIND_BLOCK:
        case NODE_KIND_NUMBER:
        case NODE_KIND_STATIC_ACCESSOR:

        // TODO: check if these expressions should 
        // return an identifier...
        case NODE_KIND_SYMBOL:
        case NODE_KIND_PROCEDURE_CALL:
        {
            if (assert_on_erroneous_node) {
                assert(0);
            }
        } break;

        case NODE_KIND_UNARY:
            return node->unary.op;
        case NODE_KIND_BINARY:
            return node->binary.op;
        case NODE_KIND_DEREF:
            return node->deref.op;
        case NODE_KIND_ASSIGN:
            return node->assign.op;

        case NODE_KIND_VARIABLE_DECL:
            return node->variable_decl.identifier;
        case NODE_KIND_PARAM:
            return node->param.identifier;
        case NODE_KIND_PROCEDURE_DECL:
            return node->procedure_decl.identifier;
        case NODE_KIND_IMPLICIT_MODULE:
            return node->implicit_module.identifier;
    }
    return null;
}

/* char* node_to_str(Node* node) { */
/*     char* buf = null; */
/*     switch (node->kind) { */
/*         case NODE_KIND_NUMBER: */
/*         { */
/*             buf_write(buf, node->number.number->lexeme); */
/*         } break; */

/*         case NODE_KIND_UNARY: */
/*         { */
/*             buf_write(buf, node->unary.op->lexeme); */
/*             char* right = node_to_str(node->unary.right); */
/*             buf_write(buf, right); */
/*             buf_free(right); */
/*         } break; */

/*         default: assert(0); break; */
/*     } */
/*     buf_push(buf, '\0'); */
/*     return buf; */
/* } */

Node* node_get_type(Node* node, bool assert_on_erroneous_node) {
    switch (node->kind) {
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_IMPLICIT_MODULE:
        // TODO: check if these expressions should 
        // return a type...
        case NODE_KIND_TYPE_PRIMITIVE:
        case NODE_KIND_TYPE_CUSTOM:
        case NODE_KIND_TYPE_PTR:
        case NODE_KIND_BLOCK:
        case NODE_KIND_NUMBER:
        case NODE_KIND_UNARY:
        case NODE_KIND_PROCEDURE_CALL:
        {
            if (assert_on_erroneous_node) {
                assert(0);
            }
        } break;

        case NODE_KIND_VARIABLE_DECL:
            return node->variable_decl.type;
        case NODE_KIND_PARAM:
            return node->param.type;
        case NODE_KIND_PROCEDURE_DECL:
            return node->procedure_decl.type;
        case NODE_KIND_SYMBOL:
            return node_get_type(node->symbol.ref, true);
    }
    return null;
}

char* node_get_name_in_word(Node* node) {
    switch (node->kind) {
        case NODE_KIND_IMPLICIT_MODULE: return "module";
        case NODE_KIND_PROCEDURE_DECL: return "procedure";
        case NODE_KIND_VARIABLE_DECL: 
        case NODE_KIND_PARAM: return "variable";
        default: assert(0);
    }
    return null;
}

const bigint* node_get_val_number(Node* node) {
    switch (node->kind) {
        case NODE_KIND_NUMBER:
            return (const bigint*)node->number.val;
        case NODE_KIND_UNARY:
        {
            /* if (node->unary.op->kind == TOKEN_KIND_MINUS) { */
            /*     if (node->unary.val == null) { */
            /*         return null; */
            /*     } */
            /*     return (const bigint*)node->unary.val; */
            /* } else { */
            /*     assert(0); */
            /* } */
            return (const bigint*)node->unary.val;
        } break;
        
        case NODE_KIND_BINARY:
        {
            return (const bigint*)node->binary.val;
        } break;

        case NODE_KIND_SYMBOL:
            return null;

        default: assert(0); break;
    }
    return null;
}

struct SrcFile {
    File* contents;
    Token** tokens;
    Node** nodes;
};

bool primitive_type_is_integer(TypePrimitiveKind kind);

bool type_is_integer(Node* node) {
    if (node->kind == NODE_KIND_TYPE_PRIMITIVE &&
        primitive_type_is_integer(node->type_primitive.kind)) {
        return true;
    }
    return false;
}

typedef struct {
    char* str;
    TypePrimitiveKind kind;
} StrToTypePrimitiveKindMap;

StrToTypePrimitiveKindMap PRIMITIVE_TYPES[] = {
    { "u8", TYPE_PRIMITIVE_KIND_U8 },
    { "u16", TYPE_PRIMITIVE_KIND_U16 },
    { "u32", TYPE_PRIMITIVE_KIND_U32 },
    { "u64", TYPE_PRIMITIVE_KIND_U64 },
    { "usize", TYPE_PRIMITIVE_KIND_USIZE },
    { "i8", TYPE_PRIMITIVE_KIND_I8 },
    { "i16", TYPE_PRIMITIVE_KIND_I16 },
    { "i32", TYPE_PRIMITIVE_KIND_I32 },
    { "i64", TYPE_PRIMITIVE_KIND_I64 },
    { "isize", TYPE_PRIMITIVE_KIND_ISIZE },
    { "bool", TYPE_PRIMITIVE_KIND_BOOL },
    { "void", TYPE_PRIMITIVE_KIND_VOID },
};

TypePrimitiveKind primitive_type_str_to_kind(char* name) {
    for (u64 i = 0; i < stack_arr_len(PRIMITIVE_TYPES); i++) {
        if (stri(PRIMITIVE_TYPES[i].str) == stri(name)) {
            return PRIMITIVE_TYPES[i].kind;
        }
    }
    return TYPE_PRIMITIVE_KIND_NONE;
}

char* primitive_type_kind_to_str(TypePrimitiveKind kind) {
    for (u64 i = 0; i < stack_arr_len(PRIMITIVE_TYPES); i++) {
        if (PRIMITIVE_TYPES[i].kind == kind) {
            return PRIMITIVE_TYPES[i].str;
        }
    }
    assert(0);
    return null;
}

int primitive_type_size(TypePrimitiveKind kind) {
    switch (kind) {
        case TYPE_PRIMITIVE_KIND_U8:
        case TYPE_PRIMITIVE_KIND_I8: return 1;

        case TYPE_PRIMITIVE_KIND_U16:
        case TYPE_PRIMITIVE_KIND_I16: return 2;

        case TYPE_PRIMITIVE_KIND_U32:
        case TYPE_PRIMITIVE_KIND_I32: return 4;

        case TYPE_PRIMITIVE_KIND_U64:
        case TYPE_PRIMITIVE_KIND_I64: return 8;

        case TYPE_PRIMITIVE_KIND_USIZE:
        case TYPE_PRIMITIVE_KIND_ISIZE: return 8;

        case TYPE_PRIMITIVE_KIND_BOOL: return 8;

        case TYPE_PRIMITIVE_KIND_VOID: return 0;
        case TYPE_PRIMITIVE_KIND_NONE: break;
    }
    assert(0);
    return 0;
}

bool primitive_type_is_signed(TypePrimitiveKind kind) {
    switch (kind) {
        case TYPE_PRIMITIVE_KIND_U8:
        case TYPE_PRIMITIVE_KIND_U16:
        case TYPE_PRIMITIVE_KIND_U32:
        case TYPE_PRIMITIVE_KIND_U64:
        case TYPE_PRIMITIVE_KIND_USIZE: return false;

        case TYPE_PRIMITIVE_KIND_I8:
        case TYPE_PRIMITIVE_KIND_I16:
        case TYPE_PRIMITIVE_KIND_I32:
        case TYPE_PRIMITIVE_KIND_I64:
        case TYPE_PRIMITIVE_KIND_ISIZE: return true;

        case TYPE_PRIMITIVE_KIND_BOOL:
        case TYPE_PRIMITIVE_KIND_VOID: 
        case TYPE_PRIMITIVE_KIND_NONE: assert(0); break;
    }
    return false;
}

bool primitive_type_is_integer(TypePrimitiveKind kind) {
    switch (kind) {
        case TYPE_PRIMITIVE_KIND_U8:
        case TYPE_PRIMITIVE_KIND_U16:
        case TYPE_PRIMITIVE_KIND_U32:
        case TYPE_PRIMITIVE_KIND_U64:
        case TYPE_PRIMITIVE_KIND_USIZE:
        case TYPE_PRIMITIVE_KIND_I8:
        case TYPE_PRIMITIVE_KIND_I16:
        case TYPE_PRIMITIVE_KIND_I32:
        case TYPE_PRIMITIVE_KIND_I64:
        case TYPE_PRIMITIVE_KIND_ISIZE: return true;

        case TYPE_PRIMITIVE_KIND_BOOL: 
        case TYPE_PRIMITIVE_KIND_VOID: return false;
        case TYPE_PRIMITIVE_KIND_NONE: assert(0); break;
    }
    return false;
}

char* type_to_str(Node* type) {
    char* buf = null;
    switch (type->kind) {
        case NODE_KIND_TYPE_PRIMITIVE:
        {
            buf_write(
                    buf, 
                    primitive_type_kind_to_str(type->type_primitive.kind));
        } break;

        case NODE_KIND_TYPE_CUSTOM:
        {
            buf_write(
                    buf, 
                    type->type_custom.symbol->symbol.identifier->lexeme);
        } break;

        case NODE_KIND_TYPE_PTR:
        {
            buf_push(buf, '*');
            if (type->type_ptr.constant) {
                buf_write(
                        buf,
                        "const ");
            }
            buf_write(
                    buf, 
                    type_to_str(type->type_ptr.right));
        } break;

        default: assert(0); break;
    }
    buf_push(buf, '\0');
    return buf;
}

void stderr_print_type(Node* type) {
    char* type_str = type_to_str(type);
    fprintf(stderr, "%s", type_str);
    buf_free(type_str);
}

int type_get_size_bytes(Node* type) {
    switch (type->kind) {
        case NODE_KIND_TYPE_PTR:
            return PRIMITIVE_TYPE_PTR_SIZE;

        case NODE_KIND_TYPE_PRIMITIVE:
        {
            return primitive_type_size(type->type_primitive.kind);
        } break;

        case NODE_KIND_TYPE_CUSTOM: 
        {
            assert(0);
        } break;
    }
    assert(0);
    return 0;
}

typedef struct {
    Node* u8;
    Node* u16;
    Node* u32;
    Node* u64;
    Node* usize;
    Node* i8;
    Node* i16;
    Node* i32;
    Node* i64;
    Node* isize;
    Node* bool_;
    Node* void_;
} PrimitiveTypePlaceholders;

PrimitiveTypePlaceholders primitive_type_placeholders;

Node* primitive_type_new_placeholder(TypePrimitiveKind kind) {
    return node_type_primitive_new(null, kind);
}

void init_primitive_types() {
    primitive_type_placeholders.u8 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_U8);
    primitive_type_placeholders.u16 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_U16);
    primitive_type_placeholders.u32 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_U32);
    primitive_type_placeholders.u64 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_U64);
    primitive_type_placeholders.usize = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_USIZE);

    primitive_type_placeholders.i8 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_I8);
    primitive_type_placeholders.i16 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_I16);
    primitive_type_placeholders.i32 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_I32);
    primitive_type_placeholders.i64 = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_I64);
    primitive_type_placeholders.isize = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_ISIZE);

    primitive_type_placeholders.bool_ = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_BOOL);
    primitive_type_placeholders.void_ = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_VOID);
}
