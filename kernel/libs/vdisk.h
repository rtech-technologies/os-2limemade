#ifndef VDISK_H
#define VDISK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t sector_size;
    uint64_t total_lba;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    bool is_atapi;
} vdisk_node_t;

void register_hardware_disk(vdisk_node_t node);
void vdisk_connect(int hw_id);
int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int get_hw_disk_count(void);
int get_connect_disk_count(void);
int vdisk_read_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
bool vdisk_is_atapi(int hw_id);
int is_sovereign_disk(int disk_id);

#endif
