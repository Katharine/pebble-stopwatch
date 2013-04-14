#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

void itoa1(int num, char* buffer) {
    const char digits[10] = "0123456789";
    buffer[0] = digits[num % 10];
}

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

// Milliseconds since January 1st 2012 in some timezone, discounting leap years.
// There must be a better way to do this...
time_t get_pebble_time() {
    PblTm t;
    get_time(&t);
    time_t seconds = t.tm_sec;
    seconds += t.tm_min * 60; 
    seconds += t.tm_hour * 3600;
    seconds += t.tm_yday * 86400;
    seconds += (t.tm_year - 2012) * 31536000;
    return seconds * 1000;
}

void format_lap(time_t lap_time, char* buffer) {
    int hundredths = (lap_time / 100) % 10;
    int seconds = (lap_time / 1000) % 60;
    int minutes = (lap_time / 60000) % 60;
    int hours = lap_time / 3600000;

    itoa2(hours, &buffer[0]);
    buffer[2] = ':';
    itoa2(minutes, &buffer[3]);
    buffer[5] = ':';
    itoa2(seconds, &buffer[6]);
    buffer[8] = '.';
    itoa1(hundredths, &buffer[9]);
}