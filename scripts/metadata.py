import subprocess
import sys

def main():
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"]).decode().strip()
    flags = sys.argv[1]

    with open("kernel/libs/core/metadata.c", "w") as f:
        f.write('#include <stdint.h>\n')
        f.write('const char* g_build_commit = "{}";\n'.format(commit))
        f.write('const char* g_build_flags = "{}";\n'.format(flags))

if __name__ == "__main__":
    main()
