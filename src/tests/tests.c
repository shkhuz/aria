#include "../cmd.h"
#include "../core.h"
#include "../buf.h"
#include "../msg.h"
#include "../compile.h"

typedef struct {
    MsgKind kind;
    const char* msg;
    OptionalSrcLoc srcloc;
} TestMsgSpec;

bool g_error = false;
usize total_tests = 0;
usize passed_tests = 0;

static void initialize_test(
    CompileCtx* out_test_ctx,
    Srcfile* srcfiles,
    usize test_call_line,
    const char* testname,
    const char* srccode)
{
    total_tests++;
#ifdef TEST_PRINT_COMPILER_MSGS
    fprintf(stderr, "\n");
#endif
    fprintf(stderr, "Testing \"%s\" (%lu)... ", testname, test_call_line);
#ifdef TEST_PRINT_COMPILER_MSGS
    fprintf(stderr, "\n");
#endif
    *srcfiles = (Srcfile){
        .handle = (File){
            .path = "<anon>",
            .abs_path = "<anon>",
            .contents = srccode,
            .len = strlen(srccode),
        },
        .tokens = NULL,
        .astnodes = NULL,
    };

    Srcfile* srcfiles_ptr = srcfiles;
    CompileCtx test_ctx = compile_new_context(NULL, NULL, false);
#ifdef TEST_PRINT_COMPILER_MSGS
    test_ctx.print_msg_to_stderr = true;
#else
    test_ctx.print_msg_to_stderr = false;
#endif
    Typespec* mod_ty = typespec_module_new(srcfiles_ptr);
    test_ctx.mod_tys = NULL;
    bufpush(test_ctx.mod_tys, mod_ty);

    read_srcfile("core", "core", span_none(), &test_ctx);

    compile(&test_ctx);
    *out_test_ctx = test_ctx;
}

static void print_test_result(
    CompileCtx* test_ctx,
    bool error,
    const char* test_call_filename,
    usize test_call_line)
{
    if (error) {
        g_error = true;
        fprintf(stderr, "\n  >> At %s:%lu", test_call_filename, test_call_line);
#ifndef TEST_PRINT_COMPILER_MSGS
        fprintf(stderr, "\n");
        bufloop(test_ctx->msgs, i) {
            test_ctx->print_msg_to_stderr = true;
            _msg_emit_no_register(&test_ctx->msgs[i], test_ctx);
            test_ctx->print_msg_to_stderr = false;
        }
#endif
    } else {
        passed_tests++;
#ifndef TEST_PRINT_COMPILER_MSGS
        fprintf(stderr, "%sok%s", g_green_color, g_reset_color);
#endif
    }
    fprintf(stderr, "\n");
}

static void print_fail_text() {
#ifndef TEST_PRINT_COMPILER_MSGS
    fprintf(stderr, "%sfail%s", g_red_color, g_reset_color);
#endif
}

static void _test_valid(
    const char* test_call_filename,
    usize test_call_line,
    const char* testname,
    const char* srccode)
{
    CompileCtx test_ctx;
    Srcfile srcfiles[1];
    initialize_test(&test_ctx, &srcfiles[0], test_call_line, testname, srccode);

    bool error = false;
    if (buflen(test_ctx.msgs) > 0) {
        if (!error) print_fail_text();
        error = true;
        fprintf(
            stderr,
            "\n  >> Expected no msgs, got %lu msgs",
            buflen(test_ctx.msgs));
    }

    print_test_result(
        &test_ctx,
        error,
        test_call_filename,
        test_call_line);
}

#define test_valid(testname, srccode) \
    (_test_valid(__FILE__, __LINE__, (testname), (srccode)))

