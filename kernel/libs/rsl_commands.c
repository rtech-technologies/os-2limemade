#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void rsl_ls(managed_ptr_t path) {
    print_cstr("bin/\nrsl.sh\n");
}

void rsl_cat(managed_ptr_t path) {
    print_cstr("File content stub.\n");
}

void rsl_cd(managed_ptr_t path) {
    print_cstr("Changed directory to: ");
    print(path);
    print_cstr("\n");
}
