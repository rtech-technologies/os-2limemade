# OSx2 Limemade Shell Manual

The RSL Shell is the primary user interface for the OSx2 operating system. It allows for direct hardware management, file manipulation, and script execution.

## Global Root (`/`)
At the global root, you can see all physical hardware detected by the kernel.
- `[SOVEREIGN]`: Disks that contain a valid OSx2 partition and are mountable.
- `[RAW DISK]`: Disks that are either empty or formatted with an unknown filesystem.

## Contextual Paths
The shell maintains a Current Working Directory (`curdir`).
- Use `cd <path>` to navigate.
- Use `cd ..` to go up one level.
- Use `cd /` to return to the global root.
- Most commands (ls, cat, write, mkdir) will use your current directory if no drive prefix (e.g., `BOOT:`) is provided.

## Command Reference

### Hardware & Maintenance
- `ls`: List disks at root or files in the current directory.
- `mount <drive_id>`: Attempt to mount a physical drive (e.g., `mount 0`). If successful, it maps to `SATAx:`.
- `format <drive_id>`: Destructively format a drive to FAT32.
- `stamp <drive_id>`: Perform a mechanical sync on the target drive.

### File Operations
- `cat <file>`: Print the contents of a file.
- `write <file>`: Create a new file or overwrite an existing one. Pre-existing files must be on a Sovereign volume.
- `mkdir <name>`: Create a new directory in the current context.
- `rmdir <name>`: Delete a directory or file.

### System
- `echo <text>`: Print text back to the screen.
- `run <script>`: Execute an RSL script from the disk.
- `color <fg> <bg>`: Update shell colors (e.g., `color light_cyan black`).
- `help`: Display this information.
- `exit`: Shut down the system and perform cleanup.

## Safe Mode
If the kernel cannot find or mount a Sovereign boot disk, it enters **Safe Mode**. In this mode, destructive commands like `write`, `mkdir`, and `rmdir` are disabled to prevent data corruption.
