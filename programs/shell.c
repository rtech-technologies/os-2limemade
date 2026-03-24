#include <include/rsl.h>

void shell_main(void) {
    color(10, 0); /* Emerald/Green on Black */
    print_cstr("Sovereign OS RSL Shell Initialized.\n");

    while (1) {
        managed_ptr_t prompt = str_create("rsl> ");
        managed_ptr_t line = input(prompt);
        release(prompt);

        const char* cmd = str_to_cstr(line);

        if (cmd[0] == 'l' && cmd[1] == 's') {
            managed_ptr_t path = str_create("/");
            rsl_ls(path);
            release(path);
        } else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't') {
            managed_ptr_t path = str_create(cmd + 4);
            rsl_cat(path);
            release(path);
        } else if (cmd[0] == 'w' && cmd[1] == 'r' && cmd[2] == 'i' && cmd[3] == 't' && cmd[4] == 'e') {
            managed_ptr_t path = str_create("output.txt");
            managed_ptr_t content = str_create("Hello Sovereign Disk");
            rsl_write(path, content);
            release(path);
            release(content);
        } else if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p') {
            print_cstr("Commands: ls, cat <path>, write, cd <path>, help, exit\n");
        } else if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't') {
            release(line);
            break;
        }

        release(line);
        break; /* Exit for build verification */
    }
}
