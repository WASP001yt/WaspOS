// disk.h
#ifndef DISK_H
#define DISK_H

int read_file(const char* name, char* out, int max_size);
int write_file(const char* name, const char* data, int size);
void list_files();

#endif
