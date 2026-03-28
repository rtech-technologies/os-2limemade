import sys
import struct

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]
    img_size = 64 * 1024 * 1024
    sector_size = 512
    partition_start = 2048 # LBA 2048 Standard

    with open(img_path, "wb") as f:
        f.seek(img_size - 1)
        f.write(b'\0')

    with open(img_path, "r+b") as f:
        # LBA 0: Sovereign Signature (MBR Protected)
        f.write(struct.pack("<I", 0xEFBEADDE))

        # LBA 1: Placeholder for GPT Primary Header
        f.seek(1 * sector_size)
        f.write(b'EFI PART') # GPT Magic

        # LBA 2048: FAT32 Boot Sector (BPB)
        f.seek(partition_start * sector_size)
        f.write(b'\xEB\x58\x90') # Jump
        f.seek(partition_start * sector_size + 3)
        f.write(b'MSDOS5.0')
        f.seek(partition_start * sector_size + 11)
        f.write(struct.pack("<H", 512)) # Bytes per sector
        f.write(struct.pack("<B", 8))   # Sectors per cluster
        f.write(struct.pack("<H", 32))  # Reserved sectors
        f.write(struct.pack("<B", 2))   # Number of FATs
        f.seek(partition_start * sector_size + 32)
        f.write(struct.pack("<I", (img_size // sector_size) - partition_start)) # Total sectors
        f.write(struct.pack("<I", 2048)) # Sectors per FAT
        f.write(struct.pack("<I", 2))    # Root cluster
        f.seek(partition_start * sector_size + 510)
        f.write(b'\x55\xAA') # Boot Signature

        # LBA 2048 + 32: FAT Table 1
        f.seek((partition_start + 32) * sector_size)
        f.write(struct.pack("<I", 0x0FFFFFF8)) # FAT[0]
        f.write(struct.pack("<I", 0xFFFFFFFF)) # FAT[1]
        f.write(struct.pack("<I", 0x0FFFFFFF)) # FAT[2] (Root EOC)

        # LBA 2048 + 32 + (2 * 2048) = LBA 6176: Data Area (Cluster 2)
        root_lba = partition_start + 32 + (2 * 2048)
        f.seek(root_lba * sector_size)

        # Root Dir Entry 1: "BIN" Directory
        name = b'BIN        '
        f.write(name + b'\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00')

        # Root Dir Entry 2: "BOOT    RSL"
        name = b'BOOT    RSL'
        f.write(name + b'\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00')

        # Cluster 3: BIN directory content
        f.seek((root_lba + 8) * sector_size)
        name = b'INSTALL RSL'
        f.write(name + b'\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00')

        # Cluster 4: BOOT.RSL content
        f.seek((root_lba + 16) * sector_size)
        f.write(b"print('OSx2 Sovereign: System Environment Initialized.');\n")

        # Cluster 5: INSTALL.RSL content
        f.seek((root_lba + 24) * sector_size)
        f.write(b"print('Preparing Sovereign Installation...');\n")

    print(f"OSx2 Limemade FAT32 Disk Image {img_path} created with GPT space and LBA 2048 alignment.")

if __name__ == "__main__":
    main()
