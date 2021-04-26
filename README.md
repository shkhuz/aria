# Aria

![GitHub Workflow Status](https://github.com/huzaifash/aria/actions/workflows/build.yml/badge.svg)
![GitHub Workflow Status](https://github.com/huzaifash/aria/actions/workflows/codeql-analysis.yml/badge.svg)

This repository houses the Aria compiler/toolchain and the language specification. 

# Sample Code 

```rust
namespace math {

struct Vector2 {
	x, y: f32,

	fn add(self, other: Vector2) {
		Self {
			x: self.x + other.x,
			y: self.y + other.y,
		}
	}
}

}
```

## _Generics_

```rust
fn max<T>(a: T, b: T) T {
	if a > b { a }
	else { b }
}
```

## _Errors_

```rust
fn allocate_memory(n: usize): u8[]! {
	let mem = gp_allocator_mem(n)?;
	{ mem, n }
}
```

## _Optionals_

```rust
fn main() {
	std::printf(if open_file("test.txt") |file| {
		"file successfully opened"
	} else {
		"file cannot be opened"
	});
}

fn open_file(fpath: string): std::File? {
	if std::os::openf(fpath, std::io::rb) |file| {
		file
	} else |_| {
		none
	}
}
```

## _Read User Input_

```rust
#import "std";

fn main() void! {
	let input: u8[];
	std::io::read_to_string(&input)?;
	defer free(input);
	let input = input as const u8[];
}
```

# Building

```sh
git clone https://github.com/huzaifash/aria.git
cd aria/
make
```

# License

This project is MIT licensed. See the `License` file 
for more details.
