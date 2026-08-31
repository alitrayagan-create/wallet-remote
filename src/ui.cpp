#include "ui.h"
#include "config.h"
#include "sd_card.h"
#include "ble_hid.h"
#include <TFT_eSPI.h>
#include <SPI.h>

// ---------------------------------------------------------------------------
// Globals: display, LVGL buffers
// ---------------------------------------------------------------------------
static TFT_eSPI tft = TFT_eSPI();

// Touch is read via SOFTWARE (bit-banged) SPI on the XPT2046 pins. This is
// essential on the CYD: the touch controller and the microSD card both sit on
// the VSPI hardware bus (display owns HSPI). If the touch used VSPI hardware,
// mounting the SD card re-routes that bus and touch dies. Bit-banging keeps the
// touch fully independent of the SD card. The XPT2046 only needs ~2.5 MHz, so
// software SPI is plenty fast.
static uint16_t xpt_read(uint8_t cmd) {
    digitalWrite(XPT2046_CS, LOW);
    for (int i = 7; i >= 0; --i) {          // shift out 8-bit command
        digitalWrite(XPT2046_MOSI, (cmd >> i) & 0x01);
        digitalWrite(XPT2046_CLK, HIGH);
        digitalWrite(XPT2046_CLK, LOW);
    }
    digitalWrite(XPT2046_CLK, HIGH);         // busy/dummy clock after command
    digitalWrite(XPT2046_CLK, LOW);
    uint16_t val = 0;
    for (int i = 11; i >= 0; --i) {          // read 12-bit result, MSB first
        digitalWrite(XPT2046_CLK, HIGH);
        val |= (uint16_t)(digitalRead(XPT2046_MISO) & 0x01) << i;
        digitalWrite(XPT2046_CLK, LOW);
    }
    digitalWrite(XPT2046_CS, HIGH);
    return val;
}

static void touch_hw_init() {
    pinMode(XPT2046_CS, OUTPUT);
    pinMode(XPT2046_CLK, OUTPUT);
    pinMode(XPT2046_MOSI, OUTPUT);
    pinMode(XPT2046_MISO, INPUT);
    digitalWrite(XPT2046_CS, HIGH);
    digitalWrite(XPT2046_CLK, LOW);
}

// Partial draw buffer, 16bpp, double-buffered. Kept small (20 lines) so the
// BLE stack has enough internal RAM to initialize alongside SD + LVGL.
static const uint32_t DRAW_BUF_PX = SCREEN_W * 20;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// Active-app hooks
static app_poll_cb g_poll = nullptr;
static app_exit_cb g_exit = nullptr;

static bool g_first_flush_done = false;

// ---------------------------------------------------------------------------
// Backlight (manual LEDC PWM — kept OFF through init to kill the boot flash)
// ---------------------------------------------------------------------------
void backlight_set(uint8_t level) {
    ledcWrite(BL_LEDC_CHANNEL, level);
}

void backlight_fade_on() {
    for (int i = 0; i <= 255; i += 8) {
        ledcWrite(BL_LEDC_CHANNEL, i);
        delay(4);
    }
    ledcWrite(BL_LEDC_CHANNEL, 255);
}

static void backlight_init_off() {
    ledcSetup(BL_LEDC_CHANNEL, BL_LEDC_FREQ, BL_LEDC_RES);
    ledcAttachPin(PIN_BACKLIGHT, BL_LEDC_CHANNEL);
    ledcWrite(BL_LEDC_CHANNEL, 0);   // dark until the first frame is ready
}

// ---------------------------------------------------------------------------
// On-board RGB LED (common-anode: PWM duty 0 = full on, 255 = off)
// ---------------------------------------------------------------------------
static bool s_rgb_cycle = false;
static uint16_t s_rgb_hue = 0;
static uint32_t s_rgb_t = 0;

static void rgb_led_write(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LEDC_RGB_R, 255 - r);
    ledcWrite(LEDC_RGB_G, 255 - g);
    ledcWrite(LEDC_RGB_B, 255 - b);
}

