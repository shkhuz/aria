<p align="center">
    <img src="docs/assets/aria-banner-dark-white-optimized.png?raw=true" alt="aria-banner-dark-white-optimized.png">
    <br/>
    <br/>
    <b>An open-source high-performance programming language & toolkit.</b>
    <br/>
    <br/>
    <a href="https://github.com/huzaifash/aria/actions">
        <img src="https://github.com/huzaifash/aria/workflows/build/badge.svg">
    </a>
    <a href="https://github.com/huzaifash/aria/blob/master/LICENSE">
        <img src="https://img.shields.io/github/license/huzaifash/aria">
    </a>
    <br/>
    <a href="https://github.com/huzaifash/aria.git">
        <img src="https://img.shields.io/github/languages/code-size/huzaifash/aria">
    </a>
</p>

## Preface
_Aria_ is not meant to be _the_ perfect language, not is it meant
to compete with other programming languages. I solely developed it because I
found other languages somewhat peculiar. After all, _Aria_ is a personal project to cater my
requirements.

If _Aria_ suits you, you are more than welcome to use it for your needs. If
not, well, there are languages and toolchains way better than _Aria_ out
there.

## Sample Code
```
#import "std";

pub fn main(): u8 {
    std.writeln({
        input :: std.read_prompt(">>> ");
        if stri(input) == stri("") {
            input = "null";
        }
        input
    })._1
}
```

## Building and Installing
Building _Aria_ requires `make` as a dependency.
```sh
make install
```

## Usage
_Makefile_ does not install the binaries to the _/usr/bin/_ directory, to
avoid cluttering root directories where the compiler is accessible to
other users. If you're just testing out _Aria_, I'd suggest keeping the binary
in the _Download_ folder of your user directory. That way, you can symlink
the compiler and remove _Aria_ from the system easily later on.
```sh
<compiler_path> <source_file>
```

