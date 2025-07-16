#ifndef STRING_H
#define STRING_H

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
int parse_rect_cmd(const char* cmd, int* x, int* y, int* width, int* height, int* color);
int parse_int(const char** ptr, int* result);
int parse_cube_cmd(const char* cmd, int* x, int* y, int* width, int* height, int* color, int* darkcolor, int* brightcolor);
#endif

