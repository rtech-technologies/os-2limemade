#ifndef CMDLETS_H
#define CMDLETS_H

#include <include/rsl.h>

/* CMD-LETS Engine Interface */
void cmdlets_init(void);
int cmdlets_execute_line(const char* line);
int cmdlets_execute_script(const char* path);

/* Control Flow Stack for JIT/Interpreter */
typedef enum {
    FLOW_NONE,
    FLOW_IF,
    FLOW_FOR
} flow_type_t;

#endif /* CMDLETS_H */
