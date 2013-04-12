#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0xC2, 0x5A, 0x8D, 0x50, 0x10, 0x5F, 0x45, 0xBF, 0xB9, 0x92, 0xCF, 0xF9, 0x58, 0xA7, 0x93, 0xAD }
PBL_APP_INFO(MY_UUID,
             "Stopwatch", "Katharine Berry",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window;
TextLayer big_time_layer;
TextLayer seconds_time_layer;
Layer line_layer;

AppTimerHandle timer_handle;
AppContextRef app;

int lap_times[3] = {0, 0, 0};
int num_laps = 0;

// The documentation claims this is defined, but it is not.
// Define it here for now.
#ifndef APP_TIMER_INVALID_HANDLE
    #define APP_TIMER_INVALID_HANDLE 0xDEADBEEF
#endif

time_t start_time = 0;
time_t elapsed_time = 0;
bool started = false;
AppTimerHandle update_timer = APP_TIMER_INVALID_HANDLE;

#define TIMER_UPDATE 1
#define FONT_BIG_TIME RESOURCE_ID_FONT_DEJAVU_SANS_BOLD_SUBSET_32
#define FONT_SECONDS RESOURCE_ID_FONT_DEJAVU_SANS_SUBSET_20

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void config_provider(ClickConfig **config, Window *window);
void handle_init(AppContextRef ctx);
time_t time_seconds();
void stop_stopwatch();
void start_stopwatch();
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void itoa2(int num, char* buffer);
void update_stopwatch();
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie);
void pbl_main(void *params);
void draw_line(Layer *me, GContext* ctx);

void config_provider(ClickConfig **config, Window *window)
{
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler)toggle_stopwatch_handler;
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler)reset_stopwatch_handler;
    (void)window;
}

void handle_init(AppContextRef ctx) {
  app = ctx;



  window_init(&window, "Stopwatch");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorBlack);
  window_set_fullscreen(&window, false);

  resource_init_current_app(&APP_RESOURCES);

  // Arrange for user input.
  window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);


  // Set up the big timer.
  GRect big_time_position = {.origin = {.x = 0, .y = 5}, .size = {.w = 105, .h = 35}};

  text_layer_init(&big_time_layer, big_time_position);
  text_layer_set_background_color(&big_time_layer, GColorBlack);
  text_layer_set_font(&big_time_layer, fonts_load_custom_font(resource_get_handle(FONT_BIG_TIME)));
  text_layer_set_text_color(&big_time_layer, GColorWhite);
  text_layer_set_text(&big_time_layer, "00:00");
  text_layer_set_text_alignment(&big_time_layer, GTextAlignmentRight);
  layer_add_child(&window.layer, &big_time_layer.layer);

  GRect seconds_time_position = {.origin = {.x = 107, .y = 17}, .size = {.w = 33, .h = 35}};

  text_layer_init(&seconds_time_layer, seconds_time_position);
  text_layer_set_background_color(&seconds_time_layer, GColorBlack);
  text_layer_set_font(&seconds_time_layer, fonts_load_custom_font(resource_get_handle(FONT_SECONDS)));
  text_layer_set_text_color(&seconds_time_layer, GColorWhite);
  text_layer_set_text(&seconds_time_layer, ":00");
  layer_add_child(&window.layer, &seconds_time_layer.layer);

  // Draw our nice line.
  GRect line_position = {.origin = {.x = 0, .y = 50}, .size = {.w = 144, .h = 2}};
  layer_init(&line_layer, line_position);
  line_layer.update_proc = &draw_line;
  layer_add_child(&window.layer, &line_layer);
}


void draw_line(Layer *me, GContext* ctx) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, GPoint(0, 0), GPoint(140, 0));
    graphics_draw_line(ctx, GPoint(0, 1), GPoint(140, 1));
}

// Time since January 1st 2012 in some timezone, discounting leap years.
// There must be a better way to do this...
time_t time_seconds() {
    PblTm t;
    get_time(&t);
    time_t seconds = t.tm_sec;
    seconds += t.tm_min * 60; 
    seconds += t.tm_hour * 3600;
    seconds += t.tm_yday * 86400;
    seconds += (t.tm_year - 2012) * 31536000;
    return seconds;
}

void stop_stopwatch()
{
    started = false;
    if(update_timer != APP_TIMER_INVALID_HANDLE) {
        if(app_timer_cancel_event(app, update_timer)) {
            update_timer = APP_TIMER_INVALID_HANDLE;
        }
    }
}

void start_stopwatch()
{
    // Hack: set the start time to now minus the previously recorded time.
    // This lets us pause and unpause the timer.
    start_time = time_seconds() - elapsed_time;
    started = true;

    update_timer = app_timer_send_event(app, 1000, TIMER_UPDATE);
}

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window)
{
    if(started) {
        stop_stopwatch();
    } else {
        start_stopwatch();
    }
}

void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window)
{
    elapsed_time = 0;
    start_time = time_seconds();
    update_stopwatch();
}

// Surely there must be some way of doing this...
void itoa2(int num, char* buffer) {
    const char digits[10] = "0123456789";
    if(num > 99) {
        buffer[0] = '9';
        buffer[1] = '9';
        return;
    } else if(num > 9) {
        buffer[0] = digits[num / 10];
    } else {
        buffer[0] = '0';
    }
    buffer[1] = digits[num % 10];
}

void update_stopwatch()
{
    static char big_time[] = "00:00";
    static char seconds_time[] = ":00";
    if(!started) return; // Bail if we aren't started.
    elapsed_time = time_seconds() - start_time;

    // Now convert to hours/minutes/seconds.
    int seconds = elapsed_time % 60;
    int minutes = (elapsed_time / 60) % 60;
    int hours = elapsed_time / 3600;

    // We can't fit three digit hours, so stop timing here.
    if(hours > 99) {
        stop_stopwatch();
        return;
    }

    itoa2(hours, &big_time[0]);
    itoa2(minutes, &big_time[3]);
    itoa2(seconds, &seconds_time[1]);

    // Now draw the strings.
    text_layer_set_text(&big_time_layer, big_time);
    text_layer_set_text(&seconds_time_layer, seconds_time);
}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
    (void)handle;
    if(cookie == TIMER_UPDATE) {
        update_stopwatch();
        update_timer = app_timer_send_event(ctx, 1000, TIMER_UPDATE);
    }
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .timer_handler = &handle_timer
  };
  app_event_loop(params, &handlers);
}
