/*
 * Pebble Stopwatch - the big, ugly file.
 * Copyright (C) 2013 Katharine Berry
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <pebble.h>

#include "laps.h"
#include "common.h"

static Window* window;

// Main display
static TextLayer* big_time_layer;
static TextLayer* seconds_time_layer;
static Layer* line_layer;
static GBitmap* button_bitmap;
static BitmapLayer* button_labels;


// Lap time display
#define LAP_TIME_SIZE 5
static char lap_times[LAP_TIME_SIZE][11] = {"00:00:00.0", "00:01:00.0", "00:02:00.0", "00:03:00.0", "00:04:00.0"};
static TextLayer* lap_layers[LAP_TIME_SIZE]; // an extra temporary layer
static int next_lap_layer = 0;
static double last_lap_time = 0;

// Actually keeping track of time
static double elapsed_time = 0;
static bool started = false;
static AppTimer* update_timer = NULL;
static double start_time = 0;
static double pause_time = 0;

// Global animation lock. As long as we only try doing things while
// this is zero, we shouldn't crash the watch.
static int busy_animating = 0;

// Fonts
static GFont big_font;
static GFont seconds_font;
static GFont laps_font;

#define TIMER_UPDATE 1
#define FONT_BIG_TIME RESOURCE_ID_FONT_DEJAVU_SANS_BOLD_SUBSET_30
#define FONT_SECONDS RESOURCE_ID_FONT_DEJAVU_SANS_SUBSET_18
#define FONT_LAPS RESOURCE_ID_FONT_DEJAVU_SANS_SUBSET_22

#define BUTTON_LAP BUTTON_ID_DOWN
#define BUTTON_RUN BUTTON_ID_SELECT
#define BUTTON_RESET BUTTON_ID_UP

#define PERSIST_STATE 1
#define PERSIST_NUM_LAPS 4
#define PERSIST_STORED_LAPS 5
	
struct StopwatchState {
	bool started;
	double elapsed_time;
	double start_time;
	double pause_time;
	double last_lap_time;
} __attribute__((__packed__));

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void config_provider(Window *window);
void handle_init();
time_t time_seconds();
void stop_stopwatch();
void start_stopwatch();
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void update_stopwatch();
void handle_timer(void* data);
int main();
void draw_line(Layer *me, GContext* ctx);
void save_lap_time(double seconds);
void lap_time_handler(ClickRecognizerRef recognizer, Window *window);
void shift_lap_layer(PropertyAnimation** animation, Layer* layer, GRect* target, int distance_multiplier);

void handle_init() {
	window = window_create();
    window_stack_push(window, true /* Animated */);
    window_set_background_color(window, GColorBlack);
    window_set_fullscreen(window, false);

    // Arrange for user input.
    window_set_click_config_provider(window, (ClickConfigProvider) config_provider);

    // Get our fonts
    big_font = fonts_load_custom_font(resource_get_handle(FONT_BIG_TIME));
    seconds_font = fonts_load_custom_font(resource_get_handle(FONT_SECONDS));
    laps_font = fonts_load_custom_font(resource_get_handle(FONT_LAPS));

    // Root layer
    Layer *root_layer = window_get_root_layer(window);

    // Set up the big timer.
	big_time_layer = text_layer_create(GRect(0, 5, 96, 35));
    text_layer_set_background_color(big_time_layer, GColorBlack);
    text_layer_set_font(big_time_layer, big_font);
    text_layer_set_text_color(big_time_layer, GColorWhite);
    text_layer_set_text(big_time_layer, "00:00");
    text_layer_set_text_alignment(big_time_layer, GTextAlignmentRight);
    layer_add_child(root_layer, (Layer*)big_time_layer);

    seconds_time_layer = text_layer_create(GRect(96, 17, 49, 35));
    text_layer_set_background_color(seconds_time_layer, GColorBlack);
    text_layer_set_font(seconds_time_layer, seconds_font);
    text_layer_set_text_color(seconds_time_layer, GColorWhite);
    text_layer_set_text(seconds_time_layer, ".0");
    layer_add_child(root_layer, (Layer*)seconds_time_layer);

    // Draw our nice line.
    line_layer = layer_create(GRect(0, 45, 144, 2));
	layer_set_update_proc(line_layer, draw_line);
    layer_add_child(root_layer, line_layer);

    // Set up the lap time layers. These will be made visible later.
    for(int i = 0; i < LAP_TIME_SIZE; ++i) {
		lap_layers[i] = text_layer_create(GRect(-139, 52, 139, 30));
        text_layer_set_background_color(lap_layers[i], GColorClear);
        text_layer_set_font(lap_layers[i], laps_font);
        text_layer_set_text_color(lap_layers[i], GColorWhite);
        text_layer_set_text(lap_layers[i], lap_times[i]);
        layer_add_child(root_layer, (Layer*)lap_layers[i]);
    }

    // Add some button labels
	
	button_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BUTTON_LABELS);
	button_labels = bitmap_layer_create(GRect(130, 10, 14, 136));
	bitmap_layer_set_bitmap(button_labels, button_bitmap);
    layer_add_child(root_layer, (Layer*)button_labels);

    // Set up lap time stuff, too.
    init_lap_window();
	
	struct StopwatchState state;
	if(persist_read_data(PERSIST_STATE, &state, sizeof(state)) != E_DOES_NOT_EXIST) {
		started = state.started;
		start_time = state.start_time;
		elapsed_time = state.elapsed_time;
		pause_time = state.pause_time;
		last_lap_time = state.last_lap_time;
		update_stopwatch();
		if(started) {
			update_timer = app_timer_register(100, handle_timer, NULL);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Started timer to resume persisted state.");
		}
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded persisted state.");
	}
}

