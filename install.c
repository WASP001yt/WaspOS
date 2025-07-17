#include "disk.h"
#include "graphics.h"
#include "string.h"

#define MBR_SIGNATURE 0xAA55
#define PARTITION_BOOTABLE 0x80
#define CUSTOM_OS_TYPE 0x88  // Custom OS filesystem type
#define MAX_FILES 64
#define FILENAME_SIZE 32

typedef struct {
    char name[FILENAME_SIZE];
    uint32_t start_sector;
    uint32_t size;
    uint8_t used;
} file_entry_t;

typedef struct {
    uint8_t status;
    uint8_t first_head;
    uint8_t first_sector;
    uint8_t first_cylinder;
    uint8_t type;
    uint8_t last_head;
    uint8_t last_sector;
    uint8_t last_cylinder;
    uint32_t first_lba;
    uint32_t sector_count;
} __attribute__((packed)) partition_entry_t;

typedef struct {
    uint8_t bootcode[446];
    partition_entry_t partitions[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

void install_os_to_disk() {
    draw_string(10, 50, "Installing Graphics OS to disk...", VGA_YELLOW);
    
    // Create MBR with partition table for GRUB
    mbr_t mbr = {0};
    
    // Set up first partition (bootable) - Linux filesystem for GRUB
    mbr.partitions[0].status = PARTITION_BOOTABLE;
    mbr.partitions[0].type = 0x83; // Linux filesystem (GRUB compatible)
    mbr.partitions[0].first_lba = 2048; // Start at 1MB
    mbr.partitions[0].sector_count = 20480; // 10MB partition
    
    mbr.signature = MBR_SIGNATURE;
    
    draw_string(10, 70, "Writing MBR...", VGA_CYAN);
    
    // Write MBR to sector 0 of target disk
    write_sector(0, (uint8_t*)&mbr);
    
    draw_string(10, 90, "Setting up filesystem...", VGA_CYAN);
    
    // Initialize filesystem on the partition
    file_entry_t empty_fat[MAX_FILES] = {0};
    write_sector(2048, (uint8_t*)empty_fat);
    
    draw_string(10, 110, "Graphics OS partition created!", VGA_GREEN);
    draw_string(10, 130, "Run: grub-install --target=i386-pc", VGA_WHITE);
    draw_string(10, 150, "      --boot-directory=/mnt/boot disk.img", VGA_WHITE);
    draw_string(10, 170, "Then copy kernel.elf to /mnt/boot/", VGA_WHITE);
}









