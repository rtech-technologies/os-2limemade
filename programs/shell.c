#include <include/rsl.h>

void shell_main(void) {
    color(10, 0); /* Emerald Green */
    print("OS boot sucsess\n");

    void* curdir = str_create("/");
    void* suffix = str_create("OS2>");

    while (1) {
        /* Pythonic: curdir + "OS2>" */
        void* prompt = str_concat(curdir, suffix);
        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (str_is_empty(cmd_line)) {
            release(cmd_line);
            continue;
        }

        if (str_match(cmd_line, "ls")) {
            rsl_ls();
        }
        else if (str_match(cmd_line, "write")) {
            void* path = input("Enter Filename: ");
            void* content = input("Enter Content: ");
            rsl_write(path, content);
            release(path);
            release(content);
        }
        else if (str_match(cmd_line, "exit")) {
            release(cmd_line);
            break;
        }
        else {
            print("Unknown Command.\n");
        }

        release(cmd_line);
        break; /* Build verification exit */
    }

    release(curdir);
    release(suffix);
}