static void _test_invalid(
    const char* test_call_filename,
    usize test_call_line,
    const char* testname,
    const char* srccode,
    usize num_msgs,
    TestMsgSpec* msgs)
{
    CompileCtx test_ctx;
    Srcfile srcfiles[1];
    initialize_test(&test_ctx, &srcfiles[0], test_call_line, testname, srccode);

    bool error = false;
    if (buflen(test_ctx.msgs) == num_msgs) {
        for (usize i = 0; i < num_msgs; i++) {
            if (test_ctx.msgs[i].kind == msgs[i].kind) {
                if (strcmp(test_ctx.msgs[i].msg, msgs[i].msg) != 0) {
                    if (!error) print_fail_text();
                    error = true;
                    fprintf(
                        stderr,
                        "\n  >> (%lu) Expected msgstr \"%s%s%s\", got \"%s%s%s\"",
                        i,
                        g_bold_color,
                        msgs[i].msg,
                        g_reset_color,
                        g_bold_color,
                        test_ctx.msgs[i].msg,
                        g_reset_color);
                }

                if (test_ctx.msgs[i].span.exists == msgs[i].srcloc.exists) {
                    if (msgs[i].srcloc.exists) {
                        SrcLoc actual_srcloc =
                            compute_srcloc_from_span(test_ctx.msgs[i].span.span);
                        if (actual_srcloc.line != msgs[i].srcloc.srcloc.line
                            || actual_srcloc.col != msgs[i].srcloc.srcloc.col) {
                            if (!error) print_fail_text();
                            error = true;
                            fprintf(
                                stderr,
                                "\n  >> (%lu) Expected msg at %lu:%lu, got %lu:%lu",
                                i,
                                msgs[i].srcloc.srcloc.line,
                                msgs[i].srcloc.srcloc.col,
                                actual_srcloc.line,
                                actual_srcloc.col);
                        }
                    }
                } else {
                    if (!error) print_fail_text();
                    error = true;
                    fprintf(
                        stderr,
                        "\n  >> (%lu) Expected msg %s span, got msg %s span",
                        i,
                        msgs[i].srcloc.exists ? "with" : "without",
                        test_ctx.msgs[i].span.exists ? "with" : "without");
                }
            } else {
                if (!error) print_fail_text();
                error = true;
                fprintf(
                    stderr,
                    "\n  >> (%lu) Expected msgkind %u, got %u",
                    i,
                    msgs[i].kind,
                    test_ctx.msgs[i].kind);
            }
        }
    } else {
        if (!error) print_fail_text();
        error = true;
        fprintf(
            stderr,
            "\n  >> Expected msgs %lu, got %lu",
            num_msgs,
            buflen(test_ctx.msgs));
    }

    print_test_result(
        &test_ctx,
        error,
        test_call_filename,
        test_call_line);
}

#define test_invalid(testname, srccode, num_msgs, msgs) \
    (_test_invalid(__FILE__, __LINE__, (testname), (srccode), (num_msgs), (msgs)))

static void _test_invalid_one_errspan(
    const char* test_call_filename,
    usize test_call_line,
    const char* testname,
    const char* srccode,
    const char* msg,
    usize line,
    usize col)
{
    _test_invalid(
        test_call_filename,
        test_call_line,
        testname,
        srccode,
        1,
        ((TestMsgSpec[1]){
            {
                .kind = MSG_ERROR,
                .msg = msg,
                .srcloc = {
                    .srcloc = {
                        .line = line,
                        .col = col,
                    },
                    .exists = true,
                },
            },
        })
    );
}

#define test_invalid_one_errspan(testname, srccode, msg, line, col) \
    (_test_invalid_one_errspan(__FILE__, __LINE__, (testname), (srccode), (msg), (line), (col)))

