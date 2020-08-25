# aria
A low-level general-purpose programming toolchain.

## Sample Code
```aria
// this is not real code.
// it is just used to demo language features.

#import "stdlib"

string :: alias char*;

main :: fn i32 {
	window :: std.alloc(Sf.RenderWindow.new(1280, 720, "Hello World"));
	window.get_options()
		  .set_type(sf.STYLE_MINIMAL)
		  .set_default_pos(sf.Vector2f.new(0.0, 0.0))
		  .init().expect("failed to set options");

	while window.is_open() {
		window.clear();
		window.render();
	}

	// test variables
	value :: some(123);
	value_x :: none;
	
	addition :: unwrap value + (unwrap value_x or 0);
}

std :: namespace {
	alloc :: pub fn<T> (obj: T) T* {
		size: u64 :: @sizeof(obj);
		memory_allocated :: mem.malloc(size);
		*memory_allocated = obj;
		return memory_allocated;
	}
}

Sf :: namespace {
	RenderWindow :: struct {
		new :: pub fn no_instance (width: u32, height: u32, title: string) option self {
			return Core.create_window();
		}

		get_options :: pub fn Window.Options {
			return Window.Options.new();
		}
	}

	Window :: namespace {
		Options :: struct {
			type: WindowType;
			default_pos: Vector2f;

			new :: pub fn no_instance Self {
				return Self {
					STYLE_NORMAL,
					Vector2f.zero(),
				};
			}

			init :: pub fn Result<u32, string> {
				return Core.inspect_options(self);
			}

			set_type :: pub fn (type: WindowType) Self {
				self.type = type;
				return self;
			}

			set_default_pos :: pub fn (pos: Vector2f) Self {
				self.default_pos = pos;
				return self;
			}
		}
	}
}
```
