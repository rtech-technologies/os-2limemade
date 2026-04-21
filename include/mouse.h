#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int x;
    int y;
    bool left_button;
    bool right_button;
    bool middle_button;
    bool active;
} mouse_state_t;

void mouse_init(void);
void mouse_poll(void);
mouse_state_t* get_mouse_state(void);

#endif
