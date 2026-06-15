#include <include/cmdlets.h>
#include <include/rsl.h>
#include <include/vfs.h>
#include <stddef.h>
void vga_print(const char* fmt, ...);

void rsl_execute_command(char* line);

typedef struct {
    char name[32];
    int value;
} cmd_var_t;

static cmd_var_t vars[64];
static int var_count = 0;
static int skip_block = 0;

void cmdlets_init(void) {
    vga_print("[INIT] CMD-LETS Scripting Engine active.\n");
    var_count = 0;
    skip_block = 0;
}

static int get_var(const char* name) {
    if (name[0] == '$') name++;
    for (int i = 0; i < var_count; i++) {
        int k = 0;
        while (name[k] && vars[i].name[k] && name[k] == vars[i].name[k]) k++;
        if (name[k] == '\0' && vars[i].name[k] == '\0') return vars[i].value;
    }
    return 0;
}

static void set_var(const char* name, int val) {
    if (name[0] == '$') name++;
    for (int i = 0; i < var_count; i++) {
        int k = 0;
        while (name[k] && vars[i].name[k] && name[k] == vars[i].name[k]) k++;
        if (name[k] == '\0' && vars[i].name[k] == '\0') { vars[i].value = val; return; }
    }
    if (var_count < 64) {
        int k = 0; while(name[k] && k < 31) { vars[var_count].name[k] = name[k]; k++; }
        vars[var_count].name[k] = '\0';
        vars[var_count].value = val;
        var_count++;
    }
}

static void expand_vars(char* dest, const char* src) {
    int di = 0;
    for (int i = 0; src[i]; i++) {
        if (src[i] == '$' && src[i+1] != ' ' && src[i+1] != '\0') {
            char var_name[32];
            int vi_idx = 0;
            i++;
            while (src[i] && src[i] != ' ' && src[i] != '\n' && src[i] != '/' && src[i] != ':' && vi_idx < 31) {
                var_name[vi_idx++] = src[i++];
            }
            var_name[vi_idx] = '\0';
            int val = get_var(var_name);
            char tmp[16];
            int ti = 0;
            int v = (val < 0) ? -val : val;
            if (val < 0) dest[di++] = '-';
            if (v == 0) tmp[ti++] = '0';
            while (v > 0) { tmp[ti++] = (v % 10) + '0'; v /= 10; }
            while (ti > 0) dest[di++] = tmp[--ti];
            i--;
        } else {
            dest[di++] = src[i];
        }
    }
    dest[di] = '\0';
}

int cmdlets_execute_line(const char* line) {
    if (!line || line[0] == '\0' || line[0] == '#') return 0;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return 0;

    char buf[256];
    int k = 0; while(line[k] && k < 255) { buf[k] = line[k]; k++; } buf[k] = '\0';

    /* if var == val */
    if (buf[0] == 'i' && buf[1] == 'f' && buf[2] == ' ') {
        char var_name[32];
        int vi_idx = 0;
        const char* p = &buf[3];
        while (*p && *p != ' ' && vi_idx < 31) var_name[vi_idx++] = *p++;
        var_name[vi_idx] = '\0';
        if (get_var(var_name) == 0) skip_block++;
        return 1;
    } else if (buf[0] == 'f' && buf[1] == 'i' && (buf[2] == '\0' || buf[2] == ' ')) {
        if (skip_block > 0) skip_block--;
        return -1;
    }

    /* for var in val1 val2 ... */
    if (buf[0] == 'f' && buf[1] == 'o' && buf[2] == 'r' && buf[3] == ' ') {
        /* simplified: just skip for now, real implementation needs stateful streamer */
        return 2;
    } else if (buf[0] == 'd' && buf[1] == 'o' && buf[2] == 'n' && buf[3] == 'e') {
        return -2;
    }

    if (skip_block > 0) return 0;

    /* Assignment */
    char* eq = NULL;
    for (int i=0; buf[i]; i++) if (buf[i] == '=') { eq = &buf[i]; break; }
    if (eq) {
        *eq = '\0';
        int val = 0;
        const char* v_s = eq + 1;
        bool neg = false;
        if (*v_s == '-') { neg = true; v_s++; }
        while (*v_s >= '0' && *v_s <= '9') { val = val * 10 + (*v_s - '0'); v_s++; }
        if (neg) val = -val;
        set_var(buf, val);
        return 0;
    }

    char expanded[256];
    expand_vars(expanded, buf);
    rsl_execute_command(expanded);
    return 0;
}

int cmdlets_execute_script(const char* path) {
    void* pstr = str_create(path);
    vfs_handle_t* h = vfs_open(pstr, "r");
    if (!h) { release(pstr); return -1; }

    char line[256];
    int idx = 0;
    char c;
    while (vfs_read(h, &c, 1) == 1) {
        if (c == '\n' || idx >= 255) {
            line[idx] = '\0';
            cmdlets_execute_line(line);
            idx = 0;
        } else {
            line[idx++] = c;
        }
    }
    if (idx > 0) { line[idx] = '\0'; cmdlets_execute_line(line); }
    vfs_close(h);
    release(pstr);
    return 0;
}
