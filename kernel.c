#include <stdint.h>
#include <stddef.h>
#include "disk.h"
#include "string.h"
#include "graphics.h"

#define VIDEO_MEMORY ((volatile char*)0xb8000)
#define VGA_MEMORY ((volatile uint8_t*)0xA0000)
#define MAX_ROWS 25
#define MAX_COLS 80
#define MAX_VAR_NAME 32
#define MAX_VAR_VALUE 128
#define MAX_VARIABLES 32

typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
    int used;
} variable_t;

static variable_t variables[MAX_VARIABLES];
static int cursor = 0;
static int shift_pressed = 0;
int cursor_y = 30;

void putchar(char c);
void puts(const char* str);
static inline uint8_t inb(uint16_t port);
char get_key();
void wait_key_release();
void install_os_to_disk();

// String functions
int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

char* strncpy(char* dest, const char* src, int n) {
    int i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    int dest_len = strlen(dest);
    int i = 0;
    while (src[i]) {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
    return dest;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

void itoa(int value, char* str) {
    int i = 0;
    int is_negative = 0;
    
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    if (value < 0) {
        is_negative = 1;
        value = -value;
    }
    
    while (value != 0) {
        str[i++] = (value % 10) + '0';
        value = value / 10;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Variable management
void set_variable(const char* name, const char* value) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i].used && strcmp(variables[i].name, name) == 0) {
            strncpy(variables[i].value, value, MAX_VAR_VALUE - 1);
            variables[i].value[MAX_VAR_VALUE - 1] = '\0';
            return;
        }
    }
    
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (!variables[i].used) {
            strncpy(variables[i].name, name, MAX_VAR_NAME - 1);
            variables[i].name[MAX_VAR_NAME - 1] = '\0';
            strncpy(variables[i].value, value, MAX_VAR_VALUE - 1);
            variables[i].value[MAX_VAR_VALUE - 1] = '\0';
            variables[i].used = 1;
            return;
        }
    }
}

const char* get_variable(const char* name) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i].used && strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

// Parse and execute commands
int parse_let_command(const char* line) {
    if (strncmp(line, "let ", 4) != 0) {
        return 0;
    }
    
    const char* ptr = line + 4;
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    char var_name[MAX_VAR_NAME];
    int name_len = 0;
    while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '=' && name_len < MAX_VAR_NAME - 1) {
        var_name[name_len++] = *ptr++;
    }
    var_name[name_len] = '\0';
    
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '=') ptr++;
    
    char var_value[MAX_VAR_VALUE];
    int value_len = 0;
    while (*ptr && *ptr != '\n' && *ptr != '\r' && value_len < MAX_VAR_VALUE - 1) {
        var_value[value_len++] = *ptr++;
    }
    var_value[value_len] = '\0';
    
    if (value_len == 0) {
        set_variable(var_name, "0");
    } else {
        set_variable(var_name, var_value);
    }
    
    return 1;
}

int parse_assignment(const char* line) {
    const char* ptr = line;
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    char var_name[MAX_VAR_NAME];
    int name_len = 0;
    while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '+' && *ptr != '-' && *ptr != '=' && name_len < MAX_VAR_NAME - 1) {
        var_name[name_len++] = *ptr++;
    }
    var_name[name_len] = '\0';
    
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    if (*ptr == '+' && *(ptr + 1) == '=') {
        ptr += 2;
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        
        int increment = atoi(ptr);
        const char* current_val = get_variable(var_name);
        int current = current_val ? atoi(current_val) : 0;
        
        char new_val[32];
        itoa(current + increment, new_val);
        set_variable(var_name, new_val);
        return 1;
    }
    
    return 0;
}

