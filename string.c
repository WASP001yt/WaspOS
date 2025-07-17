#include "string.h"

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

int strncmp(const char* a, const char* b, int n) {
    while (n-- && *a && (*a == *b)) {
        a++;
        b++;
    }
    if (n < 0) return 0;
    return *(const unsigned char*)a - *(const unsigned char*)b;
}
int parse_rect_cmd(const char* cmd, int* x, int* y, int* width, int* height, int* color) {
    if (strncmp(cmd, "rect", 4) != 0) {
        return 0;
    }
    
    const char* ptr = cmd + 4;
    
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }
    
    if (!parse_int(&ptr, x)) return 0;
    
    if (!parse_int(&ptr, y)) return 0;
    
    if (!parse_int(&ptr, width)) return 0;
    
    if (!parse_int(&ptr, height)) return 0;
    
    if (!parse_int(&ptr, color)) return 0;
    
    if (*color < 0x00 || *color > 0x0F) {
        return 0;
    }
    
    return 1;
}

int parse_cube_cmd(const char* cmd, int* x, int* y, int* width, int* height, int* color, int* darkcolor, int* brightcolor) {
    if (strncmp(cmd, "cube", 4) != 0) {
        return 0;
    }
    
    const char* ptr = cmd + 4;
    
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }
    
    if (!parse_int(&ptr, x)) return 0;
    
    if (!parse_int(&ptr, y)) return 0;
    
    if (!parse_int(&ptr, width)) return 0;
    
    if (!parse_int(&ptr, height)) return 0;
    
    if (!parse_int(&ptr, color)) return 0;
    
    if (!parse_int(&ptr, darkcolor)) return 0;
    
    if (!parse_int(&ptr, brightcolor)) return 0;
    
    if (*color < 0x00 || *color > 0x0F) {
        return 0;
    }
    
    return 1;
}

int parse_int(const char** ptr, int* result) {
    while (**ptr == ' ' || **ptr == '\t') {
        (*ptr)++;
    }
    
    if (**ptr == '\0') return 0;
    
    int value = 0;
    int negative = 0;
    
    if (**ptr == '-') {
        negative = 1;
        (*ptr)++;
    }
    
    if (**ptr < '0' || **ptr > '9') return 0;
    
    while (**ptr >= '0' && **ptr <= '9') {
        value = value * 10 + (**ptr - '0');
        (*ptr)++;
    }
    
    if (negative) value = -value;
    *result = value;
    
    return 1;
}

char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return original_dest;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        
        if (!*n) return (char*)haystack;
        haystack++;
    }
    
    return 0;
}