void handle_deinit() {
	struct StopwatchState state = (struct StopwatchState){
		.started = started,
		.start_time = start_time,
		.elapsed_time = elapsed_time,
		.pause_time = pause_time,
		.last_lap_time = last_lap_time
	};
	status_t status = persist_write_data(PERSIST_STATE, &state, sizeof(state));
	if(status < S_SUCCESS) {
		APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to persist state: %ld", status);
	}
	deinit_lap_window();
	
	bitmap_layer_destroy(button_labels);
	gbitmap_destroy(button_bitmap);
	for(int i = 0; i < LAP_TIME_SIZE; ++i) {
		text_layer_destroy(lap_layers[i]);
	}
	layer_destroy(line_layer);
	text_layer_destroy(seconds_time_layer);
	text_layer_destroy(big_time_layer);
	fonts_unload_custom_font(big_font);
	fonts_unload_custom_font(seconds_font);
	fonts_unload_custom_font(laps_font);
	window_destroy(window);
}

void draw_line(Layer *me, GContext* ctx) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, GPoint(0, 0), GPoint(140, 0));
    graphics_draw_line(ctx, GPoint(0, 1), GPoint(140, 1));
}

void stop_stopwatch() {
    started = false;
	pause_time = float_time_ms();
    if(update_timer != NULL) {
        app_timer_cancel(update_timer);
        update_timer = NULL;
    }
}

void start_stopwatch() {
    started = true;
	if(start_time == 0) {
		start_time = float_time_ms();
	} else if(pause_time != 0) {
		double interval = float_time_ms() - pause_time;
		start_time += interval;
	}
	status_t status;
    update_timer = app_timer_register(100, handle_timer, NULL);
}

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {
    if(started) {
        stop_stopwatch();
    } else {
        start_stopwatch();
    }
}

void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {
    if(busy_animating) return;
    bool is_running = started;
    stop_stopwatch();
    start_time = 0;
    last_lap_time = 0;
	elapsed_time = 0;
    if(is_running) start_stopwatch();
    update_stopwatch();

    // Animate all the laps away.
    busy_animating = LAP_TIME_SIZE;
    static PropertyAnimation* animations[LAP_TIME_SIZE];
    static GRect targets[LAP_TIME_SIZE];
    for(int i = 0; i < LAP_TIME_SIZE; ++i) {
        shift_lap_layer(&animations[i], (Layer*)lap_layers[i], &targets[i], LAP_TIME_SIZE);
        animation_schedule((Animation*)animations[i]);
    }
    next_lap_layer = 0;
    clear_stored_laps();
}

void lap_time_handler(ClickRecognizerRef recognizer, Window *window) {
    if(busy_animating) return;
    double t = elapsed_time - last_lap_time;
    last_lap_time = elapsed_time;
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Elapsed lap time: %d", (int)(t * 100));
    save_lap_time(t);
}

