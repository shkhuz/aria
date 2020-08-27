#include <stdio.h>
#include <hc.h>

int main(int argc, char** argv) {
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
}
