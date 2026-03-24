#include <include/rsl.h>

void shell_main(void) {
    color(10, 0); /* Emerald Green */
    print("OS boot sucsess\n");

    void* curdir = str_create("/");

    while (1) {
        /* Pythonic: input(curdir + "OS2>") */
        void* suffix = str_create("OS2>");
        void* prompt = str_concat(curdir, suffix);
        release(suffix);

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
            void* filename = input("Enter Filename: ");
            void* content = input("Enter Content: ");
            rsl_write(filename, content);
            release(filename);
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
    }

    release(curdir);
}
