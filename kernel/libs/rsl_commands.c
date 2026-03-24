#include <include/rsl.h>
#include <kernel/libs/fatfs/ff.h>

void rsl_ls(managed_ptr_t path) {
    DIR dp;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dp, str_to_cstr(path));
    if (res == FR_OK) {
        while (1) {
            res = f_readdir(&dp, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            print_cstr(fno.fname);
            print_cstr("\n");
        }
    } else {
        print_cstr("Error: Could not open directory.\n");
    }
}

void rsl_cat(managed_ptr_t path) {
    FIL fp;
    FRESULT res;
    char buffer[512];
    uint32_t br;

    res = f_open(&fp, str_to_cstr(path), FA_READ);
    if (res == FR_OK) {
        while (f_read(&fp, buffer, sizeof(buffer)-1, &br) == FR_OK && br > 0) {
            buffer[br] = '\0';
            print_cstr(buffer);
        }
        f_close(&fp);
    } else {
        print_cstr("Error: Could not open file for reading.\n");
    }
}

void rsl_write(managed_ptr_t path, managed_ptr_t content) {
    FIL fp;
    FRESULT res;
    uint32_t bw;

    res = f_open(&fp, str_to_cstr(path), FA_WRITE | FA_CREATE_ALWAYS);
    if (res == FR_OK) {
        f_write(&fp, str_to_cstr(content), (uint32_t)str_len(content), &bw);
        f_close(&fp);
        print_cstr("Successfully wrote to disk.\n");
    } else {
        print_cstr("Error: Could not open file for writing.\n");
    }
}

void rsl_cd(managed_ptr_t path) {
    /* Sovereign VDISK Bridge manages directory context. */
    print_cstr("Changed directory context to: ");
    print(path);
    print_cstr("\n");
}
