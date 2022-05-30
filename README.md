# The Aria project

![GitHub Workflow Status](https://github.com/shkhuz/aria/actions/workflows/build.yml/badge.svg)

This repository houses the Aria compiler/toolchain and the language
specification. 

## Sample Code

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

## Building

```sh
git clone https://github.com/shkhuz/aria.git
cd aria/
make
```

### Dependencies

- LLVM (any recent version)
- `ld` linker

For the time being, the Aria compiler only supports \*nix enviroments, because
it makes it easy to extend the compiler without having to worry about other
platforms. 

## Roadmap

- [X] Functions (internal language-compatible)
- [X] Extern C-Abi functions
- [ ] Basic constructs
  - [X] `if` statements
  - [ ] `switch` statements
  - [ ] `for` loops
  - [X] `while` loops
- [X] Local variables
- [ ] Structs
- [ ] Unions
- [ ] Enums
- [ ] Operators
  - [X] `+` `-` `==` `!=` `<` `>` `<=` `>=` `-` `&` `*`(deref) `!` `as` `()` `=`
  - [ ] `*` `/` `and` `or` `&`(bitwise) `|`
- [X] Block expressions / Implicit return expression
- [ ] Module system
- [ ] Global variables

## License

This project is MIT-licensed. See the `LICENSE` file 
for more details.

