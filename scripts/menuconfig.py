import sys
import os

def main():
    print("RTECH OSx2 Configuration Tool")
    print("------------------------------")
    serial_port = input("Enter Serial Port (default 0x3F8): ") or "0x3F8"
    heap_size = input("Enter Heap Size in MB (default 16): ") or "16"

    with open(".config", "w") as f:
        f.write(f"SERIAL_PORT={serial_port}\n")
        f.write(f"HEAP_SIZE={heap_size}\n")

    with open("include/config.h", "w") as f:
        f.write("#ifndef CONFIG_H\n#define CONFIG_H\n\n")
        f.write(f"#define CONFIG_SERIAL_PORT {serial_port}\n")
        f.write(f"#define CONFIG_HEAP_SIZE (1024 * 1024 * {heap_size})\n\n")
        f.write("#endif\n")

    print(f"Configuration saved to .config and include/config.h")

if __name__ == "__main__":
    main()
