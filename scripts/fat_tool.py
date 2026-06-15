import sys
import struct
import uuid
import os
import zlib

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]
    img_size = 64 * 1024 * 1024  # 64MB
    sector_size = 512
    total_sectors = img_size // sector_size

    # 🎯 Sentry Fix: Real Partitioning (ESP + Sovereign Data)
    esp_offset = 2048
    esp_sectors = 32768 # 16MB
    data_offset = esp_offset + esp_sectors
    data_sectors = total_sectors - data_offset - 33

    cluster_size = 4096 # 8 sectors per cluster
    fat_size_sectors = 128
    reserved_sectors = 32
    root_cluster = 2

    # Data region for Sovereign Partition
    data_region_lba = data_offset + reserved_sectors + (2 * fat_size_sectors)

    with open(img_path, "wb") as f:
        f.seek(img_size - 1)
        f.write(b'\0')
        f.seek(0)

        # Protective MBR
        f.write(struct.pack("<I", 0xEFBEADDE))
        f.seek(446)
        # Part 1: ESP
        f.write(b'\x00\x00\x02\x00\xEF\xFF\xFF\xFF') # Type 0xEF for ESP
        f.write(struct.pack("<I", esp_offset))
        f.write(struct.pack("<I", esp_sectors))
        # Part 2: Sovereign
        f.write(b'\x80\x00\x02\x00\xEE\xFF\xFF\xFF') # Bootable, Type 0xEE for GPT
        f.write(struct.pack("<I", data_offset))
        f.write(struct.pack("<I", data_sectors))
        f.seek(510)
        f.write(b'\x55\xAA')

        # GPT Entries
        entries = bytearray(128 * 128)
        # Entry 1: ESP
        entries[0:16] = uuid.UUID('C12A7328-F81F-11D2-BA4B-00A0C93EC93B').bytes_le
        entries[16:32] = uuid.uuid4().bytes_le
        entries[32:40] = struct.pack("<Q", esp_offset)
        entries[40:48] = struct.pack("<Q", esp_offset + esp_sectors - 1)
        entries[56:128] = "EFI System".encode('utf-16le')
        # Entry 2: Sovereign
        entries[128:144] = uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le
        entries[144:160] = uuid.uuid4().bytes_le
        entries[160:168] = struct.pack("<Q", data_offset)
        entries[168:176] = struct.pack("<Q", data_offset + data_sectors - 1)
        entries[184:256] = "Sovereign".encode('utf-16le')

        f.seek(2 * sector_size)
        f.write(entries)

        # GPT Header
        f.seek(1 * sector_size)
        header = bytearray(92)
        header[0:8] = b'EFI PART'
        header[8:12] = b'\x00\x00\x01\x00'
        header[12:16] = struct.pack("<I", 92)
        header[24:32] = struct.pack("<Q", 1)
        header[32:40] = struct.pack("<Q", total_sectors - 1)
        header[40:48] = struct.pack("<Q", 34)
        header[48:56] = struct.pack("<Q", total_sectors - 34)
        header[56:72] = uuid.uuid4().bytes
        header[72:80] = struct.pack("<Q", 2)
        header[80:84] = struct.pack("<I", 128)
        header[84:88] = struct.pack("<I", 128)
        header[88:92] = struct.pack("<I", zlib.crc32(entries) & 0xFFFFFFFF)
        header[16:20] = struct.pack("<I", zlib.crc32(header) & 0xFFFFFFFF)
        f.write(header)

        # BPB for Sovereign Data Partition
        f.seek(data_offset * sector_size)
        f.write(b'\xEB\x58\x90')
        f.seek(data_offset * sector_size + 3)
        f.write(b'OSX2.0  ')
        f.seek(data_offset * sector_size + 11)
        f.write(struct.pack("<H", sector_size))
        f.write(struct.pack("<B", cluster_size // sector_size))
        f.write(struct.pack("<H", reserved_sectors))
        f.write(struct.pack("<B", 2))
        f.seek(data_offset * sector_size + 32)
        f.write(struct.pack("<I", data_sectors))
        f.write(struct.pack("<I", fat_size_sectors))
        f.write(struct.pack("<I", root_cluster))
        f.seek(data_offset * sector_size + 510)
        f.write(b'\x55\xAA')

        # Init FAT
        for i in range(2):
            f.seek((data_offset + reserved_sectors + (i * fat_size_sectors)) * sector_size)
            f.write(struct.pack("<I", 0x0FFFFFF8))
            f.write(struct.pack("<I", 0xFFFFFFFF))
            f.write(struct.pack("<I", 0x0FFFFFFF))

        # Injection
        next_free_cluster = 3

        def allocate_clusters(n):
            nonlocal next_free_cluster
            start = next_free_cluster
            next_free_cluster += n
            return start

        def update_fat(cluster, val):
            for i in range(2):
                fat_lba = data_offset + reserved_sectors + (i * fat_size_sectors)
                f.seek(fat_lba * sector_size + cluster * 4)
                f.write(struct.pack("<I", val))

        def write_cluster_chain(data):
            num = (len(data) + cluster_size - 1) // cluster_size
            if num == 0: num = 1
            start = allocate_clusters(num)
            for i in range(num):
                c = start + i
                f.seek((data_region_lba + (c - 2) * (cluster_size // sector_size)) * sector_size)
                chunk = data[i*cluster_size : (i+1)*cluster_size]
                f.write(chunk)
                if i == num - 1:
                    update_fat(c, 0x0FFFFFFF)
                else:
                    update_fat(c, c + 1)
            return start

        def create_dirent(name, is_dir, cluster, size):
            ent = bytearray(32)
            parts = name.split('.')
            main_name = parts[0][:8].upper().ljust(8)
            ext = parts[1][:3].upper().ljust(3) if len(parts) > 1 else "   "
            ent[0:8] = main_name.encode('ascii', 'ignore')
            ent[8:11] = ext.encode('ascii', 'ignore')
            ent[11] = 0x10 if is_dir else 0x20
            ent[20:22] = struct.pack("<H", (cluster >> 16) & 0xFFFF)
            ent[26:28] = struct.pack("<H", cluster & 0xFFFF)
            ent[28:32] = struct.pack("<I", size)
            return ent

        def inject_dir(local_path):
            dir_entries = bytearray()
            for item in sorted(os.listdir(local_path)):
                if item == "ramdisk.img": continue # Skip self
                full = os.path.join(local_path, item)
                if os.path.isdir(full):
                    c = inject_dir(full)
                    dir_entries.extend(create_dirent(item, True, c, 0))
                else:
                    with open(full, "rb") as fi:
                        data = fi.read()
                        c = write_cluster_chain(data)
                        dir_entries.extend(create_dirent(item, False, c, len(data)))

            return write_cluster_chain(dir_entries)

        if os.path.exists("iso_root"):
            # Root directory is special, it's cluster 2.
            # We need to build its entries and write them to cluster 2.
            dir_entries = bytearray()
            for item in sorted(os.listdir("iso_root")):
                if item == "ramdisk.img": continue
                full = os.path.join("iso_root", item)
                if os.path.isdir(full):
                    c = inject_dir(full)
                    dir_entries.extend(create_dirent(item, True, c, 0))
                else:
                    with open(full, "rb") as fi:
                        data = fi.read()
                        c = write_cluster_chain(data)
                        dir_entries.extend(create_dirent(item, False, c, len(data)))

            # Write root dir to cluster 2
            f.seek(data_region_lba * sector_size)
            f.write(dir_entries)

    print(f"OSX2: 64MB Dual-Partition GPT Disk Created at {img_path}")

if __name__ == "__main__":
    main()
