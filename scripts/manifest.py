import hashlib
import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: manifest.py <kernel.elf>")
        return

    with open(sys.argv[1], "rb") as f:
        data = f.read()
        sha256 = hashlib.sha256(data).hexdigest()

    with open("kernel/libs/core/manifest.c", "w") as f:
        f.write('#include <stdint.h>\n')
        f.write('const char* g_build_manifest_hash = "{}";\n'.format(sha256))

if __name__ == "__main__":
    main()
