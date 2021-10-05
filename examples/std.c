#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void write_i64(int64_t n) {
    printf("%li", n);
}

void write_u64(uint64_t n) {
    printf("%lu", n);
}

void write_char(int8_t c) {
    fputc((char)c, stdout);
}

uint8_t rand8() {
    return rand();
}

char* read_stdio() {
    static char buf[256];
    return fgets(buf, sizeof(buf), stdin);
}

uint8_t eof_stdio() {
    if (feof(stdin)) {
        return 1;
    }
    return 0;
}

void write_str(char* str) {
    fputs(str, stdout);
}

uint64_t convert_to_u64(char* str) {
    uint64_t i;
    sscanf(str, "%lu", &i);
    return i;
}

/* void give_ints( int a, char c ,long b ,int d ,int e ,char f ,long g ,int h) { */
/*     int i = 1; */
/*     int j = 2; */
/*     int k = 3; */
/*     print_u64(a); */
/*     print_u64(b); */
/*     print_u64(c); */
/*     print_u64(d); */
/*     print_u64(e); */
/*     print_u64(f); */
/*     print_u64(g); */
/*     print_u64(h); */
/* } */

/* int main() { */
/*     int a = 1; */
/*     char c = 2; */
/*     long b = 3; */
/*     int d = 4; */
/*     int e = 1; */
/*     char f = 2; */
/*     long g = 3; */
/*     int h = 4; */
/*     give_ints(a, b, c, d, e, f, g, h); */
/* } */
