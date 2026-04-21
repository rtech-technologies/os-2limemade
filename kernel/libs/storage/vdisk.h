#ifndef VDISK_H
#define VDISK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char name[16];
    uint32_t sector_size;
    uint64_t total_lba;
    uint64_t partition_offset;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*eject)(void* priv);
    bool is_atapi;
} vdisk_node_t;

void register_hardware_disk(vdisk_node_t node);
void vdisk_connect(int hw_id);
int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int get_hw_disk_count(void);
int get_connect_disk_count(void);
int is_sovereign_disk(int disk_id);
int vdisk_read_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_eject_hw(int hw_id);
bool vdisk_is_busy(int hw_id);
uint64_t vdisk_get_offset(int hw_id);
bool vdisk_is_readonly(int hw_id);
bool vdisk_is_atapi(int hw_id);

#endif
