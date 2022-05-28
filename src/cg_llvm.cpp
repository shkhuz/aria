typedef struct {
    Srcfile* srcfile;
    std::vector<std::string> outcode;
    std::string outpath;
    int indent;
} CgContext;

#define cg_line(c, fmtstr, ...) \
    c->outcode.push_back(fmt::format(fmtstr, ##__VA_ARGS__))

// To print a `Type` using fmt::print(), use `l` format specifier
// to convert the type into llvm's notation.
// Easy way to make sure: search for `{}` in vim and match all type
// outputs to have `l` format specifier.

void init_cg() {
    unfill_fmt_type_color();
}

void deinit_cg() {
    fill_fmt_type_color();
}

void cg_function_header(CgContext* c, FunctionHeader* header) {
    std::string out = fmt::format(
            "{} {} @{}(",
            header->is_extern ? "declare" : "define",
            *header->return_type,
            *header->identifier);
    for (size_t i = 0; i < header->params.size(); i++) {
        out += fmt::format(
                "{:l}", 
                *header->params[i]->param.type);
        if (!header->is_extern) {
            header->params[i]->param.id = id_next(header->id);
            out += fmt::format(
                    " %{}",
                    header->params[i]->param.id);
        }
        if (i+1 < header->params.size()) 
            out += ", ";
    }
    out += ")";
    if (!header->is_extern) out += " {{";
    cg_line(c, out);
}

void cg_function_stmt(CgContext* c, Stmt* stmt) {
    cg_function_header(c, stmt->function.header);
    if (!stmt->function.header->is_extern) { 
        cg_line(c, "entry:");
        for (Stmt* local: stmt->function.locals) {
            std::string fmt = "%{} = alloca {:l}, align 4";
            if (local->kind == STMT_KIND_PARAM) {
                size_t oldid = local->param.id;
                local->param.id = id_next(stmt->function.header->id);
                cg_line(c, fmt,
                        local->param.id, 
                        *local->param.type);
                cg_line(c, "store {:l} %{}, {:l}* %{}, align 4",
                        *local->param.type,
                        oldid,
                        *local->param.type,
                        local->param.id);
            }
            else if (local->kind == STMT_KIND_VARIABLE) {
                local->variable.id = id_next(stmt->function.header->id);
                cg_line(c, fmt,
                        local->variable.id, 
                        *local->variable.type);
            }
            else assert(0);
        }
        cg_line(c, "}}\n");
    }
}

void cg_stmt(CgContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            cg_function_stmt(c, stmt);
        } break;
    }
}

void cg(CgContext* c, const std::string& outpath) {
    c->outpath = outpath;
    c->indent = 0;

    for (Stmt* stmt: c->srcfile->stmts) {
        cg_stmt(c, stmt);
    }

    for (std::string& s: c->outcode) {
        fmt::print("{}\n", s);
    }
}
