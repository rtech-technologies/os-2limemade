#include <include/rsl.h>

void shell_main(void) {
    color(10, 0); /* Emerald/Green on Black */
    print_cstr("OS boot success\n");

    managed_ptr_t curdir = str_create("/");
    managed_ptr_t suffix = str_create("OS2> ");

    while (1) {
        managed_ptr_t prompt = str_concat(curdir, suffix);
        managed_ptr_t cmd_line = input(prompt);
        release(prompt);

        const char* cmd = str_to_cstr(cmd_line);

        if (cmd[0] == '\0') {
            release(cmd_line);
            continue;
        }

        /* Basic Command Dispatcher */
        if (cmd[0] == 'l' && cmd[1] == 's') {
            rsl_ls(curdir);
        } else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't') {
            managed_ptr_t path = str_create(cmd + 4);
            rsl_cat(path);
            release(path);
        } else if (cmd[0] == 'w' && cmd[1] == 'r' && cmd[2] == 'i' && cmd[3] == 't' && cmd[4] == 'e') {
            managed_ptr_t write_prompt = str_create("Enter filename: ");
            managed_ptr_t path = input(write_prompt);
            release(write_prompt);

            managed_ptr_t content_prompt = str_create("Enter content: ");
            managed_ptr_t content = input(content_prompt);
            release(content_prompt);

            rsl_write(path, content);
            release(path);
            release(content);
        } else if (cmd[0] == 'c' && cmd[1] == 'd') {
            release(curdir);
            curdir = str_create(cmd + 3);
            rsl_cd(curdir);
        } else if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p') {
            print_cstr("Available Commands:\n");
            print_cstr("ls          - List files\n");
            print_cstr("cat <file>  - Read file\n");
            print_cstr("write       - Create file interactively\n");
            print_cstr("cd <path>   - Change directory\n");
            print_cstr("help        - Show this menu\n");
            print_cstr("exit        - Shutdown Sovereign Core\n");
        } else if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't') {
            release(cmd_line);
            break;
        } else {
            print_cstr("Unknown command. Type 'help' for options.\n");
        }

        release(cmd_line);
        break; /* Exit for build verification purposes */
    }

    release(curdir);
    release(suffix);
}
