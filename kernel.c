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
#define KEY_REPEAT_DELAY 1400000    // Initial delay in ticks (adjust based on your timer frequency)
#define KEY_REPEAT_RATE 150000     // Repeat rate in ticks
static int ctrl_pressed = 0;
// Add these variables at the top of your file with other globals
static uint8_t last_key_pressed = 0;
static uint32_t key_press_time = 0;
static uint32_t key_repeat_time = 0;
static uint8_t key_repeating = 0;
static uint32_t system_tick = 0;
int bg_color = 1; // Default background color
int fg_color = 31; // Default background color
// Simple timer increment function - call this from your timer interrupt
void increment_system_tick() {
    system_tick++;
}

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

void clear_all_variables() {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        variables[i].used = 0;
    }
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
    // Parse variable name - stop at space, tab, or operator characters
    while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '+' && *ptr != '-' && *ptr != '=' && name_len < MAX_VAR_NAME - 1) {
        var_name[name_len++] = *ptr++;
    }
    var_name[name_len] = '\0';

    // Skip any spaces after variable name
    while (*ptr == ' ' || *ptr == '\t') ptr++;

    // Handle += operator
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

    // Handle -= operator
    if (*ptr == '-' && *(ptr + 1) == '=') {
        ptr += 2;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        int decrement = atoi(ptr);
        const char* current_val = get_variable(var_name);
        int current = current_val ? atoi(current_val) : 0;

        char new_val[32];
        itoa(current - decrement, new_val);
        set_variable(var_name, new_val);
        return 1;
    }

    // Handle ++ operator (both ++var and var++)
    if (*ptr == '+' && *(ptr + 1) == '+') {
        const char* current_val = get_variable(var_name);
        int current = current_val ? atoi(current_val) : 0;

        char new_val[32];
        itoa(current + 1, new_val);
        set_variable(var_name, new_val);
        return 1;
    }

    // Handle -- operator (both --var and var--)
    if (*ptr == '-' && *(ptr + 1) == '-') {
        const char* current_val = get_variable(var_name);
        int current = current_val ? atoi(current_val) : 0;

        char new_val[32];
        itoa(current - 1, new_val);
        set_variable(var_name, new_val);
        return 1;
    }

    // Handle simple assignment = operator
    if (*ptr == '=' && *(ptr + 1) != '=') {
        ptr++;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        char var_value[MAX_VAR_VALUE];
        int value_len = 0;
        while (*ptr && *ptr != '\n' && *ptr != '\r' && value_len < MAX_VAR_VALUE - 1) {
            var_value[value_len++] = *ptr++;
        }
        var_value[value_len] = '\0';

        set_variable(var_name, var_value);
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

            // Check if this variable is followed by assignment operators
            // If so, don't substitute it (handle spaces around operators)
            int is_assignment = 0;
            const char* check_ptr = ptr;
            // Skip spaces after variable name
            while (*check_ptr == ' ' || *check_ptr == '\t') check_ptr++;

            if (*check_ptr == '+' && *(check_ptr+1) == '+') is_assignment = 1;  // ++
            if (*check_ptr == '-' && *(check_ptr+1) == '-') is_assignment = 1;  // --
            if (*check_ptr == '+' && *(check_ptr+1) == '=') is_assignment = 1;  // +=
            if (*check_ptr == '-' && *(check_ptr+1) == '=') is_assignment = 1;  // -=
            if (*check_ptr == '*' && *(check_ptr+1) == '=') is_assignment = 1;  // *=
            if (*check_ptr == '/' && *(check_ptr+1) == '=') is_assignment = 1;  // /=
            if (*check_ptr == '=' && *(check_ptr+1) != '=') is_assignment = 1;  // = (but not ==)

            const char* var_value = get_variable(var_name);
            if (var_value && !is_assignment) {
                // Substitute variable value only if not an assignment
                int val_len = strlen(var_value);
                if (out_pos + val_len < max_len - 1) {
                    for (int i = 0; i < val_len; i++) {
                        output[out_pos++] = var_value[i];
                    }
                }
            } else {
                // Not a variable or is assignment, copy original text
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
int val_x = 10;
int execute_single_command(const char* cmd) {
    char substituted_cmd[256];
    substitute_variables(cmd, substituted_cmd, sizeof(substituted_cmd));
    
    if (parse_let_command(substituted_cmd)) {
        return 1;
    }
    
    if (parse_assignment(substituted_cmd)) {
        return 1;
    }
    
    // Handle print/echo command
    if (strncmp(substituted_cmd, "print ", 6) == 0 || strncmp(substituted_cmd, "echo ", 5) == 0) {
        const char* text_start = substituted_cmd;
        if (strncmp(substituted_cmd, "print ", 6) == 0) {
            text_start += 6;
        } else {
            text_start += 5;
        }

        // Skip leading spaces
        while (*text_start == ' ' || *text_start == '\t') text_start++;
        // Display the text - use a safe position that won't interfere with shell
        draw_string(val_x, cursor_y, text_start, fg_color);
        cursor_y += 8;
        if (cursor_y > 150) {
            cursor_y = 50;
            val_x += 50;
        }
        if (val_x > 180) {
            clear_graphics(bg_color);
            val_x = 10;
        }
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
// Solution 1: Add a function to wait for any key press (not release)
char wait_for_key_press() {
    char c;
    do {
        c = get_key();
        // Small delay to prevent consuming too much CPU
        for (volatile int i = 0; i < 1000; i++);
        increment_system_tick();
    } while (c == 0);
    return c;
}

// Solution 2: Reset key repeat state before waiting
void reset_key_repeat_state() {
    last_key_pressed = 0;
    key_press_time = 0;
    key_repeat_time = 0;
    key_repeating = 0;
}
void execute_bash_file(const char* fname) {
    // Clear all variables at the start of script execution
    clear_all_variables();

    char buffer[2048];
    int size = read_file(fname, buffer, sizeof(buffer) - 1);
    if (size < 0) {
        draw_string(10, cursor_y, "File not found", fg_color);
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
                    for (int loop_iter = start_val; loop_iter <= end_val; loop_iter++) {
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
                        // Add delay between iterations
                        for (volatile int delay = 0; delay < 20000000; delay++);
                    }
                    
                    if (*line_end == '}') line_end++;
                    line_start = line_end;
                    continue;
                } else {
                    draw_string(10, cursor_y, "ERROR: No opening brace found!", VGA_RED);
                    cursor_y += 16;
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
char convert_scancode_to_char(uint8_t scancode) {
    // Handle function keys
    if (scancode == 0x3B) return 0x80; // F1
    if (scancode == 0x3C) return 0x81; // F2
    if (scancode == 0x3D) return 0x82; // F3
    if (scancode == 0x3E) return 0x83; // F4
    if (scancode == 0x3F) return 0x84; // F5
    if (scancode == 0x40) return 0x85; // F6
    if (scancode == 0x41) return 0x86; // F7
    if (scancode == 0x42) return 0x87; // F8
    if (scancode == 0x43) return 0x88; // F9
    if (scancode == 0x44) return 0x89; // F10
    if (scancode == 0x57) return 0x8A; // F11
    if (scancode == 0x58) return 0x8B; // F12
    
    // Handle arrow keys
    if (scancode == 0x48) return 0x90; // Up arrow
    if (scancode == 0x50) return 0x91; // Down arrow
    if (scancode == 0x4B) return 0x92; // Left arrow
    if (scancode == 0x4D) return 0x93; // Right arrow
    
    // Handle regular keys
    if (scancode < sizeof(scancode_ascii)) {
        if (shift_pressed) {
            return scancode_ascii_shifted[scancode];
        } else {
            return scancode_ascii[scancode];
        }
    }
    
    return 0;
}
char get_key() {
    uint8_t scancode;
    static uint8_t key_released = 1;  // Track if key was released

    // Check for keyboard input
    if (inb(0x64) & 1) {  // Check if keyboard data is available
        scancode = inb(0x60);
        
        // Handle shift key press/release
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            return 0;  // Don't repeat modifier keys
        }
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
            return 0;
        }
        // Handle ctrl key press/release  
        if (scancode == 0x1D) {  // Left Ctrl
            ctrl_pressed = 1;
            return 0;
        }
        if (scancode == 0x9D) {  // Left Ctrl release
            ctrl_pressed = 0;
            return 0;
        }
        // Key release (scancode has high bit set)
        if (scancode & 0x80) {
            uint8_t released_key = scancode & 0x7F;
            if (released_key == last_key_pressed) {
                key_released = 1;
                key_repeating = 0;
                last_key_pressed = 0;
            }
            return 0;
        }
        
        // Key press
        if (!(scancode & 0x80)) {
            // New key pressed
            if (scancode != last_key_pressed || key_released) {
                last_key_pressed = scancode;
                key_press_time = system_tick;
                key_repeat_time = 0;
                key_repeating = 0;
                key_released = 0;
                
                // Return the character for initial press
                return convert_scancode_to_char(scancode);
            }
        }
    }
    
    // Handle key repeat for held keys
    if (last_key_pressed && !key_released) {
        uint32_t current_time = system_tick;
        
        if (!key_repeating) {
            // Check if initial delay has passed
            if (current_time - key_press_time >= KEY_REPEAT_DELAY) {
                key_repeating = 1;
                key_repeat_time = current_time;
                return convert_scancode_to_char(last_key_pressed);
            }
        } else {
            // Check if repeat interval has passed
            if (current_time - key_repeat_time >= KEY_REPEAT_RATE) {
                key_repeat_time = current_time;
                return convert_scancode_to_char(last_key_pressed);
            }
        }
    }
    
    return 0;  // No key event
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
    int needs_redraw = 1;  // Flag to control when to redraw

    while (1) {
        char c = get_key();

        // Process key input regardless of redraw state
        if (c != 0) {
            needs_redraw = 1;  // Set redraw flag when key is processed

            if (c == 27) {
                buffer[buf_len] = '\0';
                int result = write_file(fname, buffer, buf_len);
                if (result >= 0) {
                    clear_graphics(VGA_GREEN);
                    draw_string(10, 10, "File saved successfully!", fg_color);
                    get_key();
                }
                break;
            }
            else if (c == 0x90) { // Up arrow
                // Calculate current column position
                int current_col = 0;
                int temp_pos = cursor_pos;
                
                // Move back to find the start of current line
                while (temp_pos > 0 && buffer[temp_pos - 1] != '\n') {
                    temp_pos--;
                    current_col++;
                }
                
                // Move to previous line if possible
                if (temp_pos > 0) {
                    cursor_pos = temp_pos - 1; // Move to the newline
                    
                    // Find start of previous line
                    while (cursor_pos > 0 && buffer[cursor_pos - 1] != '\n') {
                        cursor_pos--;
                    }
                    
                    // Try to maintain column position
                    int new_col = 0;
                    while (cursor_pos < buf_len && buffer[cursor_pos] != '\n' && new_col < current_col) {
                        cursor_pos++;
                        new_col++;
                    }
                }
            }
            else if (c == 0x91) { // Down arrow
                // Calculate current column position
                int current_col = 0;
                int temp_pos = cursor_pos;
                
                // Move back to find the start of current line
                while (temp_pos > 0 && buffer[temp_pos - 1] != '\n') {
                    temp_pos--;
                    current_col++;
                }
                
                // Move to next line
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
                    if (ctrl_pressed) {
                        // Ctrl+Backspace: delete until space, newline, or beginning
                        int original_pos = cursor_pos;
                        
                        // If current character before cursor is a space, delete it first
                        if (buffer[cursor_pos - 1] == ' ') {
                            cursor_pos--;
                        }
                        
                        // Then move backwards until we find a space, newline, or reach the beginning
                        while (cursor_pos > 0 && buffer[cursor_pos - 1] != ' ' && buffer[cursor_pos - 1] != '\n') {
                            cursor_pos--;
                        }
                        
                        // Remove the deleted characters from buffer
                        int chars_deleted = original_pos - cursor_pos;
                        for (int i = cursor_pos; i < buf_len - chars_deleted; i++) {
                            buffer[i] = buffer[i + chars_deleted];
                        }
                        for (int i = buf_len - chars_deleted; i < buf_len; i++) {
                            buf_len--;
                            buffer[i] = '\0';  // Clear the end of the buffer
                        }
                    } else {
                        // Regular backspace: delete one character
                        for (int i = cursor_pos - 1; i < buf_len - 1; i++) {
                            buffer[i] = buffer[i + 1];
                        }
                        buf_len--;
                        cursor_pos--;
                        buffer[buf_len] = '\0';
                    }
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

        // Redraw screen only when needed
        if (needs_redraw) {
            clear_graphics(bg_color);
            draw_string(10, 10, "Simple Text Editor (ESC=save&exit, arrows=move)", fg_color);
            draw_string(10, 30, buffer, fg_color);

            // Calculate and draw cursor
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
            
            needs_redraw = 0;
        }

        increment_system_tick();  // Increment system tick for key repeat logic
    }
}

int x, y, width, height;
int color;
int brightcolor;
int darkcolor;

void kmain() {
    // Initialize graphics mode
    init_graphics();
    clear_graphics(bg_color);
    
    // Initialize filesystem
    
    
    // Draw shell prompt
    draw_string(10, 10, "Graphics OS Shell", fg_color);
    draw_string(10, 30, "> ", fg_color);
    
    char cmd[80] = { 0 };
    int cmd_pos = 0;
    int cursor_x = 26; // After "> "
    cursor_y = 30;  // Use the global cursor_y  // Use the global cursor_y
    
    init_filesystem();
    while (1) {
        char c = get_key();
        
        if (c == '\n') {
            fill_rect(cursor_x, cursor_y, 8, 8, bg_color); // Clear the prompt area
            cmd[cmd_pos] = 0;
            cursor_y += 16; // Move to next line
            
            if (strncmp(cmd, "edit ", 5) == 0) {
                char* fname = cmd + 5;
                text_editor(fname);
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30;
            }
            else if (strncmp(cmd, "bash ", 5) == 0) {
                char* fname = cmd + 5;
                execute_bash_file(fname);
                cursor_y += 16;
            }
            else if (strcmp(cmd, "list") == 0) {
                draw_string(10, cursor_y, "Files:", fg_color);
                cursor_y += 16;
                list_files();  // Now it can access global cursor_y  // Now it can access global cursor_y
            }
            else if (strncmp(cmd, "cat ", 4) == 0) {
                char* fname = cmd + 4;
                char buf[512];
                int size = read_file(fname, buf, sizeof(buf));
                if (size >= 0) {
                    buf[size] = 0;
                    draw_string(10, cursor_y, buf, fg_color);
                    cursor_y += 16;
                }
                else {
                    draw_string(10, cursor_y, "cat: file not found", fg_color);
                    cursor_y += 16;
                }
            }
            else if (parse_rect_cmd(cmd, &x, &y, &width, &height, &color)) {
                clear_graphics(bg_color);
                
                // Draw filled rectangle with parsed color
                fill_rect(x, y, width, height, color);
                
                // Wait for key press
                wait_key_release();
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30;
            }
            else if (parse_cube_cmd(cmd, &x, &y, &width, &height, &color, &darkcolor, &brightcolor)) {
                clear_graphics(VGA_BLACK);
    
                // Calculate depth offset for 3D effect
                int depth = width / 4;
    
                // Draw the front face (main rectangle)
                fill_rect(x, y, width, height, color);
    
                // Draw the right face (darker color for shadow effect)
                for (int i = 0; i < depth; i++) {
                    fill_rect(x + width + i, y - i, 1, height, darkcolor);
                }
                fill_rect(x + width, y - depth, depth, height, darkcolor);
    
                // Draw the top face (brighter color for highlight effect)
                for (int i = 0; i < depth; i++) {
                    fill_rect(x + depth - i, y + i - depth, width, 1, brightcolor);
                }
    
                // Wait for key press
                
    
                // Reset key repeat state and wait for any key press
                reset_key_repeat_state();
                wait_for_key_press();
    
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30;
            }

            else if (strcmp(cmd, "clear") == 0) {
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30;
            }
            else if (strncmp(cmd,"help", 4)== 0 || strncmp(cmd,"info", 4)== 0|| strncmp(cmd,"i", 4)== 0) {
                draw_string(10, cursor_y, "Commands: \nedit(works but save doesnt), \nlist(doesnt work), \ncat file(doesntwork), \nrect xpos y pos width height color,\ncube xpos ypos width height \ncolor darkcolor brightcolor,\n clear", fg_color);
                cursor_y += 65;
            }
            else if (parse_bg_cmd(cmd, &color))
            {
                bg_color = color; // Reset background color
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30; // Reset cursor position
            }
            else if (parse_fg_cmd(cmd, &color))
            {
                fg_color = color; // Reset background color
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30; // Reset cursor position
            }
            else {
                // Check if command ends with .bash and execute as bash file
                int cmd_len = strlen(cmd);
                if (cmd_len > 5 && strcmp(cmd + cmd_len - 5, ".bash") == 0) {
                    execute_bash_file(cmd);
                    cursor_y += 16;
                }
                else {
                    draw_string(10, cursor_y, "Unknown command", fg_color);
                    cursor_y += 16;
                }
            }
            
            if (cursor_y >= 180) {
                clear_graphics(bg_color);
                draw_string(10, 10, "Graphics OS Shell", fg_color);
                cursor_y = 30;
            }
            
            // Draw new prompt
 
           draw_string(10, cursor_y, "> ", fg_color);
            cursor_x = 26;
            cmd_pos = 0;
        }
        else if (c == '\b') {
    if (cmd_pos > 0) {
        // Check if Ctrl is pressed (you'll need to track Ctrl key state)
        // For now, assuming you have a ctrl_pressed variable similar to shift_pressed
                if (ctrl_pressed) {
                    fill_rect(cursor_x, cursor_y, 8, 8, bg_color); // Clear the prompt area
                    // Ctrl+Backspace: delete until space or beginning
                    int original_pos = cmd_pos;
            
                    // If current character before cursor is a space, delete it first
                    if (cmd[cmd_pos - 1] == ' ') {
                        cmd_pos--;
                    }
            
                    // Then move backwards until we find a space or reach the beginning
                    while (cmd_pos > 0 && cmd[cmd_pos - 1] != ' ') {
                        cmd_pos--;
                    }
            
                    // Clear the deleted characters visually
                    int chars_deleted = original_pos - cmd_pos;
                    cursor_x -= chars_deleted * 8;
                    fill_rect(cursor_x, cursor_y, chars_deleted * 8, 8, bg_color);
                } else {
                    // Regular backspace: delete one character
                    cmd_pos--;
                    fill_rect(cursor_x, cursor_y, 8, 8, bg_color);
                    cursor_x -= 8;
                    fill_rect(cursor_x, cursor_y, 8, 8, bg_color);
                }
            }
        }
        else if (c >= 32 && c <= 126) {
            if (cmd_pos < (int)sizeof(cmd) - 1) {
                cmd[cmd_pos++] = c;
                fill_rect(cursor_x, cursor_y, 8, 8, bg_color); // Clear previous character
                draw_char(cursor_x, cursor_y, c, fg_color);
                cursor_x += 8;
            }
        }
        draw_char(cursor_x, cursor_y, '_', VGA_GREEN);
        increment_system_tick();
    }
}






























