#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "laps.h"

static Window window;

#define MAX_LAPS 9

TextLayer time_ring_layers[MAX_LAPS];
char time_ring_text[MAX_LAPS][13];
int time_ring_head = 0;
int time_ring_length = 0;

void init_lap_window() {
    window_init(&window, "Lap times");
    window_set_background_color(&window, GColorWhite);
}

void show_laps() {
    window_stack_push(&window, true);
}

void store_time(time_t lap_time) {

}