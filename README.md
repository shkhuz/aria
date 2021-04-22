# Aria

![GitHub Workflow Status](https://img.shields.io/github/workflow/status/huzaifash/aria/build?style=plastic)

This repository houses the Aria compiler/toolchain and the language specification. 

# Sample Code 

```rust
// Code shown below is only written to show the various language constructs. 

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

fn allocate_memory(n: usize): u8[]! {
	let mem = gp_allocator_mem(n)?;
	{ mem, n }
}

fn open_file(fpath: string): std::File? {
	// os-specific file handling ...
	if got_handle {
		std::File {
			// ...
		}
	} else {
		none
	}
}
```

# Building

```sh
git clone https://github.com/huzaifash/aria.git
cd aria/
make
```

License
=======

This project is MIT licensed. See the `License` file 
for more details.
