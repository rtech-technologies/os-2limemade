#include <include/rsl.h>

void rsl_execute_command(char* line);

void shell_main(void) {
    set_color(GREEN, BLACK);
    print("  _____ _______ ______ _____ _    _    ____   _____     ___  \n");
    print(" |  __ \\__   __|  ____/ ____| |  | |  / __ \\ / ____|   |__ \\ \n");
    print(" | |__) | | |  | |__ | |    | |__| | | |  | | (_____  __  ) |\n");
    print(" |  _  /  | |  |  __|| |    |  __  | | |  | |\\___ \\ \\/ / / / \n");
    print(" | | \\ \\  | |  | |___| |____| |  | | | |__| |____) >  < / /_ \n");
    print(" |_|  \\_\\ |_|  |______\\_____|_|  |_|  \\____/|_____/_/\\_____|\n");
    print("\n[ OSx2 Sovereign ] Build Success.\n");

    while (1) {
        /* In standalone mode, we use syscall for curdir if needed,
           for now simple prompt */
        void* prompt = str_create("OS2> ");
        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (cmd_line == NULL) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        char* line_buf = (char*)str_to_cstr(cmd_line);
        /* In standalone, we might need a syscall for execute_command */
        rsl_syscall(100, (uint64_t)line_buf, 0, 0);
        release(cmd_line);
    }
}
