#ifndef DISK_H
#define DISK_H

#include <stdint.h>

int read_file(const char* name, char* out, int max_size);
int write_file(const char* name, const char* data, int size);
void list_files();
int get_file_name(int index, char* name);
void init_filesystem();
void read_sector(uint32_t lba, uint8_t* buffer);
void write_sector(uint32_t lba, uint8_t* buffer);

#endif




