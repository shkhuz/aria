typedef struct {
    Srcfile* srcfile;
    std::string tmpdir;
    std::string objpath;
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
                case BUILTIN_TYPE_KIND_USIZE:
                case BUILTIN_TYPE_KIND_I8:
                case BUILTIN_TYPE_KIND_I16:
                case BUILTIN_TYPE_KIND_I32:
                case BUILTIN_TYPE_KIND_I64:
                case BUILTIN_TYPE_KIND_ISIZE:
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

void cg_stmt(CgContext* c, Stmt* stmt);

LLVMValueRef cg_expr(CgContext* c, Expr* expr) {
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            return LLVMConstIntOfStringAndSize(
                    get_llvm_type(expr->type), 
                    expr->integer.integer->lexeme.c_str(),
                    expr->integer.integer->lexeme.size(),
                    10);
        } break;

        case EXPR_KIND_BLOCK: {
            for (Stmt* stmt: expr->block.stmts) {
                cg_stmt(c, stmt);
            } 
            if (expr->block.value) {
                return cg_expr(c, expr->block.value);
            }
            return null;
        } break;

        case EXPR_KIND_SYMBOL: {
            switch (expr->symbol.ref->kind) {
                case STMT_KIND_VARIABLE:
                    return expr->symbol.ref->variable.llvmvalue;
                case STMT_KIND_PARAM: 
                    return expr->symbol.ref->param.llvmvalue;
                default: assert(0);
            }
        } break;

        case EXPR_KIND_UNOP: {
            if (expr->unop.op->kind == TOKEN_KIND_STAR) {
                LLVMValueRef child = cg_expr(c, expr->unop.child);
                return LLVMBuildLoad2(
                        c->llvmbuilder,
                        // ptr type because alloca returns a ptr
                        LLVMPointerType(get_llvm_type(expr->type), 0),
                        child,
                        "");
            }
            else assert(0);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            assert(expr->function_call.callee->kind == EXPR_KIND_SYMBOL);
            assert(expr->function_call.callee->symbol.ref->kind == STMT_KIND_FUNCTION);
            LLVMValueRef funcval = expr->function_call.callee->symbol.ref->function.header->llvmvalue;
            LLVMValueRef* arg_values = null;
            std::vector<Expr*>& args = expr->function_call.args;
            size_t arg_count = args.size();
            if (arg_count != 0) {
                arg_values = (LLVMValueRef*)malloc(sizeof(LLVMValueRef) * arg_count);
                for (size_t i = 0; i < arg_count; i++) {
                    arg_values[i] = cg_expr(c, args[i]);
                }
            }
            // TODO: change to LLVMBuildCall2
            return LLVMBuildCall(
                    c->llvmbuilder,
                    funcval,
                    arg_values,
                    arg_count,
                    "");

        } break;
    }
    assert(0);
    return null;
}

void cg_function_stmt(CgContext* c, Stmt* stmt) {
    if (!stmt->function.header->is_extern) {
        LLVMValueRef function = stmt->function.header->llvmvalue;
        std::vector<Stmt*>& params = stmt->function.header->params;
        size_t param_count = params.size();
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
                param_allocs[i] = LLVMBuildAlloca(c->llvmbuilder, LLVMTypeOf(param_values[i]), "");
                params[i]->param.llvmvalue = param_allocs[i];
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
            LLVMSetAlignment(local_val, type_bytes(local->variable.type));
            local->variable.llvmvalue = local_val;
        }
        
        if (stmt->function.header->params.size() != 0) {
            for (size_t i = 0; i < param_count; i++) { 
                LLVMBuildStore(c->llvmbuilder, param_values[i], param_allocs[i]);
            }
        }

        LLVMValueRef body_val = cg_expr(c, stmt->function.body);
        if (body_val) LLVMBuildRet(c->llvmbuilder, body_val);
        else LLVMBuildRetVoid(c->llvmbuilder);
    }

    /* LLVMDumpValue(function); */
}

