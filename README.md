<p align="center">
    <img src="docs/assets/aria-banner-dark-white-optimized.png?raw=true" alt="aria-banner-dark-white-optimized.png">
    <br/>
    <br/>
    <b>An experimental low-level compiler &amp; toolkit.</b>
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

# Code Sample

```aria
#import "std";

struct Vec<T> {
	items: T[],
	cap: usize,

	pub fn new(): Self {
		Self {
			nullslice,
			0,
		}
	}

	pub fn push(self, item: T): void! {
		if (self.items.len == self.cap) {
			self.resize(self.cap * 2)!;
		} else {
			self.items[self.items.len] = item;
			self.items.len += 1;
		}
	}

	pub fn pop(self): T? {
		if (self.items.len == 0) {
			none
		} else {
			self.items.len -= 1;
			self.items[self.items.len]
		}
	}

	// more code ...
}

pub fn main() {
	vec := Vec<usize>::new();
	vec.push(1);
	vec.push(2);
	
	while (vec.pop()) |n| {
		std::println("{}", n);
	}
}
```

# Features 

- No header files
- No GC
- No Runtime
- Errors and Optionals
- Namespaced language constructs
- Execute arbitrary code at compile-time
- Hygenic macros

# Motivation

Process by elimination:
- **C**: Header files, unhygenic macros, standard library is
a joke.
- **C++**: Same as C, but much more painful.
- **Rust**: Right direction, but:
  - Macros pollute the global namespace (deal-breaker);
  - Compile times too long for non-trivial projects (as of Apr 2021);
  - Borrow checker.
- **Zig**: Does _many_ things right, and I've even copied some of the language
  constructs from it, but:
  - Ugly syntax;
  - `comptime` too restrictive;
  - No way to contain namespace containers in one file;
  - Still in infancy (as of Apr 2021).

# License

This project is MIT licensed. See `LICENSE` for more details.
