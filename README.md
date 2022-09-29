# The Aria project

[![Build](https://github.com/shkhuz/aria/actions/workflows/build.yml/badge.svg?branch=master&event=push)](https://github.com/shkhuz/aria/actions/workflows/build.yml)

This repository houses the Aria compiler/toolchain and the language
specification. 

## Sample Code


### Fibbonacci

```rust
fn main() i32 {
    writeln_i64(fib(9));
    0
}

fn fib(n: i32) i32 {
    if n <= 1 {
        n
    } else {
        fib(n - 1) + fib(n - 2)
    }
}   
```

## Supported Targets

- x86-64
- AArch64

For the time being, the Aria compiler only supports Unix enviroments, because
it makes it easy to extend the compiler without having to worry about other
platforms. 

## Building

```sh
git clone https://github.com/shkhuz/aria.git
cd aria/
make
```

### Dependencies

- LLVM (any recent version)
- `ld` linker

## Roadmap

- [X] System V compatible functions
- [ ] Basic constructs
  - [X] `if` statements
  - [ ] `switch` statements
  - [ ] `for` loops
  - [X] `while` loops
- [X] Local variables
- [X] Global variables
- [X] Extern variables
- [X] Structs
- [ ] Unions
- [ ] Enums
- [ ] Operators
  - [X] `+` `-` `==` `!=` `<` `>` `<=` `>=` `-` `&` `*(deref)` `!` `as` `()(function call)` `=` `[](index)` `.(field access)`
  - [ ] `*` `/` `and` `or` `&(bitwise)` `|`
- [X] Block expressions / Implicit return expression
- [ ] Module system
- [X] Arrays
- [X] Slices (`[]`)

## License

This project is GNU GPLv3 licensed. See the `COPYING` file 
for more details.

