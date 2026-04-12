#include <include/rsl.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);
void serial_write_char(char c);
void serial_write_str(const char* s);
char serial_read_char(void);
int serial_received(void);

static uint8_t current_color_val = 0x07; /* White on Black */

/* I/O Helpers */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void set_color(color_t fg, color_t bg) {
    current_color_val = ((uint8_t)bg << 4) | ((uint8_t)fg & 0x0F);
}

#define PRINT_QUEUE_SIZE 32
static const char* print_queue[PRINT_QUEUE_SIZE];
static task_t* print_callers[PRINT_QUEUE_SIZE];
static int print_head = 0;
static int print_tail = 0;
static bool print_worker_active = false;

static void print_worker_entry(void) {
    while (print_head != print_tail) {
        const char* s = print_queue[print_head];
        for (int i = 0; s[i] != '\0'; i++) {
            vga_write_char(s[i], current_color_val);
        }

        task_t* caller = print_callers[print_head];
        if (caller) caller->state = TASK_READY;

        print_head = (print_head + 1) % PRINT_QUEUE_SIZE;
        sys_yield();
    }

    print_worker_active = false;
    task_t* self = get_current_task();
    if (self) self->state = TASK_ZOMBIE;
    sys_yield();
}

void print(const char* s) {
    if (!s) return;

    /* Pure hardware output during scanning or if scheduler is not yet active */
    bool tasking_is_scanning(void);
    if (tasking_is_scanning()) {
        for (int i = 0; s[i] != '\0'; i++) {
            vga_write_char(s[i], current_color_val);
        }
        return;
    }

    /* Enqueue Print */
    int next_tail = (print_tail + 1) % PRINT_QUEUE_SIZE;
    if (next_tail == print_head) {
        vga_write_char('!', current_color_val); /* Overflow Signal */
        while (next_tail == print_head) {
            sys_yield(); /* Wait for queue space */
            next_tail = (print_tail + 1) % PRINT_QUEUE_SIZE;
        }
    }

    task_t* caller = get_current_task();
    print_queue[print_tail] = s;
    print_callers[print_tail] = caller;
    print_tail = next_tail;

    if (!print_worker_active) {
        print_worker_active = true;
        int register_transient_task(void (*entry)(void), uint32_t slab_id, uint64_t arg);
        /* Use Slab 3 for Print Workers (Isolation from Core System Task) */
        int tid = register_transient_task(print_worker_entry, 3, 0);

        if (tid != -1) {
            void scheduler_force_task(int task_id);
            scheduler_force_task(tid);
        } else {
            print_worker_active = false;
            /* Emergency Fallback: Direct Print */
            for (int i = 0; s[i] != '\0'; i++) {
                vga_write_char(s[i], current_color_val);
            }
            /* Don't block caller if worker failed to spawn */
            print_head = (print_head + 1) % PRINT_QUEUE_SIZE;
            return;
        }
    }

    /* Block caller and yield until worker wakes us */
    if (caller) {
        caller->state = TASK_WAITING;
        sys_yield();
    }
}

static void print_num(uint32_t n, int base) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        vga_write_char('0', current_color_val);
        return;
    }
    const char* digits = "0123456789ABCDEF";
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0) {
        vga_write_char(buf[--i], current_color_val);
    }
}

void vga_print(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++;
            if (fmt[i] == 'd') {
                int n = __builtin_va_arg(args, int);
                print_num(n, 10);
            } else if (fmt[i] == 'x') {
                uint32_t n = __builtin_va_arg(args, uint32_t);
                print_num(n, 16);
            } else if (fmt[i] == 's') {
                char* s = __builtin_va_arg(args, char*);
                print(s);
            }
        } else {
            vga_write_char(fmt[i], current_color_val);
        }
    }
    __builtin_va_end(args);
}

static void serial_print_num(uint32_t n, int base) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        serial_write_char('0');
        return;
    }
    const char* digits = "0123456789ABCDEF";
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0) {
        serial_write_char(buf[--i]);
    }
}

