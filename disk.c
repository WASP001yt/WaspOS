// disk.c
#include "disk.h"
#include <stdint.h>
#include "string.h"  // Assuming you're providing your own strcmp, strncmp
extern void puts(const char*);

// Dummy file system with one file for demonstration
static const char* fake_file = "example.txt";
static char fake_storage[512] = "Hello from example.txt!";

int read_file(const char* name, char* out, int max_size) {
    if (strcmp(name, fake_file) != 0) return -1;
    int i = 0;
    while (fake_storage[i] && i < max_size) {
        out[i] = fake_storage[i];
        i++;
    }
    return i;
}

int write_file(const char* name, const char* data, int size) {
    if (strcmp(name, fake_file) != 0) return -1;
    int len = size < 511 ? size : 511;
    for (int i = 0; i < len; i++) {
        fake_storage[i] = data[i];
    }
    fake_storage[len] = 0;
    return len;
}

void list_files() {
    puts(fake_file);
    puts("\n");
}
