# RTECH Standard Library (RSL) Manual

RSL is the native, Pythonic system language for OSx2. It provides high-level abstractions for memory, file I/O, and hardware interaction.

## Core Concepts

### Managed Pointers (ARC)
All RSL functions return "Managed Pointers" (`void*`). These objects are automatically reference-counted.
- `retain(ptr)`: Increment the reference count.
- `release(ptr)`: Decrement the reference count. If it hits 0, the memory is reclaimed by the kernel.

### Sovereign Paths
Paths in RSL are either absolute or context-relative.
- Absolute: `BOOT:/bin/init`, `SATA0:/data/config.txt`
- Context-Relative: `init`, `../scripts/setup.rsl` (Resolved against the current working directory).

## RSL API Reference

### Console & Input
- `print(const char* s)`: Output text to VGA and Serial.
- `input(const char* prompt)`: Display a prompt and return a managed string of user input.
- `set_color(color_t fg, color_t bg)`: Update console colors.

### Filesystem (VFS)
- `rsl_ls(void* path)`: List contents of a directory.
- `rsl_cat(void* path)`: Output the contents of a file to the console.
- `rsl_write(void* path, void* content)`: Create or overwrite a file.
- `rsl_mkdir(void* path)`: Create a new directory.
- `rsl_rmdir(void* path)`: Remove a directory or file.
- `rsl_cd(void* path)`: Change the shell's active directory.
- `rsl_mount(void* path)`: Mount a hardware drive to the VFS.
- `rsl_format(void* path)`: Physically format a drive with a Sovereign FAT32 table.

### Executive
- `rsl_execute_stream(const char* path)`: Execute an RSL script streamer from the disk.
- `rsl_safe_mode()`: Check if the system is in restricted Safe Mode.
