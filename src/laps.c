#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "laps.h"
#include "common.h"

static Window window;
static ScrollLayer scroll_view;
static TextLayer no_laps_note;

#define MAX_LAPS 20
#define LAP_STRING_LENGTH 15

static TextLayer lap_layers[MAX_LAPS];
static char lap_text[MAX_LAPS][LAP_STRING_LENGTH];
static int time_ring_head = 0;
static int time_ring_length = 0;
static int times_displayed = 0;
static int total_laps = 0;

void init_lap_window() {
    window_init(&window, "Lap times");
    window_set_background_color(&window, GColorWhite);

    scroll_layer_init(&scroll_view, GRect(0, 0, 144, 152));
    scroll_layer_set_click_config_onto_window(&scroll_view, &window);

    GFont laps_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);

    for(int i = 0; i < MAX_LAPS; ++i) {
        memcpy(lap_text[i], " 1) 12:34:56.7", LAP_STRING_LENGTH);

        text_layer_init(&lap_layers[i], GRect(0, i * 25, 144, 25));
        text_layer_set_background_color(&lap_layers[i], GColorClear);
        text_layer_set_font(&lap_layers[i], laps_font);
        text_layer_set_text_color(&lap_layers[i], GColorBlack);
        text_layer_set_text(&lap_layers[i], lap_text[i]);
        layer_set_hidden(&lap_layers[i].layer, true);
        scroll_layer_add_child(&scroll_view, &lap_layers[i].layer);
    }

    layer_add_child(window_get_root_layer(&window), &scroll_view.layer);

    // Add a prompt for more laps.
    text_layer_init(&no_laps_note, GRect(0, 61, 144, 30));
    text_layer_set_background_color(&no_laps_note, GColorClear);
    text_layer_set_font(&no_laps_note, laps_font);
    text_layer_set_text_color(&no_laps_note, GColorBlack);
    text_layer_set_text_alignment(&no_laps_note, GTextAlignmentCenter);
    text_layer_set_text(&no_laps_note, "No laps yet.");
    layer_add_child(window_get_root_layer(&window), &no_laps_note.layer);
}

void show_laps() {
    window_stack_push(&window, true);
}

void clear_laps() {
    time_ring_head = 0;
    time_ring_length = 0;
}

void store_lap_time(time_t lap_time) {
    if(times_displayed < MAX_LAPS) {
        if(times_displayed == 0) {
            layer_set_hidden(&no_laps_note.layer, true);
        }
        layer_set_hidden(&lap_layers[times_displayed].layer, false);
        ++times_displayed;
        scroll_layer_set_content_size(&scroll_view, GSize(144, times_displayed * 25));
    }
    for(int i = MAX_LAPS - 1; i > 0; --i) {
        memcpy(lap_text[i], lap_text[i-1], LAP_STRING_LENGTH);
        layer_mark_dirty(&lap_layers[i].layer);
    }
    itoa2(++total_laps, &lap_text[0][0]);
    if(lap_text[0][0] == '0') lap_text[0][0] = ' ';
    format_lap(lap_time, &lap_text[0][4]);
    text_layer_set_text(&lap_layers[0], lap_text[0]);
}

void clear_stored_laps() {
    times_displayed = 0;
    scroll_layer_set_content_size(&scroll_view, GSize(144, 0));
    for(int i = 0; i < MAX_LAPS; ++i) {
        layer_set_hidden(&lap_layers[i].layer, true);
    }
    layer_set_hidden(&no_laps_note.layer, false);
    total_laps = 0;
}
