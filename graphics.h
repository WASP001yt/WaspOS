#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

#define VGA_BLACK 0
#define VGA_BLUE 1
#define VGA_GREEN 2
#define VGA_CYAN 3
#define VGA_RED 4
#define VGA_MAGENTA 5
#define VGA_BROWN 6
#define VGA_LIGHT_GREY 7
#define VGA_DARK_GREY 8
#define VGA_LIGHT_BLUE 9
#define VGA_LIGHT_GREEN 10
#define VGA_LIGHT_CYAN 11
#define VGA_LIGHT_RED 12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW 14
#define VGA_WHITE 15

void init_graphics();
void clear_graphics(uint8_t color);
void draw_string(int x, int y, const char* str, uint8_t color);
void draw_char(int x, int y, char c, uint8_t color);
void set_pixel(int x, int y, uint8_t color);
void switch_to_graphics();
void switch_to_text();
void draw_line(int x1, int y1, int x2, int y2, uint8_t color);
void draw_rect(int x, int y, int width, int height, uint8_t color);
void fill_rect(int x, int y, int width, int height, uint8_t color);

#endif


