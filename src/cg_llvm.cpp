typedef struct {
    Srcfile* srcfile;
    std::string tmpdir;
    std::string objpath;
    bool error;
    // TODO: pass this ctx to llvm functions
    LLVMContextRef llvmctx;
    LLVMBuilderRef llvmbuilder;
    LLVMModuleRef llvmmod;
} CgContext;

static LLVMTargetRef g_llvmtarget;
static LLVMTargetMachineRef g_llvmtargetmachine;
static LLVMTargetDataRef g_llvmtargetdatalayout;
static char* g_llvmtargetdatalayout_str;

// To print a `Type` using fmt::print(), use `l` format specifier
// to convert the type into llvm's notation.
// Easy way to make sure: search for `{}` in vim and match all type
// outputs to have `l` format specifier.

// TODO: store LLVMValueRef from exprs, variables, ... into their corresponding
// ds (for eg. LLVMValueRef variable -> VariableStmt

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

LLVMValueRef cg_expr(CgContext* c, Expr* expr, Type* target, bool lvalue) {
    LLVMValueRef result = null;
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            result = LLVMConstIntOfStringAndSize(
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
                result = cg_expr(c, expr->block.value, target, false);
            }
            else result = null;
        } break;

        case EXPR_KIND_SYMBOL: {
            LLVMTypeRef ty = null;
            switch (expr->symbol.ref->kind) {
                case STMT_KIND_VARIABLE: {
                    ty = get_llvm_type(expr->symbol.ref->variable.type);
                    result = expr->symbol.ref->variable.llvmvalue;
                } break;
                
                case STMT_KIND_PARAM: {
                    ty = get_llvm_type(expr->symbol.ref->param.type);
                    result = expr->symbol.ref->param.llvmvalue;
                } break;
               
                default: assert(0);
            }
            
            if (!lvalue) {
                result = LLVMBuildLoad2(c->llvmbuilder, ty, result, "");
            }
        } break;

        case EXPR_KIND_UNOP: {
            if (expr->unop.op->kind == TOKEN_KIND_STAR) {
                LLVMValueRef child = cg_expr(c, expr->unop.child, null, false);
                result = LLVMBuildLoad2(
                        c->llvmbuilder,
                        get_llvm_type(expr->type),
                        child,
                        "");
            }
            else if (expr->unop.op->kind == TOKEN_KIND_MINUS) {
                if (expr->unop.folded) {
                    // TODO: same code in BINOP: refactor please
                    assert(expr->type->kind == TYPE_KIND_BUILTIN);
                    bigint* apint = &expr->type->builtin.apint;
                    union { u64 pos; i64 neg; } val;
                    val.pos = bigint_get_lsd(apint);
                    result = LLVMConstInt(
                            get_llvm_type(expr->type), 
                            apint->sign == BIGINT_SIGN_NEG ? -val.neg : val.pos,
                            true);
                }
                else {
                    LLVMValueRef child = cg_expr(c, expr->unop.child, null, false);
                    result = LLVMBuildNeg(c->llvmbuilder, child, "");
                }
            }
            else if (expr->unop.op->kind == TOKEN_KIND_AMP) {
                result = cg_expr(c, expr->unop.child, null, true);
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
                    arg_values[i] = cg_expr(c, args[i], expr->function_call.callee->symbol.ref->function.header->params[i]->param.type, false);
                }
            }
            // TODO: change to LLVMBuildCall2
            result = LLVMBuildCall(
                    c->llvmbuilder,
                    funcval,
                    arg_values,
                    arg_count,
                    "");

        } break;

        case EXPR_KIND_BINOP: {
            if (expr->binop.folded) {
                assert(expr->type->kind == TYPE_KIND_BUILTIN);
                bigint* apint = &expr->type->builtin.apint;
                union { u64 pos; i64 neg; } val;
                val.pos = bigint_get_lsd(apint);
                result = LLVMConstInt(
                        get_llvm_type(expr->type), 
                        apint->sign == BIGINT_SIGN_NEG ? -val.neg : val.pos,
                        true);
            }
            else {
                Type* left_target_ty = null;
                Type* right_target_ty = null;
                if (type_bytes(expr->binop.left_type) > type_bytes(expr->binop.right_type)) {
                    right_target_ty = expr->binop.left_type;
                }
                else {
                    left_target_ty = expr->binop.right_type;
                }
                LLVMValueRef left = cg_expr(c, expr->binop.left, left_target_ty, false);
                LLVMValueRef right = cg_expr(c, expr->binop.right, right_target_ty, false);
                if (token_is_cmp_op(expr->binop.op)) {
                    assert(0);
                }
                else {
                    LLVMOpcode op;
                    switch (expr->binop.op->kind) {
                        case TOKEN_KIND_PLUS: op = LLVMAdd; break;
                        case TOKEN_KIND_MINUS: op = LLVMSub; break;
                        default: assert(0);
                    }
                    result = LLVMBuildBinOp(c->llvmbuilder, op, left, right, "");
                }
            }
        } break;

        default: assert(0); break;
    }
    
    if (target &&
        type_is_integer(target) && 
        type_is_integer(expr->type)) {
        if (type_bytes(target) > type_bytes(expr->type)) {
            if (builtin_type_is_signed(target->builtin.kind)) {
                result = LLVMBuildSExt(
                        c->llvmbuilder, 
                        result, 
                        get_llvm_type(target),
                        "");
            }
            else {
                result = LLVMBuildZExt(
                        c->llvmbuilder, 
                        result, 
                        get_llvm_type(target),
                        "");
            }
        }
    }
    return result;
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
        
        if (params.size() != 0) {
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
        
        if (params.size() != 0) {
            for (size_t i = 0; i < param_count; i++) { 
                LLVMBuildStore(c->llvmbuilder, param_values[i], param_allocs[i]);
            }
        }

        LLVMValueRef body_val = cg_expr(c, stmt->function.body, stmt->function.header->return_type, false);
        if (body_val) LLVMBuildRet(c->llvmbuilder, body_val);
        else LLVMBuildRetVoid(c->llvmbuilder);
    }

    /* LLVMDumpValue(function); */
}

