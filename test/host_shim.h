/* Minimal stand-ins for the Flipper GUI/input API so game.c can run on a PC. */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum { ColorWhite = 0, ColorBlack = 1 } Color;
typedef enum { FontPrimary, FontSecondary } Font;
typedef enum { AlignLeft, AlignRight, AlignTop, AlignBottom, AlignCenter } Align;

typedef enum {
    InputKeyUp,
    InputKeyDown,
    InputKeyRight,
    InputKeyLeft,
    InputKeyOk,
    InputKeyBack,
} InputKey;

typedef enum {
    InputTypePress,
    InputTypeRelease,
    InputTypeShort,
    InputTypeLong,
    InputTypeRepeat,
} InputType;

typedef struct {
    uint8_t px[64][128];
    Color color;
    Font font;
} Canvas;

void canvas_clear(Canvas* c);
void canvas_set_color(Canvas* c, Color color);
void canvas_set_font(Canvas* c, Font font);
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y);
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h);
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h);
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r);
void canvas_draw_circle(Canvas* c, int32_t x, int32_t y, size_t r);
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s);
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* s);
uint16_t canvas_string_width(Canvas* c, const char* s);
