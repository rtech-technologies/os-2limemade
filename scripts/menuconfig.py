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

    print(f"Configuration saved to .config")

if __name__ == "__main__":
    main()