char* substitute_variables(const char* cmd, char* output, int max_len) {
    int out_pos = 0;
    const char* ptr = cmd;
    
    while (*ptr && out_pos < max_len - 1) {
        if (*ptr == '$') {
            // Handle $variable syntax
            ptr++; // Skip $
            char var_name[MAX_VAR_NAME];
            int name_len = 0;
            
            while (*ptr && ((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= '0' && *ptr <= '9') || *ptr == '_') && name_len < MAX_VAR_NAME - 1) {
                var_name[name_len++] = *ptr++;
            }
            var_name[name_len] = '\0';
            
            if (name_len > 0) {
                const char* var_value = get_variable(var_name);
                if (var_value) {
                    int val_len = strlen(var_value);
                    if (out_pos + val_len < max_len - 1) {
                        for (int i = 0; i < val_len; i++) {
                            output[out_pos++] = var_value[i];
                        }
                    }
                } else {
                    // Variable not found, keep original text
                    output[out_pos++] = '$';
                    for (int i = 0; i < name_len && out_pos < max_len - 1; i++) {
                        output[out_pos++] = var_name[i];
                    }
                }
            } else {
                output[out_pos++] = '$';
            }
        } else if (*ptr >= 'a' && *ptr <= 'z') {
            // Check if this is a variable name (old behavior)
            char var_name[MAX_VAR_NAME];
            int name_len = 0;
            const char* var_start = ptr;
            
            while (*ptr && (*ptr >= 'a' && *ptr <= 'z') && name_len < MAX_VAR_NAME - 1) {
                var_name[name_len++] = *ptr++;
            }
            var_name[name_len] = '\0';
            
            const char* var_value = get_variable(var_name);
            if (var_value) {
                // Substitute variable value
                int val_len = strlen(var_value);
                if (out_pos + val_len < max_len - 1) {
                    for (int i = 0; i < val_len; i++) {
                        output[out_pos++] = var_value[i];
                    }
                }
            } else {
                // Not a variable, copy original text
                for (const char* p = var_start; p < ptr; p++) {
                    if (out_pos < max_len - 1) {
                        output[out_pos++] = *p;
                    }
                }
            }
        } else {
            output[out_pos++] = *ptr++;
        }
    }
    
    output[out_pos] = '\0';
    return output;
}

int execute_single_command(const char* cmd) {
    char substituted_cmd[256];
    substitute_variables(cmd, substituted_cmd, sizeof(substituted_cmd));
    
    if (parse_let_command(substituted_cmd)) {
        return 1;
    }
    
    if (parse_assignment(substituted_cmd)) {
        return 1;
    }
    
    // Handle cube command
    int x, y, width, height, color, darkcolor, brightcolor;
    if (parse_cube_cmd(substituted_cmd, &x, &y, &width, &height, &color, &darkcolor, &brightcolor)) {
        clear_graphics(VGA_BLACK);
        
        int depth = width / 4;
        fill_rect(x, y, width, height, color);
        
        for (int i = 0; i < depth; i++) {
            fill_rect(x + width + i, y - i, 1, height, darkcolor);
        }
        fill_rect(x + width, y - depth, depth, height, darkcolor);
        
        for (int i = 0; i < depth; i++) {
            fill_rect(x + depth - i, y + i - depth, width, 1, brightcolor);
        }
        
        // Remove the wait_key_release() and screen clearing here
        
        return 1;
    }
    
    return 0;
}

void execute_bash_file(const char* fname) {
    char buffer[2048];
    int size = read_file(fname, buffer, sizeof(buffer) - 1);
    if (size < 0) {
        draw_string(10, cursor_y, "File not found", VGA_WHITE);
        cursor_y += 16;
        return;
    }
    
    buffer[size] = '\0';
    
    char* line_start = buffer;
    char line[256];
    
    while (*line_start) {
        // Extract one line
        int line_len = 0;
        char* line_end = line_start;
        while (*line_end && *line_end != '\n' && *line_end != '\r' && line_len < 255) {
            line[line_len++] = *line_end++;
        }
        line[line_len] = '\0';
        
        // Skip empty lines and comments
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        
        if (*trimmed && *trimmed != '#') {
            // Check for for loop
            if (strncmp(trimmed, "for ", 4) == 0) {
                // Simple for loop parsing: for i 1 10
                const char* ptr = trimmed + 4;
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                
                // Get variable name
                char loop_var[32];
                int var_len = 0;
                while (*ptr && *ptr != ' ' && *ptr != '\t' && var_len < 31) {
                    loop_var[var_len++] = *ptr++;
                }
                loop_var[var_len] = '\0';
                
                // Get start value
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                int start_val = atoi(ptr);
                
                // Skip to end value
                while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                int end_val = atoi(ptr);
                
                // Debug output
                draw_string(10, cursor_y, "Loop found:", VGA_YELLOW);
                cursor_y += 16;
                
                // Skip to opening brace
                while (*line_end && *line_end != '{') line_end++;
                if (*line_end == '{') {
                    line_end++; // Skip opening brace
                    
                    // Find closing brace and store body
                    char* body_start = line_end;
                    int brace_count = 1;
                    while (*line_end && brace_count > 0) {
                        if (*line_end == '{') brace_count++;
                        else if (*line_end == '}') brace_count--;
                        if (brace_count > 0) line_end++;
                    }
                    
                    // Execute loop multiple times
                    for (int loop_iter = start_val; loop_iter < end_val; loop_iter++) {
                        char val_str[32];
                        itoa(loop_iter, val_str);
                        set_variable(loop_var, val_str);
                        
                        // Execute body
                        char* body_ptr = body_start;
                        while (body_ptr < line_end) {
                            char body_line[256];
                            int line_len = 0;
                            
                            // Extract line
                            while (body_ptr < line_end && *body_ptr != '\n' && *body_ptr != '\r' && line_len < 255) {
                                body_line[line_len++] = *body_ptr++;
                            }
                            body_line[line_len] = '\0';
                            
                            // Skip newlines
                            while (body_ptr < line_end && (*body_ptr == '\n' || *body_ptr == '\r')) body_ptr++;
                            
                            // Execute if not empty
                            char* body_trimmed = body_line;
                            while (*body_trimmed == ' ' || *body_trimmed == '\t') body_trimmed++;
                            if (*body_trimmed && *body_trimmed != '}' && *body_trimmed != '\0') {
                                execute_single_command(body_trimmed);
                            }
                        }
make                        
                        // Add delay between iterations
                        for (volatile int delay = 0; delay < 20000000; delay++);
                    }
                    
                    if (*line_end == '}') line_end++;
                    line_start = line_end;
                    continue;
                }
            } else {
                execute_single_command(trimmed);
            }
        }
        
        // Move to next line
        while (*line_end && (*line_end == '\n' || *line_end == '\r')) line_end++;
        line_start = line_end;
    }
}

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

