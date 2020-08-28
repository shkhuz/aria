`aria` is an open-source high-performance programming language & toolkit—designed and written by `Huzaifa Shaikh (+huzaifash)`.

## Semantics

### Hello World

```aria
main :: pub fn u8 {
	std.writeln("Hello, world");
}
```

### Comments

```aria
main :: pub fn u8 {
	// this is the main function.
	// it marks the starting point of the executable.
}
```

### Variable Declaration

```aria
main :: pub fn u8 {
	preprocess_time: f32;
}
```

### Variable Declaration without Type Annotation

```aria
main :: pub fn u8 {
	preprocess_time :: 6.20;
}
```

### Variable Declaration with Coercion

 ```aria
main :: pub fn u8 {
	preprocess_time: u32 :: 6.20;
}
 ```

### Pointer Declaration

```aria
main :: pub fn u8 {
	memory: void* :: std.mem.malloc(256);
}
```

### Dereferencing a Pointer

```aria
main :: pub fn u8 {
	memory: u32* :: std.mem.malloc(4);
	*memory = 17;
}
```

### Pointer Arithmetic

```aria
main :: pub fn u8 {
	memory: u32* :: std.mem.malloc(8);
	*(memory + 1) = 17;

	// in-memory arrangement:
	// | <memory> | <memory+1> |
	// | -------- | ---------- |
	// |  (undef) |     17     |
	// | -------- | ---------- |
}
```

### #import directive

```aria
#import "hc.ar"

main :: pub fn u8 {
	hc.say_hello();
}
```

### Custom Types

```aria
Vector2f :: struct {
	x: f32,
	y: f32,
}

main :: pub fn u8 {
	vector :: Vector2f {
		12.3,
		36.44,
	};

	std.printfln("[x: %, y: %]", vector.x, vector.y);
}
```

### Custom Types with Linked Functions

```aria
Vector2f :: struct {
	x: f32,
	y: f32,

	add :: pub fn (vector: Self) Self {
		return Self {
			x: self.x + vector.x,
			y: self.y + vector.y,
		};
	}
}

main :: pub fn u8 {
	a :: Vector2f {
		x: 45.4,
		y: 323.02,
	};

	translate_offset :: Vector2f {
		5.3,
		1.655,
	};

	a = a.add(translate_offset);
	std.printfln("[a.x: %, a.y: %]", a.x, a.y);
	std.printfln("[translate_offset.x: %, translate_offset.y: %]", translate_offset.x, translate_offset.y);
	std.printfln("[result.x: %, result.y: %]", a.x, a.y);
}
```

### Custom Types with Solitary Functions

```aria
Vector2f :: struct {
	x: f32,
	y: f32,

	new :: pub fn no_instance (x: f32, y: f32) Self {
		return Self {
			x,
			y,
		};
	}
}

main :: pub fn u8 {
	vector :: Vector2f.new(5.4, 0.71);
}
```

### Built-in Assertions

```aria
main :: pub fn u8 {
	#assert(12 == 12);
	#assert(null != null); // ERROR: failed assertion
}
```

### Type Alias

```aria
string :: alias char*;

main :: pub fn u8 {
	name: string :: "t89a4f2";
}
```

### Namespaces

```aria
hc :: namespace {
	say_hello :: pub fn {
		std.writeln("hc: hello.");
	}
}

ether :: namespace {
	say_hello :: pub fn {
		std.writeln("ether: hello.");
	}
}

main :: pub fn u8 {
	hc.say_hello(), ether.say_hello();
}
```

### Protocols and Extensions

```aria
Window :: protocol {
	blit :: fn (bitmap: Bitmap);
}

WindowsWindow :: struct {
	// Windows-related variables ...
}

extension WindowsWindow with Window {
	blit :: fn (bitmap: Bitmap) {
		// blit implementation for Windows
	}
}

LinuxWindow :: struct {
	// Linux-related variables ...
}

extension LinuxWindow with Window {
	blit :: fn (bitmap: Bitmap) {
		// blit implementation for Linux
	}
}
```

### Generics

```aria
// NOTE: all integers implement the integer protocol
max :: fn<T: integer> (a: T, b: T) T {
	if a > b {
		return a;
	}
	else {
		return b;
	}
}

main :: pub fn u8 {
	max(2, 4);
	max(3i32, 564i32);
	max(12u16, 55u16);
	max<u32>(75, 6);

	max(4i32, 9i8);   // \
	max<i8>(89, 4.5); // |- ERROR
	max("h", "g");    // /
}
```
