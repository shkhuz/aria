struct Cg {
    Srcfile* srcfile;
    std::string code;

    Cg(Srcfile* srcfile) 
        : srcfile(srcfile) {
    }

    void push_tabs() {
        for (int i = 0; i < TAB_SIZE; i++) {
            this->code.append(" ");
        }
    }

    void asmlb(const std::string& label) {
        this->code.append(label);
        this->code.append(":\n");
    }

    void asmw(const std::string& str) {
        this->push_tabs();
        this->code.append(str);
        this->code.append("\n");
    }

    template<typename... Args>
    void asmp(const std::string& fmt, Args... args) {
        this->push_tabs();
        std::string fmtted = fmt::format(fmt, args...);
        this->code.append(fmtted);
        this->code.append("\n");
    }

    void function(Stmt* stmt) {
        asmp("global {}", stmt->function.header->identifier->lexeme);
        asmlb(stmt->function.header->identifier->lexeme);
        asmw("push rbp");
        asmw("mov rbp, rsp");

        printf("stack size: %d\n", stmt->function.stack_vars_size);
        int stack_vars_size_aln16 = 0;
        if (stmt->function.stack_vars_size != 0) {
            stack_vars_size_aln16 = 
                round_to_next_multiple(stmt->function.stack_vars_size, 16);
            asmp("sub rsp, {}", stack_vars_size_aln16);
        }

        if (stmt->function.stack_vars_size != 0) {
            asmp("add rsp, {}", stack_vars_size_aln16);
        }
        asmlb(".L_ret");
        asmw("ret");
        asmw("");
    }

    void stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::function: {
                this->function(stmt);
            } break;
        }
    }

    void run() {
        for (auto& stmt: this->srcfile->stmts) {
            this->stmt(stmt);
        }
        std::cout << code << std::endl;
    }
};
