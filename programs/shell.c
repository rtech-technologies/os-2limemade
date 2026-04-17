#include <include/rsl.h>

void shell_main(void) {
    set_color(GREEN, BLACK);
    print("  _____ _______ ______ _____ _    _    ____   _____     ___  \n");
    print(" |  __ \\__   __|  ____/ ____| |  | |  / __ \\ / ____|   |__ \\ \n");
    print(" | |__) | | |  | |__ | |    | |__| | | |  | | (_____  __  ) |\n");
    print(" |  _  /  | |  |  __|| |    |  __  | | |  | |\\___ \\ \\/ / / / \n");
    print(" | | \\ \\  | |  | |___| |____| |  | | | |__| |____) >  < / /_ \n");
    print(" |_|  \\_\\ |_|  |______\\_____|_|  |_|  \\____/|_____/_/\\_\\____|\n");
    print("\n[ OSx2 Sovereign ] Build Success.\n");

    /* Automated Sovereignty: Try to execute BOOT.RSL */
    int rsl_execute_stream(const char* path);
    if (rsl_execute_stream("BOOT:/BOOT.RSL") != 0) {
        set_color(LIGHT_RED, BLACK);
        print("Warning: BOOT.RSL failed to load.\n");
        set_color(GREEN, BLACK);
    }

    /* Boot Menu Choice-Gate */
    print("\n1. Install OSx2 to SATA HDD\n");
    print("2. Enter Safe Mode (CD-ROM Only)\n");
    print("3. Launch Window Manager (RTC64)\n");

    while(1) {
        void* choice = input("\nSelect Option (1/2/3): ");
        if (choice) {
            if (str_match(choice, "1")) {
                print("Preparing Installation...\n");
                int rsl_execute_stream(const char* path);
                rsl_execute_stream("BOOT:/install.rsl");
                release(choice);
                break;
            } else if (str_match(choice, "2")) {
                print("Entering Safe Mode...\n");
                release(choice);
                break;
            } else if (str_match(choice, "3")) {
                print("Launching RTC64 Window Manager...\n");
                int tasking_spawn_app(const char* path);
                tasking_spawn_app("BOOT:/wm.bin");
                release(choice);
                break;
            } else {
                print("Invalid choice. Try again.\n");
                release(choice);
            }
        }
    }

    while (1) {
        void rsl_execute_command(char* line);
        void* rsl_get_curdir(void);
        void* cd = rsl_get_curdir();
        void* suffix = str_create("OS2>");
        void* prompt = str_concat(cd, suffix);
        release(cd); release(suffix);

        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (cmd_line == NULL) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        char* line_buf = (char*)str_to_cstr(cmd_line);
        rsl_execute_command(line_buf);
        release(cmd_line);
    }
}
