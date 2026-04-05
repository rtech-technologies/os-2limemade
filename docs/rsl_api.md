# OSx2 RSL API Specification

## 1. Memory Management
- `retain(ptr)`: Increments reference count.
- `release(ptr)`: Decrements reference count, potentially freeing the object.

## 2. String API
- `str_create(cstr)`: Creates an ARC-managed string from a C-string.
- `str_concat(s1, s2)`: Concatenates two managed strings.
- `str_match(s1, pattern)`: Performs a string comparison.

## 3. System Commands
Commands are dispatched via `rsl_execute_command(line)`:
- `ls [path]`: List directory.
- `cd <path>`: Change directory.
- `cat <file>`: View file.
- `write <file> [content]`: Write to file.
- `mkdir <name>`: Create directory.
- `copy <text>`: Copy to system clipboard.
- `paste`: Paste from system clipboard.
- `run <script>`: Execute RSL script.
- `settings`: Open system settings.
