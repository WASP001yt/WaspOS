#include <stdint.h>
#include "disk.h"
#include "string.h"
#include "graphics.h"

#define VIDEO_MEMORY ((volatile char*)0xb8000)
#define VGA_MEMORY ((volatile uint8_t*)0xA0000)
#define MAX_ROWS 25
#define MAX_COLS 80

static int cursor = 0;

void putchar(char c);
void puts(const char* str);
static inline uint8_t inb(uint16_t port);
char get_key();
void wait_key_release();


void clear_screen() {
    for (int i = 0; i < MAX_ROWS * MAX_COLS * 2; i += 2) {
        VIDEO_MEMORY[i] = ' ';
        VIDEO_MEMORY[i + 1] = 0x07;
    }
    cursor = 0;
}

void putchar(char c) {
    if (c == '\n') {
        cursor += MAX_COLS - (cursor % MAX_COLS);
    }
    else if (c == '\b') {
        if (cursor > 0) {
            cursor--;
            VIDEO_MEMORY[cursor * 2] = ' ';
            VIDEO_MEMORY[cursor * 2 + 1] = 0x07;
        }
        return;
    }
    else {
        VIDEO_MEMORY[cursor * 2] = c;
        VIDEO_MEMORY[cursor * 2 + 1] = 0x07;
        cursor++;
    }
    if (cursor >= MAX_ROWS * MAX_COLS) {
        cursor = 0;
    }
}

void puts(const char* str) {
    while (*str) putchar(*str++);
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void wait_key_release() {
    while (inb(0x60) & 0x80);
}

char get_key(); // (Same as original, omitted for brevity)

void render_text_at(int row, int col, const char* text) {
    int pos = row * MAX_COLS + col;
    while (*text && pos < MAX_ROWS * MAX_COLS) {
        VIDEO_MEMORY[pos * 2] = *text++;
        VIDEO_MEMORY[pos * 2 + 1] = 0x07;
        pos++;
    }
}

void reset_screen() {
    clear_screen();
    cursor = 0;
}

char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0,           // Caps Lock
    0x80,        // F1
    0x81,        // F2
    0x82,        // F3
    0x83,        // F4
    0x84,        // F5
    0x85,        // F6
    0x86,        // F7
    0x87,        // F8
    0x88,        // F9
    0x89,        // F10
    0,           // Num Lock
    0,           // Scroll Lock
    0,           // Home
    0,           // Up Arrow
    0,           // Page Up
    '-',
    0,           // Left Arrow
    0,
    0,           // Right Arrow
    '+',
    0,           // End
    0,           // Down Arrow
    0,           // Page Down
    0,           // Insert
    0,           // Delete
    0, 0, 0,
    0x8A,        // F11
    0x8B         // F12
};


char get_key() {
    uint8_t scancode;

    // Wait for key press
    while (1) {
        scancode = inb(0x60);
        if (!(scancode & 0x80)) {  // If it's a key press (not release)
            // Wait for key release before returning
            while ((inb(0x64) & 1) == 0); // Wait until output buffer is full
            while (!(inb(0x60) & 0x80));  // Wait for release of same key
            if (scancode < sizeof(scancode_ascii)) {
                return scancode_ascii[scancode];
            }
        }
    }

    return 0;
}

void text_editor(const char* fname) {
    char buffer[512];
    int size = read_file(fname, buffer, sizeof(buffer));
    if (size < 0) {
        buffer[0] = '\0';
        size = 0;
    }

    int buf_len = size;
    int cursor_pos = buf_len;

    clear_graphics(VGA_BLUE);
    draw_string(10, 10, "Simple Text Editor (ESC=exit, F2=save)", VGA_WHITE);
    draw_string(10, 30, buffer, VGA_WHITE);

    while (1) {
        char c = get_key();

        if (c == 27) {
            break;
        }
        else if (c == 0x81) {
            buffer[buf_len] = 0;
            write_file(fname, buffer, buf_len);
            break;
        }
        else if (c == '\b') {
            if (cursor_pos > 0) {
                for (int i = cursor_pos - 1; i < buf_len - 1; i++) {
                    buffer[i] = buffer[i + 1];
                }
                buf_len--;
                cursor_pos--;
                buffer[buf_len] = 0;
            }
        }
        else if (c >= 32 && c <= 126) {
            if (buf_len < sizeof(buffer) - 1) {
                for (int i = buf_len; i > cursor_pos; i--) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[cursor_pos] = c;
                buf_len++;
                cursor_pos++;
                buffer[buf_len] = 0;
            }
        }

        clear_graphics(VGA_BLUE);
        draw_string(10, 10, "Simple Text Editor (ESC=exit, F2=save)", VGA_WHITE);
        draw_string(10, 30, buffer, VGA_WHITE);

        // Draw cursor
        int cursor_x = 10 + (cursor_pos % 40) * 8;
        int cursor_y = 30 + (cursor_pos / 40) * 8;
        draw_char(cursor_x, cursor_y, '_', VGA_LIGHT_GREEN);
    }
}