char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0x8A, 0x8B
};

char scancode_ascii_shifted[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    0, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0, 0, 0, 0, 0, '_', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0x8A, 0x8B
};

char get_key() {
    uint8_t scancode;

    while (1) {
        scancode = inb(0x60);
        
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            continue;
        }
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
            continue;
        }
        
        if (!(scancode & 0x80)) {
            if (scancode == 0x3B) return 0x80;
            if (scancode == 0x3C) return 0x81;
            if (scancode == 0x3D) return 0x82;
            if (scancode == 0x3E) return 0x83;
            if (scancode == 0x3F) return 0x84;
            if (scancode == 0x40) return 0x85;
            if (scancode == 0x41) return 0x86;
            if (scancode == 0x42) return 0x87;
            if (scancode == 0x43) return 0x88;
            if (scancode == 0x44) return 0x89;
            if (scancode == 0x57) return 0x8A;
            if (scancode == 0x58) return 0x8B;
            
            // Arrow keys (extended scancodes)
            if (scancode == 0xE0) {
                // Wait for the second byte
                while (!(inb(0x64) & 1));
                uint8_t extended = inb(0x60);
                if (!(extended & 0x80)) {
                    if (extended == 0x48) return 0x90; // Up arrow
                    if (extended == 0x50) return 0x91; // Down arrow
                    if (extended == 0x4B) return 0x92; // Left arrow
                    if (extended == 0x4D) return 0x93; // Right arrow
                }
                continue;
            }
            
            // Regular arrow keys (non-extended)
            if (scancode == 0x48) return 0x90; // Up arrow
            if (scancode == 0x50) return 0x91; // Down arrow
            if (scancode == 0x4B) return 0x92; // Left arrow
            if (scancode == 0x4D) return 0x93; // Right arrow
            
            while ((inb(0x64) & 1) == 0);
            while (!(inb(0x60) & 0x80));
            
            if (scancode < sizeof(scancode_ascii)) {
                if (shift_pressed) {
                    return scancode_ascii_shifted[scancode];
                } else {
                    return scancode_ascii[scancode];
                }
            }
        }
    }
    return 0;
}

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


