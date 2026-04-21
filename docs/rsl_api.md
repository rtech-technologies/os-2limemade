# OSx2 RSL API Specification (Exhaustive List)

## 1. Memory Management (ARC System)
The Sovereign kernel utilizes Automatic Reference Counting (ARC) for heap object management.
- `retain(ptr)`: Increments the reference count of an ARC-managed object.
- `release(ptr)`: Decrements the reference count. If the count reaches zero, the object is recycled into its origin slab.

## 2. String API (Pythonic/ARC-Managed)
- `str_create(cstr)`: Creates an ARC-managed string object from a standard C-string.
- `str_concat(s1, s2)`: Returns a new managed string containing the concatenation of `s1` and `s2`.
- `str_len(str)`: Returns the number of characters in the managed string.
- `str_is_empty(str)`: Returns `true` if the string length is 0.
- `str_match(str, pattern)`: Compares the managed string against a C-string pattern.
- `str_to_cstr(str)`: Accesses the internal C-string buffer of a managed string.

## 3. Console & I/O API
- `print(s)`: Outputs a C-string to the GOP console and Serial COM1 via a Sovereign worker task.
- `input(prompt)`: Displays a prompt and waits for user input. Returns an ARC-managed string.
- `set_color(fg, bg)`: Updates the terminal color attributes for subsequent text output.

## 4. File System API (RSL Wrappers)
These functions utilize ARC-managed paths and provide high-level VFS access.
- `rsl_ls(path)`: Lists the contents of the specified directory.
- `rsl_cat(path)`: Prints the contents of a file to the console.
- `rsl_write(path, content)`: Overwrites or creates a file with the provided managed content.
- `rsl_cd(path)`: Changes the active task's current directory context.
- `rsl_mkdir(path)`: Creates a new directory on the target volume.
- `rsl_rmdir(path)`: Deletes a directory or file.
- `rsl_exists(path)`: Returns `true` if the path exists on a mounted volume.
- `rsl_mount(path)`: Mounts a physical drive (e.g., "0:/") to the VFS.
- `rsl_format(path)`: Initializes a new FAT32 Sovereign filesystem on the target drive.
- `rsl_stamp(path)`: Performs a mechanical sync (signature write) on the target drive.
- `rsl_eject(path)`: Sends an eject signal to ATAPI/SATA optical media.
- `rsl_safe_mode()`: Returns the system's current Safe Mode status.

## 5. System & Graphics API
- `rsl_draw_rrif(path, x, y)`: Renders an RRIF format image at the specified screen coordinates.
- `rsl_scan()`: Performs an AHCI hardware audit to discover newly connected drives.
- `rsl_copy(str)`: Copies a managed string to the global system clipboard.
- `rsl_paste()`: Returns an ARC-managed copy of the system clipboard contents.
- `rsl_settings()`: Invokes the menu-driven system configuration interface.
- `rsl_debug_dump()`: Outputs kernel diagnostic data (CR3, Slab usage) to the console.

## 6. Shell Command Reference
The following commands are available directly in the RSL Shell:
- `ls [path]`: List directory.
- `cd <path>`: Change directory.
- `cat <file>`: View file.
- `write <file> [content]`: Write content to file.
- `mkdir <name>`: Create directory.
- `rmdir <name>`: Remove directory.
- `echo <text>`: Print text.
- `color <fg> <bg>`: Change terminal colors.
- `copy <text>`: Copy to clipboard.
- `paste`: Paste from clipboard.
- `mount <drive_id>`: Mount a physical disk.
- `format <drive_id>`: Format a disk.
- `stamp <drive_id>`: Mechanical sync (signature write).
- `run <script_path>`: Execute an RSL script.
- `DRAWtest`: Visual verification of the GOP driver (Emerald Square).
- `scan`: Re-scan AHCI ports.
- `debug-dump`: Kernel diagnostics.
- `settings`: System settings menu.
- `help`: Display command help.
- `exit`: Shutdown the system.