static void hsv_to_rgb(uint16_t h, uint8_t& r, uint8_t& g, uint8_t& b) {
    h %= 360;
    uint8_t region = h / 60;
    uint16_t rem = (h % 60) * 255 / 60;
    uint8_t p = 0;
    uint8_t q = 255 - rem;
    uint8_t t = rem;
    switch (region) {
        case 0: r = 255; g = t;   b = p;   break;
        case 1: r = q;   g = 255; b = p;   break;
        case 2: r = p;   g = 255; b = t;   break;
        case 3: r = p;   g = q;   b = 255; break;
        case 4: r = t;   g = p;   b = 255; break;
        default: r = 255; g = p;  b = q;   break;
    }
}

void rgb_led_init() {
    ledcSetup(LEDC_RGB_R, BL_LEDC_FREQ, BL_LEDC_RES);
    ledcSetup(LEDC_RGB_G, BL_LEDC_FREQ, BL_LEDC_RES);
    ledcSetup(LEDC_RGB_B, BL_LEDC_FREQ, BL_LEDC_RES);
    ledcAttachPin(PIN_LED_R, LEDC_RGB_R);
    ledcAttachPin(PIN_LED_G, LEDC_RGB_G);
    ledcAttachPin(PIN_LED_B, LEDC_RGB_B);
    rgb_led_write(0, 0, 0);
}

void rgb_led_set(bool r, bool g, bool b) {
    s_rgb_cycle = false;
    rgb_led_write(r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
}

void rgb_led_rainbow() {
    s_rgb_cycle = true;
    s_rgb_t = 0;
}

static void rgb_led_tick() {
    if (!s_rgb_cycle) return;
    uint32_t now = millis();
    if (now - s_rgb_t < 16) return;
    s_rgb_t = now;
    s_rgb_hue = (uint16_t)((s_rgb_hue + 2) % 360);  // ~3 s per loop
    uint8_t r, g, b;
    hsv_to_rgb(s_rgb_hue, r, g, b);
    rgb_led_write(r, g, b);
}

// ---------------------------------------------------------------------------
// LVGL display flush
// ---------------------------------------------------------------------------
static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);  // true = swap bytes
    tft.endWrite();

    lv_disp_flush_ready(drv);

    if (!g_first_flush_done) {
        g_first_flush_done = true;   // main loop turns the backlight on after this
    }
}

// ---------------------------------------------------------------------------
// LVGL touch read
// ---------------------------------------------------------------------------
static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    // Pressure: z = z1 + (4095 - z2). Low when untouched, high when pressed.
    uint16_t z1 = xpt_read(0xB0);   // Z1
    uint16_t z2 = xpt_read(0xC0);   // Z2
    int z = (int)z1 + 4095 - (int)z2;

    if (z < 400) {                  // not pressed
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    // Average a few samples for stability. 0x90 = width axis, 0xD0 = height axis
    // (matches the axis assignment the previous library-based code used).
    uint32_t sw = 0, sh = 0;
    const int N = 3;
    for (int i = 0; i < N; ++i) {
        sw += xpt_read(0x90);
        sh += xpt_read(0xD0);
    }
    uint16_t rawW = sw / N;
    uint16_t rawH = sh / N;

#if TOUCH_SWAP_XY
    uint16_t tmp = rawW; rawW = rawH; rawH = tmp;
#endif

    int16_t x = map(rawW, TOUCH_RAW_MIN_X, TOUCH_RAW_MAX_X, 0, SCREEN_W - 1);
    int16_t y = map(rawH, TOUCH_RAW_MIN_Y, TOUCH_RAW_MAX_Y, 0, SCREEN_H - 1);
    x = constrain(x, 0, SCREEN_W - 1);
    y = constrain(y, 0, SCREEN_H - 1);

#if TOUCH_INVERT_X
    x = (SCREEN_W - 1) - x;
#endif
#if TOUCH_INVERT_Y
    y = (SCREEN_H - 1) - y;
#endif

#if TOUCH_DEBUG
    static uint32_t lastDbg = 0;
    if (millis() - lastDbg > 200) {
        Serial.printf("TOUCH z=%d rawW=%d rawH=%d -> %d,%d\n", z, rawW, rawH, x, y);
        lastDbg = millis();
    }
#endif

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
}

bool ui_first_frame_done() { return g_first_flush_done; }

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
void ui_set_active_hooks(app_poll_cb poll, app_exit_cb exit_cb) {
    g_poll = poll;
    g_exit = exit_cb;
}

void ui_go_home() {
    if (g_exit) {
        app_exit_cb e = g_exit;
        g_exit = nullptr;
        g_poll = nullptr;
        e();                 // stop the active app
    }
    ui_show_home();
}

