import sys
import struct

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]
    img_size = 64 * 1024 * 1024
    sector_size = 512

    with open(img_path, "wb") as f:
        f.seek(img_size - 1)
        f.write(b'\0')

    with open(img_path, "r+b") as f:
        # LBA 0: Sovereign Signature
        f.write(struct.pack("<I", 0xDEADBEEF))

        # LBA 1: FAT32 Boot Sector (Simplified)
        f.seek(1 * sector_size)
        f.write(b'\xEB\x58\x90') # Jump
        f.seek(1 * sector_size + 3)
        f.write(b'MSDOS5.0')
        f.seek(1 * sector_size + 11)
        f.write(struct.pack("<H", 512)) # Bytes per sector
        f.write(struct.pack("<B", 8))   # Sectors per cluster
        f.write(struct.pack("<H", 32))  # Reserved sectors
        f.write(struct.pack("<B", 2))   # Number of FATs
        f.seek(1 * sector_size + 32)
        f.write(struct.pack("<I", img_size // sector_size)) # Total sectors
        f.write(struct.pack("<I", 2048)) # Sectors per FAT
        f.write(struct.pack("<I", 2))    # Root cluster

        # LBA 33: FAT Table 1 (Stub: Mark Cluster 2 as EOC)
        f.seek(33 * sector_size)
        f.write(struct.pack("<I", 0x0FFFFFF8)) # FAT[0]
        f.write(struct.pack("<I", 0xFFFFFFFF)) # FAT[1]
        f.write(struct.pack("<I", 0x0FFFFFFF)) # FAT[2] (Root EOC)

        # LBA 4129: Data Area (Cluster 2 = Root Dir)
        # Offset = Reserved (32) + (NumFATs (2) * SectorsPerFAT (2048)) = 4128.
        # But our LBA 1 is the BPB, so add 1? No, BPB is LBA 0 relative to partition.
        # Actually, let's keep it simple: BPB is LBA 1.
        # Root starts at 1 + 32 + (2 * 2048) = 4129.
        f.seek(4129 * sector_size)

        # Root Dir Entry 1: "BIN" Directory
        # Name (11), Attr (1), Res (1), CrtTime (3), CrtDate (2), AccDate (2), HighClus (2), ModTime (2), ModDate (2), LowClus (2), Size (4)
        name = b'BIN        '
        f.write(name + b'\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00')

        # Root Dir Entry 2: "README  TXT"
        name = b'README  TXT'
        f.write(name + b'\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00')

        # Cluster 3: BIN directory content
        f.seek((4129 + 8) * sector_size) # Cluster 3 (8 sectors later)
        name = b'INSTALL RSL'
        f.write(name + b'\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x00')

        # Cluster 5: INSTALL.RSL content
        f.seek((4129 + 8*3) * sector_size) # Cluster 5
        f.write(b"print('Installing OSx2 Limemade...')")

    print(f"OSx2 Limemade FAT32 Disk Image {img_path} created with directories.")

if __name__ == "__main__":
    main()
