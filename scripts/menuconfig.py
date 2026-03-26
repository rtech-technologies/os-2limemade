import sys
import os

def main():
    print("RTECH OSx2 Configuration Tool")
    print("------------------------------")
    serial_port = input("Enter Serial Port (default 0x3F8): ") or "0x3F8"
    heap_size = input("Enter Heap Size in MB (default 16): ") or "16"

    print("\nAvailable Classic Screen Resolutions:")
    print("1. 320x200 (Mode 13h Classic)")
    print("2. 640x400 (Text Mode Classic)")
    print("3. 640x480 (Classic VGA)")
    print("4. 800x600 (SVGA)")
    print("5. 1024x768 (XGA)")
    res_choice = input("Select Resolution (1-5, default 3): ") or "3"

    res_map = {
        "1": "320x200",
        "2": "640x400",
        "3": "640x480",
        "4": "800x600",
        "5": "1024x768"
    }
    resolution = res_map.get(res_choice, "640x480")
    width, height = resolution.split('x')

    with open(".config", "w") as f:
        f.write(f"SERIAL_PORT={serial_port}\n")
        f.write(f"HEAP_SIZE={heap_size}\n")
        f.write(f"SCREEN_WIDTH={width}\n")
        f.write(f"SCREEN_HEIGHT={height}\n")

    with open("include/config.h", "w") as f:
        f.write("#ifndef CONFIG_H\n#define CONFIG_H\n\n")
        f.write(f"#define CONFIG_SERIAL_PORT {serial_port}\n")
        f.write(f"#define CONFIG_HEAP_SIZE (1024 * 1024 * {heap_size})\n")
        f.write(f"#define CONFIG_SCREEN_WIDTH {width}\n")
        f.write(f"#define CONFIG_SCREEN_HEIGHT {height}\n\n")
        f.write("#endif\n")

    # Update limine.cfg
    limine_cfg = f"""TIMEOUT=0
SERIAL=yes

:OSx2 Limemade
    PROTOCOL=limine
    INTERFACE_RESOLUTION={resolution}
    KERNEL_PATH=boot:///boot/kernel.elf
    MODULE_PATH=boot:///boot/ramdisk.img
"""
    with open("boot/limine.cfg", "w") as f:
        f.write(limine_cfg)

    print(f"Configuration saved to .config and include/config.h")
    print(f"Limine resolution set to {resolution}")

if __name__ == "__main__":
    main()
