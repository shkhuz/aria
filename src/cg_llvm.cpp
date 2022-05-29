#include <llvm-c/Core.h>

typedef struct {
    Srcfile* srcfile;
    std::string outpath;
    // TODO: pass this ctx to llvm functions
    LLVMContextRef llvmctx;
    LLVMBuilderRef llvmbuilder;
    LLVMModuleRef llvmmod;
} CgContext;

// To print a `Type` using fmt::print(), use `l` format specifier
// to convert the type into llvm's notation.
// Easy way to make sure: search for `{}` in vim and match all type
// outputs to have `l` format specifier.

// TODO: store LLVMValueRef from exprs, variables, ... into their corresponding
// ds (for eg. LLVMValueRef variable -> VariableStmt

void init_cg() {
    unfill_fmt_type_color();
}

void deinit_cg() {
    fill_fmt_type_color();
}

LLVMTypeRef get_llvm_type(Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
            switch (type->builtin.kind) {
                case BUILTIN_TYPE_KIND_U8:
                case BUILTIN_TYPE_KIND_U16:
                case BUILTIN_TYPE_KIND_U32:
                case BUILTIN_TYPE_KIND_U64:
                case BUILTIN_TYPE_KIND_I8:
                case BUILTIN_TYPE_KIND_I16:
                case BUILTIN_TYPE_KIND_I32:
                case BUILTIN_TYPE_KIND_I64:
                    return LLVMIntType(builtin_type_bytes(&type->builtin) << 3);
                case BUILTIN_TYPE_KIND_BOOLEAN:
                    return LLVMInt1Type();
                case BUILTIN_TYPE_KIND_VOID:
                    return LLVMVoidType();
            } 
        } break;

        case TYPE_KIND_PTR: {
            return LLVMPointerType(get_llvm_type(type->ptr.child), 0);
        } break;
    }
    assert(0);
    return null;
}

void cg_function_stmt(CgContext* c, Stmt* stmt) {
    std::vector<Stmt*>& params = stmt->function.header->params;
    LLVMTypeRef return_type = get_llvm_type(stmt->function.header->return_type);
    LLVMTypeRef* param_types = null;
    size_t param_count = params.size();
    if (param_count != 0) {
        param_types = (LLVMTypeRef*)malloc(sizeof(LLVMTypeRef) * param_count);
        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = get_llvm_type(params[i]->param.type);
        }
    }
    LLVMTypeRef function_type = LLVMFunctionType(
            return_type,
            param_types,
            param_count,
            false);
    LLVMValueRef function = LLVMAddFunction(
            c->llvmmod, 
            stmt->function.header->identifier->lexeme.c_str(),
            function_type);

    if (!stmt->function.header->is_extern) {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlock(function, "entry");
        LLVMPositionBuilderAtEnd(c->llvmbuilder, entry);

        LLVMValueRef* param_allocs = null;
        LLVMValueRef* param_values = null;
        
        if (stmt->function.header->params.size() != 0) {
            param_allocs = (LLVMValueRef*)malloc(sizeof(LLVMValueRef) * param_count);
            param_values = (LLVMValueRef*)malloc(sizeof(LLVMValueRef) * param_count);
            LLVMGetParams(function, param_values);
            for (size_t i = 0; i < param_count; i++) { 
                LLVMSetValueName(
                        param_values[i], 
                        params[i]->param.identifier->lexeme.c_str());
                param_allocs[i] = LLVMBuildAlloca(c->llvmbuilder, param_types[i], "");
            }
        }

        // local variables
        std::vector<Stmt*>& locals = stmt->function.locals;
        for (Stmt* local: locals) {
            LLVMTypeRef local_type = get_llvm_type(local->variable.type);
            LLVMValueRef local_val = LLVMBuildAlloca(
                    c->llvmbuilder,
                    local_type,
                    local->variable.identifier->lexeme.c_str());
        }
        
        if (stmt->function.header->params.size() != 0) {
            for (size_t i = 0; i < param_count; i++) { 
                LLVMBuildStore(c->llvmbuilder, param_values[i], param_allocs[i]);
            }
        }
    }

    /* LLVMDumpValue(function); */
}

void cg_stmt(CgContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            cg_function_stmt(c, stmt);
        } break;
    }
}

LLVMValueRef cg_expr(CgContext* c, Expr* expr) {
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            return LLVMConstIntOfStringAndSize(
                    get_llvm_type(expr->type), 
                    expr->integer.integer->lexeme.c_str(),
                    expr->integer.integer->lexeme.size(),
                    10);
        } break;
    }
    assert(0);
    return null;
}

void cg(CgContext* c, const std::string& outpath) {
    c->outpath = outpath;
    c->llvmctx = LLVMContextCreate();
    c->llvmbuilder = LLVMCreateBuilderInContext(c->llvmctx);
    c->llvmmod = LLVMModuleCreateWithName(c->srcfile->handle->path.c_str());

    for (Stmt* stmt: c->srcfile->stmts) {
        cg_stmt(c, stmt);
    }
    LLVMDumpModule(c->llvmmod);
}