void cg_assign_value(CgContext* c, Type* left_ty, Type* right_ty, Expr* left, Expr* right, bool left_lvalue, size_t align_to) {
    // TODO: same code in BINOP: refactor please
    Type* left_target_ty = null;
    Type* right_target_ty = null;
    if (type_bytes(left_ty) > type_bytes(right_ty)) {
        right_target_ty = left_ty;
    }
    else {
        left_target_ty = right_ty;
    }
    LLVMValueRef leftval = cg_expr(c, left, left_target_ty, left_lvalue);
    LLVMValueRef rightval = cg_expr(c, right, right_target_ty, false);
    LLVMValueRef res = LLVMBuildStore(c->llvmbuilder, rightval, leftval);
    LLVMSetAlignment(res, align_to);
}

void cg_variable_stmt(CgContext* c, Stmt* stmt) {
    if (stmt->variable.initializer) {
        LLVMValueRef rightval = cg_expr(c, stmt->variable.initializer, stmt->variable.type, false);
        LLVMValueRef res = LLVMBuildStore(c->llvmbuilder, rightval, stmt->variable.llvmvalue);
        LLVMSetAlignment(res, type_bytes(stmt->variable.type));
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
            if (stmt->assign.left->kind == EXPR_KIND_UNOP &&
                stmt->assign.left->unop.op->kind == TOKEN_KIND_STAR) {
                cg_assign_value(
                        c,
                        stmt->assign.left_type,
                        stmt->assign.right_type,
                        stmt->assign.left->unop.child,
                        stmt->assign.right,
                        false,
                        type_bytes(stmt->assign.left_type));
            } 
            else {
                cg_assign_value(
                        c,
                        stmt->assign.left_type,
                        stmt->assign.right_type, 
                        stmt->assign.left,
                        stmt->assign.right,
                        true,
                        type_bytes(stmt->assign.left_type));
            }
        } break;

        case STMT_KIND_EXPR: {
            cg_expr(c, stmt->expr.child, builtin_type_placeholders.void_kind, false);
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

bool init_cg() {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    char* errors = null;
    bool error = LLVMGetTargetFromTriple(
            LLVMGetDefaultTargetTriple(), 
            &g_llvmtarget, 
            &errors);
    if (error) {
        root_error(
                "cannot get LLVM target from target triple: {} ({})", 
                LLVMGetDefaultTargetTriple(),
                errors);
    }
    LLVMDisposeMessage(errors);
    errors = null;
    if (error) return true;

    printf(
            "-------- MACHINE INFO --------\n"
            "target: %s, [%s], %d, %d\n", 
            LLVMGetTargetName(g_llvmtarget), 
            LLVMGetTargetDescription(g_llvmtarget), 
            LLVMTargetHasJIT(g_llvmtarget), 
            LLVMTargetHasTargetMachine(g_llvmtarget));
    printf("host triple: %s\n", LLVMGetDefaultTargetTriple());
    /* printf("features: %s\n", LLVMGetHostCPUFeatures()); */
    g_llvmtargetmachine = LLVMCreateTargetMachine(
            g_llvmtarget, 
            LLVMGetDefaultTargetTriple(), 
            "generic", 
            LLVMGetHostCPUFeatures(), 
            LLVMCodeGenLevelDefault, 
            LLVMRelocDefault, 
            LLVMCodeModelDefault);
    g_llvmtargetdatalayout = LLVMCreateTargetDataLayout(g_llvmtargetmachine);
    g_llvmtargetdatalayout_str = LLVMCopyStringRepOfTargetData(g_llvmtargetdatalayout);
    printf("datalayout: %s\n", g_llvmtargetdatalayout_str);
    return false;
}

void deinit_cg() {
    LLVMDisposeMessage(g_llvmtargetdatalayout_str);
}

void cg(CgContext* c) {
    c->error = false;
    c->llvmctx = LLVMContextCreate();
    c->llvmbuilder = LLVMCreateBuilderInContext(c->llvmctx);
    c->llvmmod = LLVMModuleCreateWithName(c->srcfile->handle->path.c_str());

    for (Stmt* stmt: c->srcfile->stmts) {
        cg_top_level(c, stmt);
    }
    for (Stmt* stmt: c->srcfile->stmts) {
        cg_stmt(c, stmt);
    }
    LLVMDumpModule(c->llvmmod);
    
    bool error = false;
    char* errors = null;

    error = LLVMVerifyModule(c->llvmmod, LLVMReturnStatusAction, &errors);
    if (error) {
        root_error(
                "[internal] IR generated by the compiler is invalid: \n{}{}{}"
                "\n> Please file a bug at github.com/shkhuz/aria/issues", 
                g_fcyann_color,
                errors,
                g_reset_color);
    }
    LLVMDisposeMessage(errors);
    errors = null;
    if (error) { 
        c->error = true;
        return;
    }

    LLVMPassManagerRef pm = LLVMCreatePassManager();
    LLVMAddPromoteMemoryToRegisterPass(pm);
    LLVMRunPassManager(pm, c->llvmmod);
    fmt::print(stderr, "\n-------- AFTER PASSES --------\n");
    LLVMDumpModule(c->llvmmod);
   
    LLVMSetTarget(c->llvmmod, LLVMGetDefaultTargetTriple());
    LLVMSetDataLayout(c->llvmmod, g_llvmtargetdatalayout_str);

    std::string objpath = fmt::format(
            "{}{}.o", 
            c->tmpdir, 
            c->srcfile->handle->path, 
            ".o");
    c->objpath = objpath;
    // TODO: if LLVMTargetMachineEmitToFile() fails, do something 
    // about the newly created directories
    mkdir_p(objpath);

    error = LLVMTargetMachineEmitToFile(
            g_llvmtargetmachine, 
            c->llvmmod, 
            (char*)objpath.c_str(),
            LLVMObjectFile, 
            &errors);
    if (error) {
        root_error("cannot output object file {}: {}", objpath, errors);
    }
    LLVMDisposeMessage(errors);
    errors = null;
    if (error) {
        c->error = true;
        return;
    }
    
    return;
}