static void back_home_event(lv_event_t* e) {
    (void)e;
    ui_go_home();
}

// Standard page with title bar + back button.
lv_obj_t* ui_make_screen(const char* title, lv_event_cb_t back_cb) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, 36);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1e2630), 0);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* back = lv_btn_create(bar);
    lv_obj_set_size(back, 44, 28);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x33404d), 0);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_center(bl);

    lv_obj_t* tl = lv_label_create(bar);
    lv_label_set_text(tl, title);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, 0);
    lv_obj_align(tl, LV_ALIGN_CENTER, 0, 0);

    // Content container below the bar
    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_size(content, SCREEN_W, SCREEN_H - 36);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 8, 0);

    // Load and auto-delete the previous screen (frees its RAM).
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    return content;
}

lv_obj_t* ui_make_tile(lv_obj_t* parent, const char* symbol, const char* text,
                       lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 102, 96);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x263340), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a5a76), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_pad_all(btn, 4, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t* icon = lv_label_create(btn);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_width(lbl, 94);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
    return btn;
}

// ---------------------------------------------------------------------------
// Home menu (main menu: one tile per app)
// ---------------------------------------------------------------------------
static void open_wallet_evt(lv_event_t* e)    { (void)e; app_wallet_open(); }
static void open_macropad_evt(lv_event_t* e)  { (void)e; app_macropad_open(); }
static void open_settings_evt(lv_event_t* e)      { (void)e; app_settings_open(); }
static void open_about_evt(lv_event_t* e)         { (void)e; app_about_open(); }

void ui_show_home() {
    g_poll = nullptr;
    g_exit = nullptr;

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0c0f13), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header = lv_label_create(scr);
    lv_label_set_text(header, "Wallet Remote");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_20, 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 10, 10);

    // SD card indicator (top center) when a card is present.
    if (sd_card_present()) {
        lv_obj_t* sd = lv_obj_create(scr);
        lv_obj_set_size(sd, 36, 22);
        lv_obj_align(sd, LV_ALIGN_TOP_MID, 0, 12);
        lv_obj_set_style_radius(sd, 6, 0);
        lv_obj_set_style_bg_color(sd, lv_color_hex(0x1a4d2e), 0);
        lv_obj_set_style_border_width(sd, 0, 0);
        lv_obj_clear_flag(sd, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* sdLbl = lv_label_create(sd);
        lv_label_set_text(sdLbl, LV_SYMBOL_SAVE " SD");
        lv_obj_set_style_text_font(sdLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(sdLbl);
    }

    int tileTop = 44;
    if (!sd_card_present()) {
        lv_obj_t* warn = lv_label_create(scr);
        lv_label_set_text(warn,
            LV_SYMBOL_WARNING " No SD card\n"
            "Insert a FAT32 microSD to store\n"
            "logins and shortcuts.");
        lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(warn, SCREEN_W - 20);
        lv_obj_set_style_text_color(warn, lv_color_hex(0xffaa44), 0);
        lv_obj_set_style_text_font(warn, &lv_font_montserrat_12, 0);
        lv_obj_align(warn, LV_ALIGN_TOP_MID, 0, 36);
        tileTop = 108;
    }

    const int16_t colX[2] = {16, 122};
    const int16_t rowY[2] = {(int16_t)tileTop, (int16_t)(tileTop + 108)};

    lv_obj_t* t0 = ui_make_tile(scr, LV_SYMBOL_DIRECTORY, "Password\nWallet", open_wallet_evt, NULL);
    lv_obj_set_pos(t0, colX[0], rowY[0]);
    lv_obj_t* t1 = ui_make_tile(scr, LV_SYMBOL_KEYBOARD, "Macro Pad",     open_macropad_evt,      NULL);
    lv_obj_set_pos(t1, colX[1], rowY[0]);
    lv_obj_t* t2 = ui_make_tile(scr, LV_SYMBOL_SETTINGS, "Settings",      open_settings_evt,      NULL);
    lv_obj_set_pos(t2, colX[0], rowY[1]);
    lv_obj_t* t3 = ui_make_tile(scr, LV_SYMBOL_LIST,     "About",         open_about_evt,         NULL);
    lv_obj_set_pos(t3, colX[1], rowY[1]);

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

// ---------------------------------------------------------------------------
// Settings app (brightness + RGB LED) and About app
// ---------------------------------------------------------------------------
static void brightness_slider_evt(lv_event_t* e) {
    lv_obj_t* s = lv_event_get_target(e);
    int v = lv_slider_get_value(s);
    if (v < 20) v = 20;             // never fully dark
    backlight_set((uint8_t)v);
}

// RGB LED color swatches. Encode r/g/b bits in the user_data pointer.
static void rgb_color_evt(lv_event_t* e) {
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    if (v < 0) {
        rgb_led_rainbow();
        return;
    }
    rgb_led_set(v & 0x4, v & 0x2, v & 0x1);
}

void app_settings_open() {
    lv_obj_t* c = ui_make_screen("Settings", back_home_event);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c, 8, 0);

    lv_obj_t* bl = lv_label_create(c);
    lv_label_set_text(bl, "Brightness");

    lv_obj_t* slider = lv_slider_create(c);
    lv_obj_set_width(slider, SCREEN_W - 60);
    lv_slider_set_range(slider, 20, 255);
    lv_slider_set_value(slider, 255, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_slider_evt, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* rl = lv_label_create(c);
    lv_label_set_text(rl, "RGB LED");

    // Color grid: label + rgb-bit value (bit2=R, bit1=G, bit0=B). bits=-1 = rainbow.
    struct { const char* name; uint32_t hex; int bits; int w; } cols[] = {
        {"Off",   0x444444,  0, 100},
        {"Red",   0xd83a3a,  4, 100},
        {"Green", 0x3ad85a,  2, 100},
        {"Blue",  0x3a6ad8,  1, 100},
        {"White", 0xeeeeee,  7, 100},
        {"RGB",   0x7b2dff, -1, 208},
    };
    lv_obj_t* grid = lv_obj_create(c);
    lv_obj_set_size(grid, SCREEN_W - 24, 130);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 2, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (auto& col : cols) {
        lv_obj_t* b = lv_btn_create(grid);
        lv_obj_set_size(b, col.w, 32);
        lv_obj_add_event_cb(b, rgb_color_evt, LV_EVENT_CLICKED, (void*)(intptr_t)col.bits);
        lv_obj_t* lbl = lv_label_create(b);
        lv_label_set_text(lbl, col.name);
        if (col.bits == 7)
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x222222), 0);
        lv_obj_center(lbl);
    }

    ui_set_active_hooks(nullptr, nullptr);
}

