import sys
import struct
import uuid

def main():
    if len(sys.argv) < 2:
        print("Usage: fat_tool.py <output_img>")
        sys.exit(1)

    img_path = sys.argv[1]
    img_size = 64 * 1024 * 1024  # 64MB
    sector_size = 512
    part_offset = 2048           # The Sovereign Offset
    total_sectors = img_size // sector_size
    part_sectors = total_sectors - part_offset - 33 # Leave room for backup GPT

    with open(img_path, "wb") as f:
        # 1. Create the sparse file
        f.seek(img_size - 1)
        f.write(b'\0')
        f.seek(0)

        # 2. LBA 0: Protective MBR
        f.write(struct.pack("<I", 0xEFBEADDE)) # Sovereign Signature
        f.seek(446)
        # Entry 1: GPT Protective Partition (Type 0xEE)
        f.write(b'\x00\x00\x02\x00\xEE\xFF\xFF\xFF')
        f.write(struct.pack("<I", 1)) # Start LBA 1
        f.write(struct.pack("<I", total_sectors - 1))
        f.seek(510)
        f.write(b'\x55\xAA')

        # 3. Partition Entry Array (LBA 2-33)
        entries = bytearray(128 * 128)
        # Entry 1: Sovereign Data
        # Partition Type GUID (EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 - Basic Data)
        entries[0:16] = uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le
        entries[16:32] = uuid.uuid4().bytes_le # Unique GUID
        entries[32:40] = struct.pack("<Q", part_offset) # Start LBA 2048
        entries[40:48] = struct.pack("<Q", part_offset + part_sectors - 1) # End LBA
        entries[56:128] = "Sovereign".encode('utf-16le')

        f.seek(2 * sector_size)
        f.write(entries)

        # 4. LBA 1: GPT Header
        f.seek(1 * sector_size)
        header = bytearray(92)
        header[0:8] = b'EFI PART'
        header[8:12] = b'\x00\x00\x01\x00' # Revision
        header[12:16] = struct.pack("<I", 92) # Header size
        header[24:32] = struct.pack("<Q", 1) # MyLBA
        header[32:40] = struct.pack("<Q", total_sectors - 1) # AlternateLBA
        header[40:48] = struct.pack("<Q", 34) # FirstUsableLBA
        header[48:56] = struct.pack("<Q", total_sectors - 34) # LastUsableLBA
        header[56:72] = uuid.uuid4().bytes # Disk GUID
        header[72:80] = struct.pack("<Q", 2) # PartitionEntryLBA
        header[80:84] = struct.pack("<I", 128) # NumberOfPartitionEntries
        header[84:88] = struct.pack("<I", 128) # SizeOfPartitionEntry

        # Calculate Partition Entry Array Checksum
        import zlib
        header[88:92] = struct.pack("<I", zlib.crc32(entries) & 0xFFFFFFFF)

        # Calculate Header Checksum
        header[16:20] = b'\x00\x00\x00\x00'
        header[16:20] = struct.pack("<I", zlib.crc32(header) & 0xFFFFFFFF)
        f.write(header)

        # 4. LBA 2048: The BPB
        f.seek(part_offset * sector_size)
        f.write(b'\xEB\x58\x90') # Boot Jump
        f.seek(part_offset * sector_size + 3)
        f.write(b'OSX2.0  ')    # OEM Name
        f.seek(part_offset * sector_size + 11)
        f.write(struct.pack("<H", sector_size))        # Bytes/Sector
        f.write(struct.pack("<B", 8)) # Sectors/Cluster
        f.write(struct.pack("<H", 32))    # Reserved
        f.write(struct.pack("<B", 2))            # FATs
        f.seek(part_offset * sector_size + 32)
        f.write(struct.pack("<I", part_sectors))       # Total Sectors
        f.write(struct.pack("<I", 128))    # FAT Size
        f.write(struct.pack("<I", 2))                  # Root Cluster
        f.seek(part_offset * sector_size + 510)
        f.write(b'\x55\xAA') # Boot Signature

        # 5. Initialize FAT Tables
        for i in range(2):
            f.seek((part_offset + 32 + (i * 128)) * sector_size)
            f.write(struct.pack("<I", 0x0FFFFFF8))
            f.write(struct.pack("<I", 0xFFFFFFFF))
            f.write(struct.pack("<I", 0x0FFFFFFF))

    print(f"OSX2: 64MB GPT Sovereign Disk Created at {img_path}.")

if __name__ == "__main__":
    main()
