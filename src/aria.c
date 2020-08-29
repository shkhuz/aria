#include <arpch.h>
#include <buf.h>

int main(void) {

	int* integers = null;
	buf_push(integers, 0);
	buf_push(integers, 1);
	buf_push(integers, 2);
	buf_push(integers, 3);
	buf_push(integers, 4);
	buf_push(integers, 5);

	buf_remove(integers, 3);

	for (int i = 0; i < 6; ++i) {
		printf("(%d)\n", integers[i]);
	}

	if (str_intern("huzaif") == str_intern("huzaifa")) {
		puts(":: names do match.");
	}
	else {
		puts(":: names do not match.");
	}

	File* aria_src_file = file_read("examples/hello_worlda.ar");
	if (!aria_src_file) {
		puts(":: cannot find `examples/hello_world.ar`: aborting");
	} else {
		puts(aria_src_file->contents);
	}

}
