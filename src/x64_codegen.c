typedef struct {
    SrcFile* srcfile;
    char* code;
    int last_local_offset;
} CodeGenerator;

void codegenerator_node(CodeGenerator* self, Node* node);

void codegenerator_print_tab(CodeGenerator* self) {
    for (int i = 0; i < TAB_COUNT; i++) {
        buf_push(self->code, ' ');
    }
}

#define asml(l) \
    buf_printf(self->code, "%s:\n", l); \

#define asmp(fmt, ...) \
    codegenerator_print_tab(self); \
    buf_printf(self->code, fmt, ##__VA_ARGS__); \
    buf_push(self->code, '\n')

#define asmw(str) \
    codegenerator_print_tab(self); \
    buf_write(self->code, str); \
    buf_push(self->code, '\n')

void codegenerator_number(CodeGenerator* self, Node* node) {
    asmp("mov rax, %s", node->number.number->lexeme);
}

void codegenerator_cast(CodeGenerator* self, Node* node) {
    codegenerator_node(self, node->cast.left);
}

void codegenerator_block(CodeGenerator* self, Node* node) {
    buf_loop(node->block.nodes, i) {
        codegenerator_node(self, node->block.nodes[i]);
    }
    if (node->block.return_value) {
        codegenerator_node(self, node->block.return_value);
    }
}

char* codegenerator_get_asm_type_specifier(int bytes) {
    switch (bytes) {
        case 1: return "byte ptr";
        case 2: return "word ptr";
        case 4: return "dword ptr";
        case 8: return "qword ptr";
    }
    assert(0);
    return null;
}

void codegenerator_return(CodeGenerator* self, Node* node) {
    codegenerator_node(self, node->return_.right);
    asmw("jmp .L_ret");
}

void codegenerator_variable_decl(CodeGenerator* self, Node* node) {
    if (node->variable_decl.initializer) {
        int bytes = type_get_size_bytes(node->variable_decl.type);
        codegenerator_node(self, node->variable_decl.initializer);
        asmp(
                "mov %s [rbp - %lu], rax", 
                codegenerator_get_asm_type_specifier(bytes),
                self->last_local_offset + bytes);
        self->last_local_offset += bytes;
    }

}

void codegenerator_procedure_decl(CodeGenerator* self, Node* node) {
    asmp(
            "global %s", 
            node->procedure_decl.header->procedure_header.identifier->lexeme);
    asml(
            node->procedure_decl.header->procedure_header.identifier->lexeme);
    asmw("push rbp");
    asmw("mov rbp, rsp");

    int local_vars_bytes_align_16 = 0;
    if (node->procedure_decl.local_vars_bytes) {
        local_vars_bytes_align_16 = 
            round_to_next_multiple(node->procedure_decl.local_vars_bytes, 16);

        asmp("sub rsp, %lu", local_vars_bytes_align_16);
    }
    codegenerator_block(self, node->procedure_decl.body);

    if (node->procedure_decl.local_vars_bytes) {
        asmp("add rsp, %lu", local_vars_bytes_align_16);
    }
    asml(".L_ret");
    asmw("pop rbp");
    asmw("ret");
    asmw("");
}

void codegenerator_extern_procedure(CodeGenerator* self, Node* node) {
    asmp(
            "extern %s", 
            node->extern_procedure.header->
                procedure_header.identifier->lexeme);
    asmw("");
}

void codegenerator_node(CodeGenerator* self, Node* node) {
    switch (node->kind) {
        case NODE_KIND_PROCEDURE_DECL: 
        {
            codegenerator_procedure_decl(self, node);
        } break;

        case NODE_KIND_EXTERN_PROCEDURE:
        {
            codegenerator_extern_procedure(self, node);
        } break;

        case NODE_KIND_VARIABLE_DECL:
        {
            codegenerator_variable_decl(self, node);
        } break;

        case NODE_KIND_RETURN:
        {
            codegenerator_return(self, node);
        } break;

        case NODE_KIND_BLOCK:
        {
            codegenerator_block(self, node);
        } break;

        case NODE_KIND_CAST:
        {
            codegenerator_cast(self, node);
        } break;

        case NODE_KIND_NUMBER:
        {
            codegenerator_number(self, node);
        } break;
    }
}

void codegenerator_init(CodeGenerator* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->code = null;
    self->last_local_offset = 0;
}

void codegenerator_gen(CodeGenerator* self) {
    buf_loop(self->srcfile->nodes, i) {
        codegenerator_node(self, self->srcfile->nodes[i]);
    }
    buf_push(self->code, '\0');

    printf("%s\n", self->code);
}

#undef asml
#undef asmp
#undef asmw