void text_editor(const char* fname) {
    char buffer[512];
    int size = read_file(fname, buffer, sizeof(buffer) - 1);
    if (size < 0) {
        buffer[0] = '\0';
        size = 0;
    } else {
        buffer[size] = '\0';
    }

    int buf_len = size;
    int cursor_pos = buf_len;

    while (1) {
        clear_graphics(VGA_BLUE);
        draw_string(10, 10, "Simple Text Editor (ESC=save&exit, arrows=move)", VGA_WHITE);
        
        // Draw the text content
        draw_string(10, 30, buffer, VGA_WHITE);

        // Calculate cursor position accounting for newlines
        int line = 0;
        int col = 0;
        for (int i = 0; i < cursor_pos; i++) {
            if (buffer[i] == '\n') {
                line++;
                col = 0;
            } else {
                col++;
            }
        }
        
        int cursor_x = 10 + col * 8;
        int cursor_y = 30 + line * 8;
        draw_char(cursor_x, cursor_y, '_', VGA_LIGHT_GREEN);

        char c = get_key();

        if (c == 27) {
            buffer[buf_len] = '\0';
            int result = write_file(fname, buffer, buf_len);
            if (result >= 0) {
                clear_graphics(VGA_GREEN);
                draw_string(10, 10, "File saved successfully!", VGA_WHITE);
                get_key();
            }
            break;
        }
        else if (c == 0x90) { // Up arrow
            // Move to previous line
            if (cursor_pos > 0) {
                int current_col = col;
                cursor_pos--;
                while (cursor_pos > 0 && buffer[cursor_pos] != '\n') {
                    cursor_pos--;
                }
                if (buffer[cursor_pos] == '\n') cursor_pos++;
                
                // Try to maintain column position
                int new_col = 0;
                while (cursor_pos < buf_len && buffer[cursor_pos] != '\n' && new_col < current_col) {
                    cursor_pos++;
                    new_col++;
                }
            }
        }
        else if (c == 0x91) { // Down arrow
            // Move to next line
            int current_col = col;
            while (cursor_pos < buf_len && buffer[cursor_pos] != '\n') {
                cursor_pos++;
            }
            if (cursor_pos < buf_len) cursor_pos++; // Skip newline
            
            // Try to maintain column position
            int new_col = 0;
            while (cursor_pos < buf_len && buffer[cursor_pos] != '\n' && new_col < current_col) {
                cursor_pos++;
                new_col++;
            }
        }
        else if (c == 0x92) { // Left arrow
            if (cursor_pos > 0) cursor_pos--;
        }
        else if (c == 0x93) { // Right arrow
            if (cursor_pos < buf_len) cursor_pos++;
        }
        else if (c == '\n') {
            if (buf_len < sizeof(buffer) - 1) {
                for (int i = buf_len; i > cursor_pos; i--) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[cursor_pos] = '\n';
                buf_len++;
                cursor_pos++;
            }
        }
        else if (c == '\b') {
            if (cursor_pos > 0) {
                for (int i = cursor_pos - 1; i < buf_len - 1; i++) {
                    buffer[i] = buffer[i + 1];
                }
                buf_len--;
                cursor_pos--;
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
            }
        }
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
    
    // Initialize filesystem
    
    
    // Draw shell prompt
    draw_string(10, 10, "Graphics OS Shell", VGA_WHITE);
    draw_string(10, 30, "> ", VGA_WHITE);
    
    char cmd[80] = { 0 };
    int cmd_pos = 0;
    int cursor_x = 26; // After "> "
    cursor_y = 30;  // Use the global cursor_y  // Use the global cursor_y
    
    init_filesystem();
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
            else if (strncmp(cmd, "bash ", 5) == 0) {
                char* fname = cmd + 5;
                execute_bash_file(fname);
                cursor_y += 16;
            }
            else if (strcmp(cmd, "list") == 0) {
                draw_string(10, cursor_y, "Files:", VGA_WHITE);
                cursor_y += 16;
                list_files();  // Now it can access global cursor_y  // Now it can access global cursor_y
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
            else if (strcmp(cmd, "install") == 0) {
                install_os_to_disk();
                cursor_y += 32;
            }
            else if (strncmp(cmd,"help", 4)== 0 || strncmp(cmd,"info", 4)== 0|| strncmp(cmd,"i", 4)== 0) {
                draw_string(10, cursor_y, "Commands: \nedit(works but save doesnt), \nlist(doesnt work), \ncat file(doesntwork), \nrect xpos y pos width height color,\ncube xpos ypos width height \ncolor darkcolor brightcolor,\n clear", VGA_WHITE);
                cursor_y += 65;
            }
            else {
                // Check if command ends with .bash and execute as bash file
                int cmd_len = strlen(cmd);
                if (cmd_len > 5 && strcmp(cmd + cmd_len - 5, ".bash") == 0) {
                    execute_bash_file(cmd);
                    cursor_y += 16;
                }
                else {
                    draw_string(10, cursor_y, "Unknown command", VGA_WHITE);
                    cursor_y += 16;
                }
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






























