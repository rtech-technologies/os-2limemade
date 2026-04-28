#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void main(void) {
    print("Sovereign OS Installation Tool\n");
    print("------------------------------\n");

    void* drive_str = input("Enter drive ID to partition (e.g., 0): ");
    if (!drive_str) return;

    print("Partitioning drive...\n");
    rsl_format(drive_str);

    print("Stamping Sovereign signature...\n");
    rsl_stamp(drive_str);

    print("Installation components deployed.\n");
    release(drive_str);
}
