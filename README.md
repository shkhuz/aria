![aria-banner-transparent-blue.png](assets/aria-banner-transparent-blue.png?raw=true)
An open-source high-performance programming language & toolkit.

## Semantics

### Hello World
```aria
#import <std>

pub main :: fn u8 {
	std.println("Hello, {}", "world");
}
```

### Comments
```aria
pub main :: fn u8 {
	// this is the `main` function.
	// it marks the starting point of the executable.
}
```

### Variable Declaration
```aria
pub main :: fn u8 {
	preprocess_time: f32;
}
```

### Variable Declaration Without Type Annotation
```aria
pub main :: fn u8 {
	preprocess_time :: 6.20;
}
```

### Pointer Declaration
```aria
pub main :: fn u8 {
	memory: void*: std.mem.malloc(256);
}
```

### Dereferencing a Pointer
```aria
pub main :: fn u8 {
	memory: u32* :: std.mem.malloc(4);
	*memory = 17;
}
```

### Pointer Arithmetic

```aria
pub main :: fn u8 {
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

pub main :: fn u8 {
	hc.say_hello();
}
```

### Custom Types

```aria
Vector2f :: struct {
	x: f32,
	y: f32,
}

pub main :: fn u8 {
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

	pub add :: fn (vector: Self) Self {
		return Self {
			x: self.x + vector.x,
			y: self.y + vector.y,
		};
	}
}

pub main :: fn u8 {
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

	pub new :: fn no_instance (x: f32, y: f32) Self {
		return Self {
			x,
			y,
		};
	}
}

pub main :: fn u8 {
	vector :: Vector2f.new(5.4, 0.71);
}
```

### Built-in Assertions

```aria
pub main :: fn u8 {
	#assert(12 == 12);
	#assert(null != null); // ERROR: failed assertion
}
```

### Type Alias

```aria
string :: alias char*;

pub main :: fn u8 {
	name: string :: "t89a4f2";
}
```

### Namespaces

```aria
hc :: namespace {
	pub say_hello :: fn {
		std.writeln("hc: hello.");
	}
}

ether :: namespace {
	pub say_hello :: fn {
		std.writeln("ether: hello.");
	}
}

pub main :: fn u8 {
	hc.say_hello(), ether.say_hello();
}
```

### Traits

```aria
Window :: trait {
	blit :: fn (bitmap: Bitmap);
}

WindowsWindow :: struct {
	// Windows-related variables ...
}

impl Window for WindowsWindow {
	blit :: fn (bitmap: Bitmap) {
		// blit implementation for Windows
	}
}

LinuxWindow :: struct {
	// Linux-related variables ...
}

impl Window for LinuxWindow {
	blit :: fn (bitmap: Bitmap) {
		// blit implementation for Linux
	}
}
```

### Generics

```aria
// NOTE: all integers implement the `integer` trait
max :: fn<T: integer> (a: T, b: T) T {
	if a > b {
		return a;
	}
	else {
		return b;
	}
}

pub main :: fn u8 {
	max(2, 4);
	max(3i32, 564i32);
	max(12u16, 55u16);
	max<u32>(75, 6);

	max(4i32, 9i8);   // \
	max<i8>(89, 4.5); // |- ERROR
	max("h", "g");    // /
}
```
