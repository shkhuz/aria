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
    TYPE_KIND_IDENTIFIER,
} TypeKind;

typedef enum {
    NODE_KIND_TYPE,
    NODE_KIND_BLOCK,
    NODE_KIND_VARIABLE_DECL,
    NODE_KIND_PROCEDURE_DECL,
} NodeKind;

struct Node {
    NodeKind kind;
    Token* head;
    Token* tail;

    union {
        struct {
            TypeKind kind;
            union {
                Token* identifier;
            };
        } type;

        struct {
            Node** nodes;
            // TODO: add node expression 
        } block;

        struct {
            bool mut;
            Token* identifier;
            Node* type;
        } variable_decl;

        struct {
            Token* identifier;
            Node** params;
            Node* type;
            Node* body;
        } procedure_decl;
    };
};

Node* node_type_identifier_new(
        Token* identifier) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_TYPE;
    node->head = identifier;
    node->type.kind = TYPE_KIND_IDENTIFIER;
    node->type.identifier = identifier;
    node->tail = identifier;
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
        Token* semicolon) {
    alloc_with_type(node, Node);
    node->kind = NODE_KIND_VARIABLE_DECL;
    node->head = keyword;
    node->variable_decl.mut = mut;
    node->variable_decl.identifier = identifier;
    node->variable_decl.type = type;
    // TODO: should we change the `tail` to 
    // the `tail` of the type (or the identifier 
    // before the colon)?
    node->tail = semicolon;
    return node;
}

Token* node_get_identifier(Node* node) {
    Token* identifier = null;
    switch (node->kind) {
        case NODE_KIND_TYPE:
        case NODE_KIND_BLOCK:
        {
            assert(0);
        } break;

        case NODE_KIND_VARIABLE_DECL:
        {
            identifier = node->variable_decl.identifier;
        } break;

        case NODE_KIND_PROCEDURE_DECL:
        {
            identifier = node->procedure_decl.identifier;
        } break;
    }
    return identifier;
}

struct SrcFile {
    File* contents;
    Token** tokens;
    Node** nodes;
};

/* typedef struct { */
/*  SrcFile* srcfile; */
/*  bool error; */
/*  u64 error_count; */
/* } Checker; */

/* void checker_init(Checker* self, SrcFile* srcfile); */
/* void checker_check(Checker* self); */