void app_about_open() {
    lv_obj_t* c = ui_make_screen("About", back_home_event);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* t1 = lv_label_create(c);
    lv_label_set_text(t1, "Wallet Remote v2.2\n"
                          "ESP32-2432S028\n\n"
                          "Password Wallet (SD card)\n"
                          "Macro Pad (BLE keyboard)\n\n"
                          "Pair as 'Wallet Remote'.\n"
                          "Power from USB or a battery.\n"
                          "Copy from PC opens PowerShell,\n"
                          "then closes it when done.\n"
                          "FAT32 microSD stores logins.");
    lv_label_set_long_mode(t1, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t1, SCREEN_W - 40);

    char buf[64];
    snprintf(buf, sizeof(buf), "Free heap: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
    lv_obj_t* t2 = lv_label_create(c);
    lv_label_set_text(t2, buf);

    ui_set_active_hooks(nullptr, nullptr);
}

// ---------------------------------------------------------------------------
// Init + tick
// ---------------------------------------------------------------------------
void ui_init() {
    backlight_init_off();          // backlight OFF first (no flash)

    tft.init();
    tft.setRotation(UI_ROTATION);
    tft.fillScreen(TFT_BLACK);     // clear any garbage while still dark

    touch_hw_init();

    lv_init();
    buf1 = (lv_color_t*)heap_caps_malloc(DRAW_BUF_PX * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = (lv_color_t*)heap_caps_malloc(DRAW_BUF_PX * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DRAW_BUF_PX);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W;
    disp_drv.ver_res = SCREEN_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    rgb_led_init();
    sd_card_init();

    // Start BLE HID ONCE and keep it alive for the whole session. Both the
    // Macro Pad and the Password Wallet use it; tearing it down on every app
    // switch crashed the BT stack and dropped the laptop connection.
    ble_hid_begin();

    ui_show_home();
}

void ui_tick() {
    rgb_led_tick();
    if (g_poll) g_poll();
    lv_timer_handler();
}
