import sys
import struct

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]

    # Create a 64MB blank image
    img_size = 64 * 1024 * 1024
    with open(img_path, "wb") as f:
        f.write(b'\x00' * img_size)

    # Inject 0xDEADBEEF at LBA 0
    with open(img_path, "r+b") as f:
        signature = 0xDEADBEEF
        f.write(struct.pack("<I", signature))
        print(f"Injected 0xDEADBEEF signature at LBA 0 of {img_path}")

        # Basic FAT32 BPB (Bios Parameter Block) - Mock
        f.seek(1 * 512) # Seek to LBA 1
        f.write(b'\xEB\x58\x90') # Jump instruction
        f.seek(1 * 512 + 3)
        f.write(b'MSDOS5.0') # OEM Name
        f.seek(1 * 512 + 11)
        f.write(struct.pack("<H", 512)) # Bytes per sector
        f.write(struct.pack("<B", 8)) # Sectors per cluster
        f.write(struct.pack("<H", 32)) # Reserved sectors
        print(f"Mock FAT32 BPB injected at LBA 1 of {img_path}")

    # In a real scenario, this would use 'mkfs.fat' or manual BPB creation
    print(f"Sovereign FAT32 Disk Image {img_path} created (Stub).")

if __name__ == "__main__":
    main()
