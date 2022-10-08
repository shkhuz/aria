# Aria programming language and toolchain

[![Build](https://github.com/shkhuz/aria/actions/workflows/build.yml/badge.svg?branch=master&event=push)](https://github.com/shkhuz/aria/actions/workflows/build.yml)

_Note: The compiler is undergoing major changes at the moment, so the examples in this document and in the examples/ directory might not work._

This repository houses the Aria compiler/toolchain and the language
specification. 

## Installation

Dependencies:

- LLVM
- lld

### Arch Linux

Aria is available on the AUR. Using an AUR helper (Yaourt):

```console
yay -S aria
```

Or downloading and installing the package manually:

```console
git clone https://aur.archlinux.org/aria.git
cd aria/
makepkg -si
```

### Unix

```console
git clone https://github.com/shkhuz/aria.git
cd aria/
make
sudo make install
```

## License

This project is GNU GPLv3 licensed. See the `COPYING` file 
for more details.

