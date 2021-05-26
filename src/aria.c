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
    TYPE_KIND_BASE,
    TYPE_KIND_PTR,
} TypeKind;

typedef enum {
    NODE_KIND_NUMBER,
    NODE_KIND_TYPE,
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
            TypeKind kind;
            union {
                struct {
                    Node* symbol;
                } base;

                struct {
                    Node* right;
                } ptr;
            };
        } type;

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
        } variable_decl;

        struct {
            Token* identifier;
            Node** params;
            Node* type;
            Node* body;
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

Node* node_type_base_new(
        Node* symbol) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE;
    node->head = symbol->head;
    node->type.kind = TYPE_KIND_BASE;
    node->type.base.symbol = symbol;
    node->tail = symbol->tail;
    return node;
}

Node* node_type_ptr_new(
        Token* star,
        Node* right) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE;
    node->head = star;
    node->type.kind = TYPE_KIND_PTR;
    node->type.ptr.right = right;
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
        Token* semicolon) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_EXPR_STMT;
    node->head = expr->head;
    node->expr_stmt.expr = expr;
    node->tail = semicolon;
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
        Token* semicolon) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_VARIABLE_DECL;
    node->head = keyword;
    node->variable_decl.mut = mut;
    node->variable_decl.identifier = identifier;
    node->variable_decl.type = type;
    node->variable_decl.initializer = initializer;
    // TODO: should we change the `tail` to 
    // the `tail` of the type (or the identifier 
    // before the colon)?
    node->tail = semicolon;
    return node;
}

Token* node_get_identifier(Node* node, bool assert_on_erroneous_node) {
    switch (node->kind) {
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_TYPE:
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
        case NODE_KIND_TYPE:
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

// This function is used to create tokens 
// apart from source file tokens. Mainly 
// used to instantiate builtin types, intrinsics,
// directives, etc.
Token* token_from_string(char* identifier) {
    return token_alloc(
            TOKEN_KIND_IDENTIFIER,
            identifier,
            null,
            null,
            null,
            0,
            0,
            0);
}

// A separate buffer for builtin types apart from
// the globals is created, to loop over them.
Node** builtin_types = null;
Node* u8_type;
Node* u16_type;
Node* u32_type;
Node* u64_type;
Node* usize_type;
Node* i8_type;
Node* i16_type;
Node* i32_type;
Node* i64_type;
Node* isize_type;
Node* void_type;

Node* builtin_type_base_from_string(char* identifier) {
    Node* type = node_type_base_new( 
            node_symbol_new(
                token_from_string(identifier)));
    // This pushes the newly-created type to the 
    // `builtin_types` buffer so that I don't forget
    // to update it in `init_builtin_types`.
    buf_push(builtin_types, type);
    return type;
}

void init_builtin_types() {
    u8_type = builtin_type_base_from_string("u8");
    u16_type = builtin_type_base_from_string("u16");
    u32_type = builtin_type_base_from_string("u32");
    u64_type = builtin_type_base_from_string("u64");
    usize_type = builtin_type_base_from_string("usize");
    i8_type = builtin_type_base_from_string("i8");
    i16_type = builtin_type_base_from_string("i16");
    i32_type = builtin_type_base_from_string("i32");
    i64_type = builtin_type_base_from_string("i64");
    isize_type = builtin_type_base_from_string("isize");
    void_type = builtin_type_base_from_string("void");
}

