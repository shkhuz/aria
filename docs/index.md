---
layout: home
title: Home
permalink: /
nav_order: 1
---

# The Aria project
{: .fs-9 }

An experimental systems programming language.
{: .fs-6 .fw-300 }

# Sample Code 

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

## _Generics_

```
proc max<T>(a: T, b: T) T {
	if a > b { a }
	else { b }
}
```

## _Errors_

```
@import("std");

proc allocate_memory(n: usize) ![]u8 {
	let mem = std::gp_allocator_mem(n)?;
	{ mem, n }
}
```

## _Optionals_

```
@import("std");

proc main() {
	std::printf(if open_file("test.txt") |_| {
		"file successfully opened"
	} else {
		"file cannot be opened"
	});
}

proc open_file(fpath: string) ?std::File {
	if std::os::openf(fpath, std::io::rb) |file| {
		file
	} else |_| {
		none
	}
}
```

## _Read User Input_

```
@import("std");

proc main() !void {
	let input = std::io::read_to_string()?;
	defer free(input);
	redef input = input as const u8[];
}
```

## _Conditional Compilation_

```
@import("std");

proc main() !void {
	std::writeln(static if std::os::host_os == std::os::OsType::UNIX {
		"we are on *NIX"
	} else if std::os::host_os == std::os::OsType::Windows {
		"windows."
	} else {
		"something else. hmm..."
	});
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
