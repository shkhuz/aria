#include "core.cpp"
#include "bigint.cpp"
#include "file_io.cpp"
#include "types.cpp"

int main(int argc, char* argv[]) {
    ALLOC_WITH_TYPE(b, bigint);
    bigint_init_u64(b, UINT32_MAX);
    if (bigint_fits_i32(b)) {
        int m = min(1, 2);
        printf("%d", m);
        float n = min(1.0f, 2.0f);
        printf("%f", n);
    }

    TokenKind kind = TOKEN_KIND_IDENTIFIER;

    fmt::print("Hello, {}!\n", kind);

    FileOrError f = read_file("Makefile");
    if (f.status == FileOpenOp::success) {
        std::cout << f.handle->contents;
    }
    else {
        std::cerr << "Out of luck!" << std::endl;
    }
}
