#pragma once
#include <Arduino.h>
#include <lvgl.h>

// ---- Lifecycle ----
void ui_init();     // display + touch + lvgl + backlight, then show home menu
void ui_tick();     // call every loop(): runs active-app poll + lv_timer_handler

// ---- Backlight (manual control avoids the CYD boot flash) ----
void backlight_set(uint8_t level);   // 0..255
void backlight_fade_on();            // smooth ramp to full

// ---- On-board RGB LED ----
void rgb_led_init();
void rgb_led_set(bool r, bool g, bool b);
void rgb_led_rainbow();   // cycling hue, like an RGB mouse / keyboard

// ---- Per-app hooks -------------------------------------------------------
// An app registers an optional per-frame poll and a teardown callback that
// runs when the user leaves the app (returns to the home menu). This is how
// we can drop per-app timers and screens when returning home.
typedef void (*app_poll_cb)();
typedef void (*app_exit_cb)();
void ui_set_active_hooks(app_poll_cb poll, app_exit_cb exit_cb);

// ---- Navigation helpers --------------------------------------------------
// Build a standard full-screen page with a title bar + back button, load it
// (auto-deleting the previous screen to free RAM), and return the content
// container to place widgets in. `back_cb` fires when the back button is hit.
lv_obj_t* ui_make_screen(const char* title, lv_event_cb_t back_cb);

// Build a labelled icon tile (used for menu grids). Returns the button.
lv_obj_t* ui_make_tile(lv_obj_t* parent, const char* symbol, const char* text,
                       lv_event_cb_t cb, void* user_data);

// Return to the home menu (runs the active app's teardown first).
void ui_go_home();
void ui_show_home();

// ---- App entry points (each app owns its own screens) --------------------
void app_wallet_open();
void app_macropad_open();
void app_settings_open();
void app_about_open();
