import sys
import struct
import uuid
import os

def to_83_name(name):
    """Converts a filename to FAT 8.3 format."""
    parts = name.split('.')
    base = parts[0].upper()[:8]
    ext = parts[1].upper()[:3] if len(parts) > 1 else ""
    return base.ljust(8) + ext.ljust(3)

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]
    sector_size = 512
    part_offset = 2048           # The Sovereign Offset
    sectors_per_cluster = 8
    reserved_sectors = 32
    num_fats = 2
    fat_size = 128               # Sectors per FAT

    # Files to include in the Right-Sized ramdisk
    files_to_include = ["kernel.elf", "boot/limine.cfg", "wm.bin", "text_editor.bin"]
    # Add any other .bin files if they exist in programs/ or root
    for f in os.listdir("."):
        if f.endswith(".bin") and f not in files_to_include:
            files_to_include.append(f)
    if os.path.exists("programs"):
        for f in os.listdir("programs"):
            if f.endswith(".bin") and f not in files_to_include:
                files_to_include.append(f)

    total_file_size = 0
    valid_files = []
    for f_name in files_to_include:
        p = f_name
        base_name = os.path.basename(f_name)
        if not os.path.exists(p):
            p = os.path.join("programs", base_name)
        if not os.path.exists(p):
            p = os.path.join("iso_root", "boot", base_name)

        if os.path.exists(p):
            sz = os.path.getsize(p)
            total_file_size += sz
            valid_files.append((base_name, p, sz))
        else:
            print(f"Warning: File {f_name} not found, skipping.")

    # Calculate required sectors
    # Root directory (1 cluster) + Files
    required_clusters = 1 # Start with root dir
    for name, p, sz in valid_files:
        required_clusters += (sz + (sectors_per_cluster * sector_size) - 1) // (sectors_per_cluster * sector_size)

    # Ensure FAT is large enough to map all clusters
    # 4 bytes per cluster in FAT32. fat_size * 512 / 4 clusters mapped.
    # 128 * 512 / 4 = 16384 clusters. Plenty for our small ramdisk.

    required_data_sectors = required_clusters * sectors_per_cluster
    total_sectors = part_offset + reserved_sectors + (num_fats * fat_size) + required_data_sectors + 33
    img_size = total_sectors * sector_size

    part_sectors = total_sectors - part_offset - 33

    with open(img_path, "wb") as f:
        f.write(b'\0' * img_size)
        f.seek(0)

        # 1. LBA 0: Sovereign Signature
        f.write(struct.pack("<I", 0x5056524E))
        f.seek(446)
        f.write(b'\x00\x00\x02\x00\xEE\xFF\xFF\xFF')
        f.write(struct.pack("<I", 1))
        f.write(struct.pack("<I", total_sectors - 1))
        f.seek(510)
        f.write(b'\x55\xAA')

        # 2. GPT (LBAs 1-33)
        entries = bytearray(128 * 128)
        entries[0:16] = uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le
        entries[16:32] = uuid.uuid4().bytes_le
        entries[32:40] = struct.pack("<Q", part_offset)
        entries[40:48] = struct.pack("<Q", part_offset + part_sectors - 1)
        entries[56:128] = "Sovereign".encode('utf-16le')

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

        import zlib
        header[88:92] = struct.pack("<I", zlib.crc32(entries) & 0xFFFFFFFF)
        header[16:20] = b'\x00\x00\x00\x00'
        header[16:20] = struct.pack("<I", zlib.crc32(header) & 0xFFFFFFFF)
        f.write(header)
        f.seek(2 * sector_size)
        f.write(entries)

        # 3. BPB (FAT32)
        f.seek(part_offset * sector_size)
        f.write(b'\xEB\x58\x90')
        f.seek(part_offset * sector_size + 3)
        f.write(b'SVRN2.0 ')
        f.seek(part_offset * sector_size + 11)
        f.write(struct.pack("<H", sector_size))
        f.write(struct.pack("<B", sectors_per_cluster))
        f.write(struct.pack("<H", reserved_sectors))
        f.write(struct.pack("<B", num_fats))
        f.seek(part_offset * sector_size + 32)
        f.write(struct.pack("<I", part_sectors))
        f.write(struct.pack("<I", fat_size))
        f.seek(part_offset * sector_size + 44)
        f.write(struct.pack("<I", 2)) # Root Cluster
        f.seek(part_offset * sector_size + 510)
        f.write(b'\x55\xAA')

        # 4. FAT Tables
        fat = [0] * (fat_size * sector_size // 4)
        fat[0] = 0x0FFFFFF8
        fat[1] = 0xFFFFFFFF
        fat[2] = 0x0FFFFFFF # End of chain for Cluster 2 (Root Dir)

        # 5. Data Area (Injected Files + Dir Entries)
        data_start_lba = part_offset + reserved_sectors + (num_fats * fat_size)

        # Root Directory Entries
        root_dir = bytearray()
        current_cluster = 3

        for name, src_p, sz in valid_files:
            # Create Directory Entry
            entry = bytearray(32)
            entry[0:11] = to_83_name(name).encode('ascii')
            entry[11] = 0x20 # Archive
            entry[26:28] = struct.pack("<H", current_cluster & 0xFFFF)
            entry[20:22] = struct.pack("<H", (current_cluster >> 16) & 0xFFFF)
            entry[28:32] = struct.pack("<I", sz)
            root_dir.extend(entry)

            # Write File Data
            with open(src_p, "rb") as sf:
                data = sf.read()
                f.seek((data_start_lba + (current_cluster - 2) * sectors_per_cluster) * sector_size)
                f.write(data)

            # Update FAT Chain
            clusters_needed = (sz + (sectors_per_cluster * sector_size) - 1) // (sectors_per_cluster * sector_size)
            for i in range(clusters_needed):
                if i == clusters_needed - 1:
                    fat[current_cluster + i] = 0x0FFFFFFF
                else:
                    fat[current_cluster + i] = current_cluster + i + 1

            current_cluster += clusters_needed

        # Write Root Directory to Cluster 2
        f.seek(data_start_lba * sector_size)
        f.write(root_dir)

        # Write FAT Tables
        fat_binary = struct.pack(f"<{len(fat)}I", *fat)
        for i in range(num_fats):
            f.seek((part_offset + reserved_sectors + (i * fat_size)) * sector_size)
            f.write(fat_binary)

    print(f"OSX2: {img_size // 1024}KB Right-Sized Sovereign Disk Created at {img_path} with {len(valid_files)} files.")

if __name__ == "__main__":
    main()
