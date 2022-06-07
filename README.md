# The Aria project

![GitHub Workflow Status](https://github.com/shkhuz/aria/actions/workflows/build.yml/badge.svg)

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

For the time being, the Aria compiler only supports \*nix enviroments, because
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

- [X] Functions (internal language-compatible)
- [X] Extern System V compatible functions
- [ ] Basic constructs
  - [X] `if` statements
  - [ ] `switch` statements
  - [ ] `for` loops
  - [X] `while` loops
- [X] Local variables
- [ ] Structs
- [ ] Unions
- [X] Arrays
- [ ] Enums
- [ ] Operators
  - [X] `+` `-` `==` `!=` `<` `>` `<=` `>=` `-` `&` `*(deref)` `!` `as` `()(function call)` `=` `[](index)`
  - [ ] `*` `/` `and` `or` `&(bitwise)` `|`
- [X] Block expressions / Implicit return expression
- [ ] Module system
- [X] Global variables
- [X] Extern variables

## License

This project is GNU GPLv3 licensed. See the `COPYING` file 
for more details.