void cg_assign_value(CgContext* c, Expr* right, LLVMValueRef left, size_t align_to) {
    LLVMValueRef rightval = cg_expr(c, right);
    LLVMValueRef res = LLVMBuildStore(c->llvmbuilder, rightval, left);
    LLVMSetAlignment(res, align_to);
}

void cg_variable_stmt(CgContext* c, Stmt* stmt) {
    if (stmt->variable.initializer) {
        cg_assign_value(
                c, 
                stmt->variable.initializer,
                stmt->variable.llvmvalue,
                type_bytes(stmt->variable.type));
    }
}

void cg_stmt(CgContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            cg_function_stmt(c, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            cg_variable_stmt(c, stmt);
        } break;

        case STMT_KIND_ASSIGN: {
            cg_assign_value(
                    c,
                    stmt->assign.right,
                    cg_expr(c, stmt->assign.left),
                    type_bytes(stmt->assign.left_type));
        } break;

        case STMT_KIND_EXPR: {
            cg_expr(c, stmt->expr.child);
        } break;
    }
}

void cg_top_level(CgContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
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
            stmt->function.header->llvmvalue = function;
        } break;

        case STMT_KIND_VARIABLE: {
            assert(0);
        } break;

        default: assert(0);
    }
}

// mandatory `/` after directory name
void mkdir_p(const std::string& path) {
    assert(path.size() != 0);
    size_t start = 0;
    while (path[start] == '/') start++;
    start--;

    size_t idx = start;
    size_t pathlen = path.size();

    while (idx != pathlen) {
        if (path[idx] == '/') {
            std::string subpath = path.substr(start, idx);
            mkdir(subpath.c_str(), 0777);
        }
        idx++;
    }
}

void cg(CgContext* c) {
    c->llvmctx = LLVMContextCreate();
    c->llvmbuilder = LLVMCreateBuilderInContext(c->llvmctx);
    c->llvmmod = LLVMModuleCreateWithName(c->srcfile->handle->path.c_str());

    for (Stmt* stmt: c->srcfile->stmts) {
        cg_top_level(c, stmt);
    }
    for (Stmt* stmt: c->srcfile->stmts) {
        cg_stmt(c, stmt);
    }
    
    char* errors = null;
    LLVMDumpModule(c->llvmmod);
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    LLVMTargetRef target;
    LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &errors);
    /* printf("error: %s\n", errors); */
    LLVMDisposeMessage(errors);
    /* printf("target: %s, [%s], %d, %d\n", LLVMGetTargetName(target), LLVMGetTargetDescription(target), LLVMTargetHasJIT(target), LLVMTargetHasTargetMachine(target)); */
    /* printf("triple: %s\n", LLVMGetDefaultTargetTriple()); */
    /* printf("features: %s\n", LLVMGetHostCPUFeatures()); */
    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, LLVMGetDefaultTargetTriple(), "generic", LLVMGetHostCPUFeatures(), LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    LLVMSetTarget(c->llvmmod, LLVMGetDefaultTargetTriple());
    LLVMTargetDataRef datalayout = LLVMCreateTargetDataLayout(machine);
    char* datalayout_str = LLVMCopyStringRepOfTargetData(datalayout);
    /* printf("datalayout: %s\n", datalayout_str); */
    LLVMSetDataLayout(c->llvmmod, datalayout_str);
    LLVMDisposeMessage(datalayout_str);

    std::string objpath = fmt::format(
            "{}{}.o", 
            c->tmpdir, 
            c->srcfile->handle->path, 
            ".o");
    c->objpath = objpath;
    /* std::cout << "path: " << objpath << std::endl; */
    mkdir_p(objpath);
    LLVMTargetMachineEmitToFile(machine, c->llvmmod, (char*)objpath.c_str(), LLVMObjectFile, &errors);
    /* printf("error: %s\n", errors); */
    LLVMDisposeMessage(errors);
}
