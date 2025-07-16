#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

#define VGA_BLACK       0x00
#define VGA_BLUE        0x01
#define VGA_GREEN       0x02
#define VGA_CYAN        0x03
#define VGA_RED         0x04
#define VGA_MAGENTA     0x05
#define VGA_BROWN       0x06
#define VGA_LIGHT_GREY  0x07
#define VGA_DARK_GREY   0x08
#define VGA_LIGHT_BLUE  0x09
#define VGA_LIGHT_GREEN 0x0A
#define VGA_LIGHT_CYAN  0x0B
#define VGA_LIGHT_RED   0x0C
#define VGA_LIGHT_MAGENTA 0x0D
#define VGA_LIGHT_BROWN 0x0E
#define VGA_WHITE       0x0F

void init_graphics();
void set_pixel(int x, int y, uint8_t color);
uint8_t get_pixel(int x, int y);
void draw_line(int x1, int y1, int x2, int y2, uint8_t color);
void draw_rect(int x, int y, int width, int height, uint8_t color);
void fill_rect(int x, int y, int width, int height, uint8_t color);
void clear_graphics(uint8_t color);
void switch_to_graphics();
void switch_to_text();

#endif
