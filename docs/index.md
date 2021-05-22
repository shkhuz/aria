---
title: The Aria project
title-prefix: Home
subtitle: An experimental systems programming language.
---

## Sample Code 

```
module math {
	struct Vector2 {
		x, y: f32,

		proc add(self, other: Vector2) {
			Self {
				x: self.x + other.x,
				y: self.y + other.y,
			}
		}
	}
}
```

### Generics

```
proc max<T>(a: T, b: T) T {
	if a > b { a }
	else { b }
}
```

### Errors

```
@import("std");

proc allocate_memory(n: usize) ![]u8 {
	let mem = std::gp_allocator_mem(n)?;
	std::slice::from_raw(mem, n)
}
```

### Optionals

```
@import("std");

proc main() {
	std::printf(if open_file("test.txt") {
		"file successfully opened"
	} else {
		"file cannot be opened"
	});
}

proc open_file(fpath: string) ?std::File {
	if std::os::openf(fpath, std::io::rb) with file {
		file
	} else {
		none
	}
}
```

### Read User Input

```
@import("std");

proc main() !void {
	let input = std::io::read_to_string()?;
	defer free(input);
	redef input = input as []const u8;
}
```

### Conditional Compilation

```
@import("std");

proc main() !void {
	std::writeln(static match std::os::host_os {
		std::os::OsType::UNIX => "we are on *NIX",
		std::os::OsType::Windows => "windows.",
		else => "something else. hmm...",
	});
}
```

## Building

```sh
git clone https://github.com/huzaifash/aria.git
cd aria/
make
```

## License

This project is MIT licensed. See the `License` file 
for more details.

