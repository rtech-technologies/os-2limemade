import sys
import struct

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]
    img_size = 64 * 1024 * 1024  # 64MB
    sector_size = 512
    part_offset = 2048           # The Sovereign Offset
    total_sectors = img_size // sector_size
    part_sectors = total_sectors - part_offset

    # FAT32 Parameters
    sectors_per_cluster = 8
    reserved_sectors = 32
    num_fats = 2
    sectors_per_fat = 128        # Enough for 64MB @ 8 sectors/cluster

    with open(img_path, "wb") as f:
        # 1. Create the sparse file
        f.seek(img_size - 1)
        f.write(b'\0')
        f.seek(0)

        # 2. LBA 0: Sovereign Signature & MBR Partition Table
        # Signature
        f.write(struct.pack("<I", 0xEFBEADDE))
        # Fill until partition table offset (446)
        f.seek(446)
        # Entry 1: Bootable, Type 0x0C (FAT32 LBA), Start LBA 2048
        f.write(b'\x80\x00\x00\x00\x0C\x00\x00\x00')
        f.write(struct.pack("<I", 2048)) # Start LBA
        f.write(struct.pack("<I", part_sectors)) # Size
        # MBR Boot Signature
        f.seek(510)
        f.write(b'\x55\xAA')

        # 3. LBA 1: GPT Placeholder
        f.seek(1 * sector_size)
        f.write(b'EFI PART')

        # 4. LBA 2048: The BPB (The Heart of the FS)
        f.seek(part_offset * sector_size)
        f.write(b'\xEB\x58\x90') # Boot Jump
        f.seek(part_offset * sector_size + 3)
        f.write(b'OSX2.0  ')    # OEM Name
        f.seek(part_offset * sector_size + 11)
        f.write(struct.pack("<H", sector_size))        # Bytes/Sector
        f.write(struct.pack("<B", sectors_per_cluster)) # Sectors/Cluster
        f.write(struct.pack("<H", reserved_sectors))    # Reserved
        f.write(struct.pack("<B", num_fats))            # FATs
        f.seek(part_offset * sector_size + 32)
        f.write(struct.pack("<I", part_sectors))       # Total Sectors in Partition
        f.write(struct.pack("<I", sectors_per_fat))    # FAT Size
        f.write(struct.pack("<I", 2))                  # Root Cluster (2)
        f.seek(part_offset * sector_size + 510)
        f.write(b'\x55\xAA') # Boot Signature

        # 5. Initialize FAT Tables (Cluster 0, 1, and 2)
        # Cluster 0: Media Type, Cluster 1: EOC, Cluster 2: Root Dir EOC
        for i in range(num_fats):
            f.seek((part_offset + reserved_sectors + (i * sectors_per_fat)) * sector_size)
            f.write(struct.pack("<I", 0x0FFFFFF8)) # FAT[0]
            f.write(struct.pack("<I", 0xFFFFFFFF)) # FAT[1]
            f.write(struct.pack("<I", 0x0FFFFFFF)) # FAT[2] (End of Root Chain)

        # 6. Data Area (Cluster 2) starts at part_offset + reserved + (num_fats * sectors_per_fat)
        # 2048 + 32 + (2 * 128) = 2336.
        data_start = part_offset + reserved_sectors + (num_fats * sectors_per_fat)
        f.seek(data_start * sector_size)

        # Root Dir Entry 1: "BOOT    RSL"
        name = b'BOOT    RSL'
        f.write(name + b'\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00')

        # Root Dir Entry 2: "README  TXT"
        name = b'README  TXT'
        f.write(name + b'\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00')

    print(f"OSX2: 64MB Sovereign Disk Created at {img_path}.")
    print(f"Handshake: LBA 0=0xEFBEADDE | LBA 2048=0x55AA")

if __name__ == "__main__":
    main()
