#include "disk.h"
#include <stdint.h>
#include "string.h"
#include "graphics.h"

extern void puts(const char*);
extern int cursor_y;

#define SECTOR_SIZE 512
#define MAX_FILES 64
#define FILENAME_SIZE 32

// Add inline functions at the top
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

typedef struct {
    char name[FILENAME_SIZE];
    uint32_t start_sector;
    uint32_t size;
    uint8_t used;
} file_entry_t;

// File allocation table stored in sector 0
static file_entry_t file_table[MAX_FILES];

void init_filesystem() {
    // Try to read file table from disk sector 0
    read_sector(0, (uint8_t*)file_table);
    
    // Check if the file table is valid (first entry should have a reasonable name or be empty)
    // If sector 0 is uninitialized, it will be all zeros or garbage
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used) {
            // Check if name contains valid characters
            int name_valid = 1;
            for (int j = 0; j < FILENAME_SIZE && file_table[i].name[j]; j++) {
                if (file_table[i].name[j] < 32 || file_table[i].name[j] > 126) {
                    name_valid = 0;
                    break;
                }
            }
            if (name_valid) {
                valid = 1;
                break;
            }
        }
    }
    
    // If file table is invalid, initialize it as empty
    if (!valid) {
        for (int i = 0; i < MAX_FILES; i++) {
            file_table[i].used = 0;
            file_table[i].name[0] = '\0';
            file_table[i].start_sector = 0;
            file_table[i].size = 0;
        }
        // Write the empty file table to disk
        write_sector(0, (uint8_t*)file_table);
    }
}

int read_file(const char* name, char* out, int max_size) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && strcmp(name, file_table[i].name) == 0) {
            uint32_t sectors_to_read = (file_table[i].size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            uint8_t sector_buffer[SECTOR_SIZE];
            int bytes_read = 0;
            
            for (uint32_t j = 0; j < sectors_to_read && bytes_read < max_size; j++) {
                read_sector(file_table[i].start_sector + j, sector_buffer);
                
                int copy_size = file_table[i].size - bytes_read;
                if (copy_size > SECTOR_SIZE) copy_size = SECTOR_SIZE;
                if (copy_size > max_size - bytes_read) copy_size = max_size - bytes_read;
                
                for (int k = 0; k < copy_size; k++) {
                    out[bytes_read + k] = sector_buffer[k];
                }
                bytes_read += copy_size;
            }
            return bytes_read;
        }
    }
    return -1;
}

int write_file(const char* name, const char* data, int size) {
    // Find existing file or create new one
    int file_index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && strcmp(name, file_table[i].name) == 0) {
            file_index = i;
            break;
        }
    }
    
    // If not found, find free slot
    if (file_index == -1) {
        for (int i = 0; i < MAX_FILES; i++) {
            if (!file_table[i].used) {
                file_index = i;
                break;
            }
        }
    }
    
    if (file_index == -1) return -1; // No free slots
    
    // Calculate sectors needed
    uint32_t sectors_needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    
    // Find free sectors (simple allocation starting from sector 1)
    uint32_t start_sector = 1; // Sector 0 is for file table
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && file_table[i].start_sector >= start_sector) {
            start_sector = file_table[i].start_sector + 
                          ((file_table[i].size + SECTOR_SIZE - 1) / SECTOR_SIZE);
        }
    }
    
    // Update file table entry
    strcpy(file_table[file_index].name, name);
    file_table[file_index].start_sector = start_sector;
    file_table[file_index].size = size;
    file_table[file_index].used = 1;
    
    // Write file data to sectors
    uint8_t sector_buffer[SECTOR_SIZE];
    for (uint32_t i = 0; i < sectors_needed; i++) {
        // Clear buffer
        for (int j = 0; j < SECTOR_SIZE; j++) {
            sector_buffer[j] = 0;
        }
        
        // Copy data to buffer
        int copy_size = (size > SECTOR_SIZE) ? SECTOR_SIZE : size;
        for (int j = 0; j < copy_size; j++) {
            sector_buffer[j] = data[j];
        }
        
        write_sector(start_sector + i, sector_buffer);
        data += copy_size;
        size -= copy_size;
    }
    
    // Write updated file table back to sector 0
    write_sector(0, (uint8_t*)file_table);
    
    return file_table[file_index].size;
}

void list_files() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used) {
            draw_string(10, cursor_y, file_table[i].name, VGA_WHITE);
            cursor_y += 16;
        }
    }
}

int get_file_name(int index, char* name) {
    if (index >= 0 && index < MAX_FILES && file_table[index].used) {
        int i = 0;
        while (file_table[index].name[i] && i < FILENAME_SIZE - 1) {
            name[i] = file_table[index].name[i];
            i++;
        }
        name[i] = 0;
        return 1;
    }
    return 0;
}

void write_sector(uint32_t lba, uint8_t* buffer) {
    // Wait for drive to be ready
    while (inb(0x1F7) & 0x80);
    
    // Set up LBA addressing
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);  // Sector count
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F7, 0x30);  // Write command
    
    // Wait for ready
    while (!(inb(0x1F7) & 0x08));
    
    // Write 512 bytes
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ((uint16_t*)buffer)[i]);
    }
}

void read_sector(uint32_t lba, uint8_t* buffer) {
    // Wait for drive to be ready
    while (inb(0x1F7) & 0x80);
    
    // Set up LBA addressing
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);  // Sector count
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F7, 0x20);  // Read command
    
    // Wait for data ready
    while (!(inb(0x1F7) & 0x08));
    
    // Read 512 bytes (256 words)
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = inw(0x1F0);
    }
}