int main() {
    init_global_compiler_state();
    init_bigint();

    test_invalid_one_errspan(
        "unterminated string literal error",
        "\"hello\n",
        "unterminated string literal",
        1,
        1);

    test_invalid(
        "invalid char error",
        "`",
        2,
        ((TestMsgSpec[2]){
            {
                .kind = MSG_ERROR,
                .msg = "invalid character",
                .srcloc = {
                    .srcloc = {
                        .line = 1,
                        .col = 1,
                    },
                    .exists = true,
                },
            },
            {
                .kind = MSG_NOTE,
                .msg = "each invalid character is reported only once",
                .srcloc = { .exists = false },
            },
        })
    );

    //test_valid(
    //    "Empty struct definition",
    //    "struct Main {}\n"
    //    "pub fn main() void {}\n");

    //test_valid(
    //    "Struct definition with one field",
    //    "struct Main { a: u8 }\n"
    //    "pub fn main() void {}\n");

    //test_valid(
    //    "Struct definition with one field",
    //    "struct Main { a: u8, }\n"
    //    "pub fn main() void {}\n");

    //test_valid(
    //    "Struct instantiation",
    //    "struct Main { a: u8, b: u16, }\n"
    //    "pub fn main() void {\n"
    //    "    imm x = Main {\n"
    //    "        .a = 1,\n"
    //    "        .b = 1,\n"
    //    "    };\n"
    //    "}\n");

    //test_valid(
    //    "Struct instantiation",
    //    "struct Main { a: u8, b: u16, }\n"
    //    "pub fn main() void {\n"
    //    "    imm x: Main = .{\n"
    //    "        .a = 1,\n"
    //    "        .b = 1,\n"
    //    "    };\n"
    //    "}\n");

    //test_valid(
    //    "Struct type usage",
    //    "pub fn main() void {}\n"
    //    "pub fn call(m: Main) void {}\n"
    //    "struct Main { a: u8, b: u16, }\n");

    //test_valid(
    //    "Empty function definition",
    //    "pub fn main() void {}\n");

    //test_valid(
    //    "Private empty function definition",
    //    "pub fn main() void {}\n"
    //    "fn empty() void {}\n");

    //test_valid(
    //    "Function definition with return type",
    //    "pub fn main() void {}\n"
    //    "fn empty() *imm u8 {}\n");

    //test_valid(
    //    "Function definition with arguments",
    //    "pub fn main() void {}\n"
    //    "fn empty(a: u8, b: []i32) void;\n");

    //test_valid(
    //    "Function definition with arguments",
    //    "pub fn main() void {}\n"
    //    "fn empty(a: u8, b: []imm i32,) void;\n");

    //test_valid(
    //    "Function definition with body",
    //    "pub fn main() void { return; }\n");

    //test_valid(
    //    "Function call",
    //    "pub fn main() void { call(); }\n"
    //    "fn call() void {}\n");

    //test_valid(
    //    "Function call",
    //    "pub fn main() void { _ = call(); }\n"
    //    "fn call() u8 {}\n");

    //test_valid(
    //    "Function call",
    //    "pub fn main() void { _ = call(1); }\n"
    //    "fn call(a: i32) u8 {}\n");

    //test_valid(
    //    "Function call",
    //    "pub fn main() void { _ = call(1, 2); }\n"
    //    "fn call(a: i32, b: i64) u8 {}\n");

    //test_valid(
    //    "Function call",
    //    "pub fn main() void { _ = call(1, 2,); }\n"
    //    "fn call(a: i32, b: i64,) u8 {}\n");

    //test_valid(
    //    "Builtin types",
    //    "pub fn main() void {\n"
    //    "    mut x: (\n"
    //    "        u8,\n"
    //    "        u16,\n"
    //    "        u32,\n"
    //    "        u64,\n"
    //    "        i8,\n"
    //    "        i16,\n"
    //    "        i32,\n"
    //    "        i64,\n"
    //    "        f32,\n"
    //    "        f64,\n"
    //    "        bool,\n"
    //    "        void,\n"
    //    "        *u8,\n"
    //    "        *imm u8,\n"
    //    "        [*]u8,\n"
    //    "        [*]imm u8,\n"
    //    "        []u8,\n"
    //    "        []imm u8,\n"
    //    "        [0]u8,\n"
    //    "        [1]u8,\n"
    //    "        [2]u8,\n"
    //    "        (u8),\n"
    //    "        (u8, u16),\n"
    //    "        (u8, u16,),\n"
    //    "    );\n"
    //    "}\n");

    // Main tests end

    test_invalid_one_errspan(
        "eof error in function call",
        "fn main() void{\n"
        "   call(\n",
        "unexpected end of file",
        3,
        1);

    test_invalid_one_errspan(
        "eof error in function call",
        "fn main() void{\n"
        "   call(1,\n",
        "unexpected end of file",
        3,
        1);

    test_invalid_one_errspan(
        "eof error in function call",
        "fn main() void{\n"
        "   call(1, 2, 3, a, b, \n",
        "unexpected end of file",
        3,
        1);

    test_invalid_one_errspan(
        "eof error in function call",
        "fn main() void{\n"
        "   call(",
        "unexpected end of file",
        2,
        9);

    test_invalid_one_errspan(
        "eof error in function header",
        "fn main(\n",
        "unexpected end of file",
        2,
        1);

    test_invalid_one_errspan(
        "eof error in function header",
        "fn main(a: usize,\n",
        "unexpected end of file",
        2,
        1);

    test_invalid_one_errspan(
        "eof error in function header",
        "fn main(a: usize, b: u8,\n",
        "unexpected end of file",
        2,
        1);

    test_invalid_one_errspan(
        "eof error in function header",
        "fn main(",
        "unexpected end of file",
        1,
        9);

    test_invalid_one_errspan(
        "eof error in scoped block",
        "fn main() void {\n",
        "unexpected end of file",
        2,
        1);

    test_invalid_one_errspan(
        "eof error in scoped block",
        "fn main() void {\n"
        "   imm s = 1;\n",
        "unexpected end of file",
        3,
        1);

    test_invalid_one_errspan(
        "eof error in scoped block",
        "fn main() void {",
        "unexpected end of file",
        1,
        17);

    test_invalid_one_errspan(
        "eof error in struct typespec",
        "struct Main {\n",
        "unexpected end of file",
        2,
        1);

    test_invalid_one_errspan(
        "eof error in struct typespec",
        "struct Main {\n"
        "   a: u8,\n",
        "unexpected end of file",
        3,
        1);

    test_invalid_one_errspan(
        "expected identifier in typespec access expr error",
        "fn main() my.1 {}",
        "expected symbol name",
        1,
        14);

    test_invalid_one_errspan(
        "expected identifier in typespec access expr error",
        "fn main() my.ty.\"df\" {}",
        "expected symbol name",
        1,
        17);

    test_invalid_one_errspan(
        "expected type error",
        "fn main() 1 {}\n",
        "expected type",
        1,
        11);

    test_invalid_one_errspan(
        "expected type error",
        "fn main() \"a\" {}\n",
        "expected type",
        1,
        11);

    test_invalid_one_errspan(
        "expected type error",
        "fn main(a: 1) void {}\n",
        "expected type",
        1,
        12);

    test_invalid_one_errspan(
        "if expr missing lparen in if cond",
        "fn main() void {\n"
        "   if true) {}\n"
        "}\n",
        "expected `(`",
        2,
        7);

    test_invalid_one_errspan(
        "if expr missing rparen in if cond",
        "fn main() void {\n"
        "   if (true {}\n"
        "}\n",
        "expected `)`",
        3,
        1);

    test_invalid_one_errspan(
        "if expr missing lparen in else if cond",
        "fn main() void {\n"
        "   if (true) {}\n"
        "   else if true) {}\n"
        "}\n",
        "expected `(`",
        3,
        12);

    test_invalid_one_errspan(
        "if expr missing rparen in else if cond",
        "fn main() void {\n"
        "   if (true) {}\n"
        "   else if (true {}\n"
        "}\n",
        "expected `)`",
        4,
        1);

    test_invalid_one_errspan(
        "if expr inside if body without `{}`",
        "fn main() void {\n"
        "   if (true) if (true) {}\n"
        "}\n",
        "`if` branch cannot contain another `if`, `while`, `for`",
        2,
        14);

    test_invalid_one_errspan(
        "expected expression inside function",
        "fn main() void{\n"
        "   )\n"
        "}\n",
        "expected expression",
        2,
        4);

    test_invalid_one_errspan(
        "expected expression inside function",
        "fn main() void{\n"
        "   if (true) else {}\n"
        "}\n",
        "expected expression",
        2,
        14);

    // TEST: more `expected expression` tests

    test_invalid_one_errspan(
        "missing comma in function call",
        "fn main() void{\n"
        "   call(1, 2 3);\n"
        "}\n",
        "expected `,` or `)`",
        2,
        14);

    test_invalid_one_errspan(
        "missing rparen in function call",
        "fn main() void{\n"
        "   call(1, 2, 3;\n"
        "}\n",
        "expected `,` or `)`",
        2,
        16);

    // TEST: "missing rparen in directive"

    test_invalid_one_errspan(
        "expected identifier in access expr error",
        "fn main() void{\n"
        "   my.1;\n"
        "}\n",
        "expected symbol name",
        2,
        7);

    test_invalid_one_errspan(
        "expected identifier in access expr error",
        "fn main() void{\n"
        "   my.\"\";\n"
        "}\n",
        "expected symbol name",
        2,
        7);

    test_invalid_one_errspan(
        "expected identifier in function header error",
        "fn 1() void{}\n",
        "expected function name",
        1,
        4);

    test_invalid_one_errspan(
        "expected identifier in function header error",
        "fn \"\"() void{}\n",
        "expected function name",
        1,
        4);

    test_invalid_one_errspan(
        "missing lparen in function header",
        "fn main) void{}\n",
        "expected `(`",
        1,
        8);

    test_invalid_one_errspan(
        "expected parameter name in function header",
        "fn main(1) void{}\n",
        "expected parameter name or `)`",
        1,
        9);

    test_invalid_one_errspan(
        "expected parameter name in function header",
        "fn main(\"\") void{}\n",
        "expected parameter name or `)`",
        1,
        9);

    test_invalid_one_errspan(
        "expected colon after parameter name function header",
        "fn main(a) void{}\n",
        "expected `:`",
        1,
        10);

    test_invalid_one_errspan(
        "missing comma in function header",
        "fn main(a: usize b: usize) void{}\n",
        "expected `,` or `)`",
        1,
        18);

    test_invalid_one_errspan(
        "missing `>` in generic type specifier",
        "fn main() void { imm x: HashMap<i32, i32;\n",
        "expected `,` or `>`",
        1,
        41);

    test_invalid_one_errspan(
        "missing array type after specifying size",
        "fn main() void { imm x: [1]; }\n",
        "expected type",
        1,
        28);

    test_invalid_one_errspan(
        "index",
        "fn main() void { imm a: u8 = 1; imm ptr = &a; ptr[0] = 2; }\n",
        "cannot index type `*imm u8`",
        1,
        47);

    // TODO: add integer error tests
    //test_invalid_one_errspan(
    //    "integer overflow",
    //    "fn main() void { imm x = 18446744073709551616 }\n",
    //    "",
    //    1,
    //    28);

    test_valid(
        "function definition",
        "fn main() void {}\n");

    test_valid(
        "local declaration",
        "fn main() void {\n"
        "    imm x: u32 = 0;\n"
        "}\n");

    //test_valid(
    //    "array literal",
    //    "fn main() void { imm x: [4]i32 = [0, 1, 2, 3]; }\n");

    //test_valid(
    //    "array literal",
    //    "fn main() void { imm x: [4]i32 = [0, 1, 2, 3]; }\n");

    //test_valid(
    //    "array literal",
    //    "fn main() void { imm x = [0, 1, 2, 3]i32; }\n");

    //test_valid(
    //    "tuple literal",
    //    "fn main() void { imm x = .(1, 2); }\n");

    //test_valid(
    //    "tuple literal",
    //    "fn main() void { imm x: (i32) = .(1); }\n");

    //test_valid(
    //    "tuple literal",
    //    "fn main() void { imm x: () = .(); }\n");

    //test_valid(
    //    "tuple literal",
    //    "fn main() void { imm x = .(); }\n");

    test_valid(
        "integer literal",
        "fn main() void { imm x = 1010101010100; }\n");

    // REMINDER: At scoped block
    // TODO: add tests for using variable/function in itself
    // TODO: add tests for using values as types in variables/functions

    fprintf(
        stderr,
        "total tests %lu; passed %s%lu%s; failed %s%lu%s\n",
        total_tests,
        passed_tests == total_tests ? g_bold_green_color : "",
        passed_tests,
        g_reset_color,
        passed_tests != total_tests ? g_bold_red_color : "",
        total_tests - passed_tests,
        g_reset_color);
    if (g_error) exit(1);
}
