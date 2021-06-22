typedef struct Node Node;
typedef struct SrcFile SrcFile;

#define PROCEDURE_DECL_KEYWORD "proc"
#define VARIABLE_DECL_KEYWORD "let"

char* keywords[] = {
    "module",
    "struct",
    PROCEDURE_DECL_KEYWORD,
    VARIABLE_DECL_KEYWORD,
    "mut",
    "const",
};

char* directives[] = {
    "@import",
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
    TYPE_PRIMITIVE_KIND_VOID,
    TYPE_PRIMITIVE_KIND_NONE,
} TypePrimitiveKind;

#define PRIMITIVE_TYPE_PTR_SIZE 8

typedef enum {
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_KEYWORD,
    TOKEN_KIND_DIRECTIVE,
    TOKEN_KIND_STRING,
    TOKEN_KIND_NUMBER_8,
    TOKEN_KIND_NUMBER_16,
    TOKEN_KIND_NUMBER_32,
    TOKEN_KIND_NUMBER_64,
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
    *token = (Token) {
        kind,
        lexeme,
        start,
        end,
        srcfile,
        line,
        column,
        char_count
    };
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
    NODE_KIND_TYPE_PRIMITIVE,
    NODE_KIND_TYPE_CUSTOM,
    NODE_KIND_TYPE_PTR,
    NODE_KIND_SYMBOL,
    NODE_KIND_PROCEDURE_CALL,
    NODE_KIND_BLOCK,
    NODE_KIND_PARAM,
    NODE_KIND_EXPR_STMT,
    NODE_KIND_VARIABLE_DECL,
    NODE_KIND_PROCEDURE_DECL,
} NodeKind;

struct Node {
    NodeKind kind;
    Token* head;
    Token* tail;

    union {
        struct {
            Token* number;
        } number;

        struct {
            Token* token;
            TypePrimitiveKind kind;
        } type_primitive;

        struct {
            Node* symbol;
        } type_custom;

        struct {
            Node* right;
        } type_ptr;

        // A symbol is an identifier
        // with an optional `::` accessor
        // before it.
        // TODO: implement `::`
        struct {
            Token* identifier;
            Node* ref;
        } symbol;

        struct {
            Node* callee;
            Node** args;
        } procedure_call;

        struct {
            Node** nodes;
            // TODO: add node expression 
        } block;

        struct {
            Token* identifier;
            Node* type;
        } param;

        struct {
            Node* expr;
        } expr_stmt;

        struct {
            bool mut;
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
    };
};

Node* node_number_new(
        Token* number) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_NUMBER;
    node->head = number;
    node->number.number = number;
    node->tail = number;
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
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE_PTR;
    node->head = star;
    node->type_ptr.right = right;
    node->tail = right->tail;
    return node;
}

Node* node_symbol_new(
        Token* identifier) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_SYMBOL;
    node->head = identifier;
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
        Token* rbrace) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_BLOCK;
    node->head = lbrace;
    node->block.nodes = nodes;
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

Node* node_expr_stmt_new(
        Node* expr,
        Token* tail) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_EXPR_STMT;
    node->head = expr->head;
    node->expr_stmt.expr = expr;
    node->tail = tail;
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

Node* node_variable_decl_new(
        Token* keyword,
        bool mut, 
        Token* identifier,
        Node* type,
        Node* initializer,
        Token* semicolon,
        bool in_procedure) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_VARIABLE_DECL;
    node->head = keyword;
    node->variable_decl.mut = mut;
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

Token* node_get_identifier(Node* node, bool assert_on_erroneous_node) {
    switch (node->kind) {
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_TYPE_PRIMITIVE:
		case NODE_KIND_TYPE_CUSTOM:
		case NODE_KIND_TYPE_PTR:
        case NODE_KIND_BLOCK:
        case NODE_KIND_NUMBER:
        // TODO: check if these expressions should 
        // return an identifier...
        case NODE_KIND_SYMBOL:
        case NODE_KIND_PROCEDURE_CALL:
        {
            if (assert_on_erroneous_node) {
                assert(0);
            }
        } break;

        case NODE_KIND_VARIABLE_DECL:
            return node->variable_decl.identifier;
        case NODE_KIND_PARAM:
            return node->param.identifier;
        case NODE_KIND_PROCEDURE_DECL:
            return node->procedure_decl.identifier;
    }
    return null;
}

Node* node_get_type(Node* node, bool assert_on_erroneous_node) {
    switch (node->kind) {
        case NODE_KIND_EXPR_STMT:
        // TODO: check if these expressions should 
        // return a type...
        case NODE_KIND_TYPE_PRIMITIVE:
        case NODE_KIND_TYPE_CUSTOM:
        case NODE_KIND_TYPE_PTR:
        case NODE_KIND_BLOCK:
        case NODE_KIND_NUMBER:
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

struct SrcFile {
    File* contents;
    Token** tokens;
    Node** nodes;
};

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

        case TYPE_PRIMITIVE_KIND_VOID: return 0;
        case TYPE_PRIMITIVE_KIND_NONE: break;
    }
    assert(0);
    return 0;
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

    primitive_type_placeholders.void_ = 
        primitive_type_new_placeholder(TYPE_PRIMITIVE_KIND_VOID);
}