int x, y, width, height;
int color;
int brightcolor;
int darkcolor;

void kmain() {
    // Initialize graphics mode
    init_graphics();
    clear_graphics(VGA_BLUE);
    
    // Draw shell prompt
    draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
    draw_string(10, 30, "> ", VGA_WHITE);
    
    char cmd[80] = { 0 };
    int cmd_pos = 0;
    int cursor_x = 26; // After "> "
    int cursor_y = 30;
    
    while (1) {
        char c = get_key();
        
        if (c == '\n') {
            cmd[cmd_pos] = 0;
            cursor_y += 16; // Move to next line
            
            if (strncmp(cmd, "edit ", 5) == 0) {
                char* fname = cmd + 5;
                text_editor(fname);
                clear_graphics(VGA_BLUE);
                draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
                cursor_y = 30;
            }
            else if (strcmp(cmd, "list") == 0) {
                draw_string(10, cursor_y, "Files:", VGA_WHITE);
                cursor_y += 16;
                list_files();
            }
            else if (strncmp(cmd, "cat ", 4) == 0) {
                char* fname = cmd + 4;
                char buf[512];
                int size = read_file(fname, buf, sizeof(buf));
                if (size >= 0) {
                    buf[size] = 0;
                    draw_string(10, cursor_y, buf, VGA_WHITE);
                    cursor_y += 16;
                }
                else {
                    draw_string(10, cursor_y, "cat: file not found", VGA_WHITE);
                    cursor_y += 16;
                }
            }
            else if (parse_rect_cmd(cmd, &x, &y, &width, &height, &color)) {
                clear_graphics(VGA_BLUE);
                
                // Draw filled rectangle with parsed color
                fill_rect(x, y, width, height, color);
                
                // Wait for key press
                wait_key_release();
                clear_graphics(VGA_BLUE);
                draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
                cursor_y = 30;
            }
            else if (parse_cube_cmd(cmd, &x, &y, &width, &height, &color, &darkcolor, &brightcolor)) {
                clear_graphics(VGA_BLACK);
                
                // Calculate depth offset for 3D effect
                int depth = width / 4;  // Depth should be proportional to width
                
                // Draw the front face (main rectangle)
                fill_rect(x, y, width, height, color);
                
                // Draw the right face (darker color for shadow effect)
                // This creates a parallelogram by connecting front-right edge to back-right edge
                for (int i = 0; i < depth; i++) {
                    fill_rect(x + width + i, y - i, 1, height, darkcolor);
                }
                fill_rect(x + width, y - depth, depth, height, darkcolor);
                
                // Draw the top face (brighter color for highlight effect)
                // Simple parallelogram for the top face
                for (int i = 0; i < depth; i++) {
                    fill_rect(x + depth - i, y + i - depth, width, 1, brightcolor);
                }
                
                // Wait for key press
                wait_key_release();
                clear_graphics(VGA_BLUE);
                draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
                cursor_y = 30;
            }
            else if (strcmp(cmd, "clear") == 0) {
                clear_graphics(VGA_BLUE);
                draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
                cursor_y = 30;
            }
            else if (strncmp(cmd,"help", 4)== 0 || strncmp(cmd,"info", 4)== 0|| strncmp(cmd,"i", 4)== 0) {
                draw_string(10, cursor_y, "Commands: \nedit(works but save doesnt), \nlist(doesnt work), \ncat file(doesntwork), \nrect xpos y pos width height color,\ncube xpos ypos width height \ncolor darkcolor brightcolor,\n clear", VGA_WHITE);
                cursor_y += 65;
            }
            else {
                draw_string(10, cursor_y, "Unknown command", VGA_WHITE);
                cursor_y += 16;
            }
            
            if (cursor_y >= 180) {
                clear_graphics(VGA_BLUE);
                draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
                cursor_y = 30;
            }
            
            // Draw new prompt
            draw_string(10, cursor_y, "> ", VGA_WHITE);
            cursor_x = 26;
            cmd_pos = 0;
        }
        else if (c == '\b') {
            if (cmd_pos > 0) {
                cmd_pos--;
                cursor_x -= 8;
                // Clear the character
                fill_rect(cursor_x, cursor_y, 8, 8, VGA_BLUE);
            }
        }
        else if (c >= 32 && c <= 126) {
            if (cmd_pos < (int)sizeof(cmd) - 1) {
                cmd[cmd_pos++] = c;
                draw_char(cursor_x, cursor_y, c, VGA_WHITE);
                cursor_x += 8;
            }
        }
    }
}




















