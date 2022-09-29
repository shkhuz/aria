# The Aria project

[![Build](https://github.com/shkhuz/aria/actions/workflows/build.yml/badge.svg?branch=master&event=push)](https://github.com/shkhuz/aria/actions/workflows/build.yml)

_Note: The compiler is undergoing major changes at the moment, so the examples in this document and in the examples/ directory might not work._

This repository houses the Aria compiler/toolchain and the language
specification. 

## Sample Code

### Fibbonacci

```rust
const std = @load("std.ar");

def main() {
    std.print("{}\n", fib(9));
}

def fib(n: i32) {
    if (n <= 1) n
    else fib(n-1) + fib(n-2)
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

- LLVM

## Roadmap

- [ ] System V compatible functions
- [ ] Basic constructs
  - [X] `if` statements
  - [ ] `switch` statements
  - [ ] `for` loops
  - [X] `while` loops
- [ ] Local variables
- [ ] Global variables
- [ ] Extern variables
- [ ] Structs
- [ ] Unions
- [ ] Enums
- [ ] Operators
  - [ ] `+` `-` `==` `!=` `<` `>` `<=` `>=` `-` `&` `*(deref)` `!` `as` `()(function call)` `=` `[](index)` `.(field access)`
  - [ ] `*` `/` `and` `or` `&(bitwise)` `|`
- [ ] Block expressions / Implicit return expression
- [ ] Module system
- [ ] Arrays
- [ ] Slices (`[]`)

## License

This project is GNU GPLv3 licensed. See the `COPYING` file 
for more details.

