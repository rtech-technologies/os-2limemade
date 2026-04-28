#ifndef RTC64_H
#define RTC64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_WINDOWS 8
#define MAX_MENU_ITEMS 4

typedef enum {
    RTC64_EVENT_NONE,
    RTC64_EVENT_MOUSE_DOWN,
    RTC64_EVENT_MOUSE_UP,
    RTC64_EVENT_MOUSE_MOVE,
    RTC64_EVENT_KEY_DOWN,
    RTC64_EVENT_BUTTON_CLICK
} rtc64_event_type_t;

typedef struct {
    rtc64_event_type_t type;
    int x, y;
    char key;
    int button_id;
} rtc64_event_t;

typedef struct {
    int id;
    const char* label;
    int x, y, w, h;
    void (*handler)(void);
} rtc64_button_t;

typedef struct {
    const char* label;
    void (*handler)(void);
} rtc64_menu_item_t;

typedef struct {
    const char* label;
    rtc64_menu_item_t items[MAX_MENU_ITEMS];
    int item_count;
} rtc64_menu_t;

typedef struct {
    int id;
    int x, y, w, h;
    const char* title;
    bool visible;
    bool focused;
    bool dragging;

    rtc64_menu_t menus[2];
    int menu_count;

    rtc64_button_t buttons[4];
    int button_count;

    void (*on_draw)(int x, int y, int w, int h);
    void (*on_event)(rtc64_event_t ev);
} rtc64_window_t;

/* RTC64 Core APIs */
void rtc64_init(void);
int rtc64_create_window(int x, int y, int w, int h, const char* title);
void rtc64_draw_all(void);
void rtc64_update(void);
void rtc64_wm_task(void);

/* Rendering Primitives */
void rtc64_draw_rect(int x, int y, int w, int h, uint32_t color);
void rtc64_draw_text(const char* s, int x, int y, uint32_t color);

/* ImGui Backend Primitives */
void rtc64_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void rtc64_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void rtc64_blit(int x, int y, int w, int h, uint32_t* data);

#endif