void update_stopwatch() {
    static char big_time[] = "00:00";
    static char deciseconds_time[] = ".0";
    static char seconds_time[] = ":00";

    // Now convert to hours/minutes/seconds.
    int tenths = (int)(elapsed_time * 10) % 10;
    int seconds = (int)elapsed_time % 60;
    int minutes = (int)elapsed_time / 60 % 60;
    int hours = (int)elapsed_time / 3600;

    // We can't fit three digit hours, so stop timing here.
    if(hours > 99) {
        stop_stopwatch();
        return;
    }
	
	if(hours < 1) {
		snprintf(big_time, 6, "%02d:%02d", minutes, seconds);
		snprintf(deciseconds_time, 3, ".%d", tenths);
	} else {
		snprintf(big_time, 6, "%02d:%02d", hours, minutes);
		snprintf(seconds_time, 4, ":%02d", seconds);
	}

    // Now draw the strings.
    text_layer_set_text(big_time_layer, big_time);
    text_layer_set_text(seconds_time_layer, hours < 1 ? deciseconds_time : seconds_time);
}

void animation_stopped(Animation *animation, void *data) {
	property_animation_destroy((PropertyAnimation*)animation);
    --busy_animating;
}

void shift_lap_layer(PropertyAnimation** animation, Layer* layer, GRect* target, int distance_multiplier) {
    GRect origin = layer_get_frame(layer);
    *target = origin;
    target->origin.y += target->size.h * distance_multiplier;
	*animation = property_animation_create_layer_frame(layer, NULL, target);
    animation_set_duration((Animation*)*animation, 250);
    animation_set_curve((Animation*)*animation, AnimationCurveLinear);
    animation_set_handlers((Animation*)*animation, (AnimationHandlers){
        .stopped = (AnimationStoppedHandler)animation_stopped
    }, NULL);
}

void save_lap_time(double lap_time) {
    if(busy_animating) return;

    static PropertyAnimation* animations[LAP_TIME_SIZE];
    static GRect targets[LAP_TIME_SIZE];

    // Shift them down visually (assuming they actually exist)
    busy_animating = LAP_TIME_SIZE;
    for(int i = 0; i < LAP_TIME_SIZE; ++i) {
        if(i == next_lap_layer) continue; // This is handled separately.
        shift_lap_layer(&animations[i], (Layer*)lap_layers[i], &targets[i], 1);
        animation_schedule((Animation*)animations[i]);
    }

    // Once those are done we can slide our new lap time in.
    format_lap(lap_time, lap_times[next_lap_layer]);

    // Animate it
    static PropertyAnimation* entry_animation;
    //static GRect origin; origin = ;
    //static GRect target; target = ;
    entry_animation = property_animation_create_layer_frame((Layer*)lap_layers[next_lap_layer], &GRect(-139, 52, 139, 26), &GRect(5, 52, 139, 26));
    animation_set_curve((Animation*)entry_animation, AnimationCurveEaseOut);
    animation_set_delay((Animation*)entry_animation, 50);
    animation_set_handlers((Animation*)entry_animation, (AnimationHandlers){
        .stopped = (AnimationStoppedHandler)animation_stopped
    }, NULL);
    animation_schedule((Animation*)entry_animation);
    next_lap_layer = (next_lap_layer + 1) % LAP_TIME_SIZE;

    // Get it into the laps window, too.
    store_lap_time(lap_time);
}

void handle_timer(void* data) {
	if(started) {
		double now = float_time_ms();
		elapsed_time = now - start_time;
		update_timer = app_timer_register(100, handle_timer, NULL);
	}
	update_stopwatch();
}

void handle_display_lap_times(ClickRecognizerRef recognizer, Window *window) {
    show_laps();
}

void config_provider(Window *window) {
	window_single_click_subscribe(BUTTON_RUN, (ClickHandler)toggle_stopwatch_handler);
	window_single_click_subscribe(BUTTON_RESET, (ClickHandler)reset_stopwatch_handler);
	window_single_click_subscribe(BUTTON_LAP, (ClickHandler)lap_time_handler);
	window_long_click_subscribe(BUTTON_LAP, 700, (ClickHandler)handle_display_lap_times, NULL);
}

int main() {
	handle_init();
	app_event_loop();
	handle_deinit();
	return 0;
}