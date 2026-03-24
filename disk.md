# OSx2 Limemade: Disk & Navigation Guide

This document explains how users can navigate the Sovereign storage system using the RSL Shell.

## 1. The Global Root (`/`)
The top-level of the file system is the **Global Root**. It acts as a virtual directory that contains all registered physical storage devices.

- When you run `ls` at `/`, you will see a list of disks and partitions.
- Format: `disk_id:/partition_id/` (e.g., `0:/0/`).

## 2. Disk Labeling
OSx2 Limemade uses numbers to label disks and partitions for simplicity and tiered access.

- **0:/0/**: Disk 0, Partition 0. This is typically the primary boot device or ramdisk.
- **1:/0/**: Disk 1, Partition 0. This could be a USB drive found during PCI scanning.

## 3. Navigating
To move into a disk, use the `cd` command with the absolute path:

```bash
rsl> cd 0:/0/
```

Once inside a disk, standard file operations like `ls`, `cat`, and `write` apply to that volume's filesystem (FatFS).

## 4. Verification
If a disk is missing its `0xDEADBEEF` signature, it will not appear in the Global Root list, as it is not recognized as a **Sovereign** volume.
