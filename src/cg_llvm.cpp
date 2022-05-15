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
    g_fmt_type_highlight = "";
    g_fmt_type_reset_highlight = "";
}

void deinit_cg() {
    g_fmt_type_highlight = g_fcyan_color;
    g_fmt_type_reset_highlight = g_reset_color;
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
    cg_line(c, out);
}

void cg_function_stmt(CgContext* c, Stmt* stmt) {
    cg_function_header(c, stmt->function.header);
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