void serial_print(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++;
            if (fmt[i] == 'd') {
                int n = __builtin_va_arg(args, int);
                serial_print_num(n, 10);
            } else if (fmt[i] == 'x') {
                uint32_t n = __builtin_va_arg(args, uint32_t);
                serial_print_num(n, 16);
            } else if (fmt[i] == 's') {
                char* s = __builtin_va_arg(args, char*);
                serial_write_str(s);
            }
        } else {
            serial_write_char(fmt[i]);
        }
    }
    __builtin_va_end(args);
}

/* Scancode to ASCII (Simplified US-QWERTY) */
static char __attribute__((used)) scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
};

static char __attribute__((used)) shift_scancode_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
  '(', ')', '_', '+', '\b',	/* Backspace */
  '\t',			/* Tab */
  'Q', 'W', 'E', 'R',	/* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '"', '~',   0,		/* Left shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
  'M', '<', '>', '?',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
};

static bool __attribute__((used)) shift_pressed = false;

#define KBD_BUF_SIZE 64
static char kbd_buffer[KBD_BUF_SIZE];
static int kbd_head = 0;
static int kbd_tail = 0;

void kbd_push(char c) {
    if (!c || c == (char)-1) return;
    int next = (kbd_tail + 1) % KBD_BUF_SIZE;
    if (next != kbd_head) {
        kbd_buffer[kbd_tail] = c;
        kbd_tail = next;
    }
}

char kbd_pop(void) {
    if (kbd_head == kbd_tail) return 0;
    char c = kbd_buffer[kbd_head];
    kbd_head = (kbd_head + 1) % KBD_BUF_SIZE;
    return c;
}

void hw_poll(void) {
    #include <include/config.h>

    /* 1. PS/2 Keyboard Polling */
#if defined(CONFIG_INTERFACE_ALL) || defined(CONFIG_INTERFACE_PS2)
    uint8_t status = inb(0x64);
    if (status & 1) {
        uint8_t scancode = inb(0x60);
        if (!(status & 0x20)) { /* Not Mouse Data */
            if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
            else if (scancode == 0xAA || scancode == 0xB6) shift_pressed = false;
            else if (!(scancode & 0x80)) {
                if (scancode == 0x01) kbd_push(27);
                else if (scancode < 128) {
                    char c = shift_pressed ? shift_scancode_map[scancode] : scancode_map[scancode];
                    if (c) kbd_push(c);
                }
            }
        }
    }
#endif

    /* 2. Serial COM1 Polling */
    if (serial_received()) {
        char c = serial_read_char();
        if (c == '\n' || c == '\r' || c == '\b' || c == 27 || (c >= 32 && c <= 126)) {
            kbd_push(c);
        }
    }

    /* 4. PS/2 Mouse Polling */
#if defined(CONFIG_INTERFACE_ALL) || defined(CONFIG_INTERFACE_PS2)
    void mouse_poll(void);
    mouse_poll();
#endif
}

char get_char(void) {
    while (1) {
        hw_poll();
        char c = kbd_pop();
        if (c) return c;

        /* Sovereign Active-Relay: Yield while waiting for input */
        task_t* current = get_current_task();
        if (current) {
            current->state = TASK_INPUT_WAIT;
            scheduler_force_task(1);
        }
        sys_yield();
        if (current) current->state = TASK_RUNNING;
        __asm__ volatile ("pause");
    }
}

void* input(const char* prompt) {
    if (prompt) {
        print(prompt);
    }

    char buffer[128];
    int idx = 0;

    while (idx < 127) {
        char c = get_char();

        /* Wait for Enter key to be released before returning to prevent typing loop */
        if (c == (char)-1) continue;

        /* Break logic: ESC */
        if (c == 27) {
            vga_write_char('^', current_color_val);
            vga_write_char('C', current_color_val);
            vga_write_char('\n', current_color_val);
            return NULL;
        }

        if (c == '\n' || c == '\r') {
            vga_write_char('\n', current_color_val);
            break;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                vga_write_char('\b', current_color_val);
            }
        } else {
            buffer[idx++] = c;
            vga_write_char(c, current_color_val);
        }
    }

    buffer[idx] = '\0';
    return str_create(buffer);
}
