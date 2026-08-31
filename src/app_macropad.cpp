#include "ui.h"
#include "config.h"
#include "ble_hid.h"
#include "shortcut_store.h"
#include "sd_card.h"
#include "clip_bridge.h"
#include <string.h>

static lv_obj_t* s_status = nullptr;
static void status_del_cb(lv_event_t* e) { (void)e; s_status = nullptr; }
static int s_cat = 0;
static int s_lastVol = 25;

static void run_app(const char* cmd) {
    if (!ble_hid_connected()) return;
    ble_hid_combo(KEY_LEFT_GUI, 'r');
    delay(400);
    ble_hid_type(cmd);
    delay(80);
    ble_hid_key(KEY_RETURN);
}

static void run_start(const char* name) {
    if (!ble_hid_connected()) return;
    ble_hid_key(KEY_LEFT_GUI);
    delay(450);
    ble_hid_type(name);
    delay(500);
    ble_hid_key(KEY_RETURN);
}

static void power_cmd(char finalKey) {
    if (!ble_hid_connected()) return;
    ble_hid_combo(KEY_LEFT_GUI, 'x');
    delay(450);
    ble_hid_key('u');
    delay(350);
    ble_hid_key(finalKey);
}

// ---------------------------------------------------------------------------
// Categories / actions
// ---------------------------------------------------------------------------
struct Action { const char* symbol; const char* label; };

static const char* CAT_NAMES[]   = { "Edit", "Media", "System", "Apps", "Power", "Type", "Mouse" };
static const char* CAT_SYMBOLS[] = { LV_SYMBOL_COPY, LV_SYMBOL_AUDIO,
                                     LV_SYMBOL_SETTINGS, LV_SYMBOL_DRIVE,
                                     LV_SYMBOL_POWER, LV_SYMBOL_KEYBOARD,
                                     LV_SYMBOL_GPS };
static const int NUM_CATS = 7;
static const int CAT_TYPE  = 5;
static const int CAT_MOUSE = 6;

static const Action EDIT_ACTS[] = {
    {LV_SYMBOL_COPY, "Copy"}, {LV_SYMBOL_CUT, "Cut"}, {LV_SYMBOL_PASTE, "Paste"},
    {LV_SYMBOL_LOOP, "Undo"}, {LV_SYMBOL_REFRESH, "Redo"}, {LV_SYMBOL_SAVE, "Save"},
    {LV_SYMBOL_LIST, "Sel All"}, {LV_SYMBOL_EYE_OPEN, "Find"},
};
static const Action MEDIA_ACTS[] = {
    {LV_SYMBOL_PLAY, "Play/Pause"}, {LV_SYMBOL_NEXT, "Next"}, {LV_SYMBOL_PREV, "Prev"},
    {LV_SYMBOL_VOLUME_MAX, "Vol +"}, {LV_SYMBOL_VOLUME_MID, "Vol -"}, {LV_SYMBOL_MUTE, "Mute"},
};
static const Action SYS_ACTS[] = {
    {LV_SYMBOL_IMAGE, "Snip"}, {LV_SYMBOL_VIDEO, "Record"}, {LV_SYMBOL_POWER, "Lock"},
    {LV_SYMBOL_HOME, "Desktop"}, {LV_SYMBOL_LIST, "Task View"}, {LV_SYMBOL_DIRECTORY, "Explorer"},
};
static const Action APP_ACTS[] = {
    {LV_SYMBOL_EDIT, "Notepad"}, {LV_SYMBOL_PLUS, "Calc"}, {LV_SYMBOL_WIFI, "Browser"},
    {LV_SYMBOL_GPS, "Terminal"}, {LV_SYMBOL_PLAY, "Steam"}, {LV_SYMBOL_CALL, "Discord"},
};
static const Action POWER_ACTS[] = {
    {LV_SYMBOL_POWER, "Shut Down"}, {LV_SYMBOL_REFRESH, "Restart"}, {LV_SYMBOL_EYE_CLOSE, "Sleep"},
    {LV_SYMBOL_DOWNLOAD, "Hibernate"}, {LV_SYMBOL_CLOSE, "Sign Out"}, {LV_SYMBOL_KEYBOARD, "Lock"},
};

static int cat_count(int cat) {
    switch (cat) {
        case 0: return sizeof(EDIT_ACTS) / sizeof(Action);
        case 1: return sizeof(MEDIA_ACTS) / sizeof(Action);
        case 2: return sizeof(SYS_ACTS) / sizeof(Action);
        case 3: return sizeof(APP_ACTS) / sizeof(Action);
        case 4: return sizeof(POWER_ACTS) / sizeof(Action);
    }
    return 0;
}
static const Action* cat_acts(int cat) {
    switch (cat) {
        case 0: return EDIT_ACTS;
        case 1: return MEDIA_ACTS;
        case 2: return SYS_ACTS;
        case 3: return APP_ACTS;
        case 4: return POWER_ACTS;
    }
    return EDIT_ACTS;
}

static void do_action(int cat, int idx) {
    switch (cat) {
        case 0: // Edit
            switch (idx) {
                case 0: ble_hid_combo(KEY_LEFT_CTRL, 'c'); break;
                case 1: ble_hid_combo(KEY_LEFT_CTRL, 'x'); break;
                case 2: ble_hid_combo(KEY_LEFT_CTRL, 'v'); break;
                case 3: ble_hid_combo(KEY_LEFT_CTRL, 'z'); break;
                case 4: ble_hid_combo(KEY_LEFT_CTRL, 'y'); break;
                case 5: ble_hid_combo(KEY_LEFT_CTRL, 's'); break;
                case 6: ble_hid_combo(KEY_LEFT_CTRL, 'a'); break;
                case 7: ble_hid_combo(KEY_LEFT_CTRL, 'f'); break;
            } break;
        case 1: // Media
            if (!ble_hid_connected()) break;
            switch (idx) {
                case 0: ble_hid_media(KEY_MEDIA_PLAY_PAUSE); break;
                case 1: ble_hid_media(KEY_MEDIA_NEXT_TRACK); break;
                case 2: ble_hid_media(KEY_MEDIA_PREVIOUS_TRACK); break;
                case 3: ble_hid_media(KEY_MEDIA_VOLUME_UP); break;
                case 4: ble_hid_media(KEY_MEDIA_VOLUME_DOWN); break;
                case 5: ble_hid_media(KEY_MEDIA_MUTE); break;
            } break;
        case 2: // System (Windows)
            switch (idx) {
                case 0: ble_hid_combo2(KEY_LEFT_GUI, KEY_LEFT_SHIFT, 's'); break;
                case 1: ble_hid_combo2(KEY_LEFT_GUI, KEY_LEFT_ALT, 'r'); break;
                case 2: ble_hid_combo(KEY_LEFT_GUI, 'l'); break;
                case 3: ble_hid_combo(KEY_LEFT_GUI, 'd'); break;
                case 4: ble_hid_combo(KEY_LEFT_GUI, KEY_TAB); break;
                case 5: ble_hid_combo(KEY_LEFT_GUI, 'e'); break;
            } break;
        case 3: // Apps
            switch (idx) {
                case 0: run_app("notepad"); break;
                case 1: run_app("calc"); break;
                case 2: run_app("https://"); break;  // opens default browser
                case 3: run_app("wt"); break;         // Windows Terminal
                case 4: run_start("Steam"); break;    // Start-menu search
                case 5: run_start("Discord"); break;
            } break;
        case 4: // Power (Win+X quick menu)
            switch (idx) {
                case 0: power_cmd('u'); break;        // shut down
                case 1: power_cmd('r'); break;        // restart
                case 2: power_cmd('s'); break;        // sleep
                case 3: power_cmd('h'); break;        // hibernate
                case 4: power_cmd('i'); break;        // sign out
                case 5: ble_hid_combo(KEY_LEFT_GUI, 'l'); break;
            } break;
    }
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
static void macro_show_categories();

static void action_btn_evt(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    do_action(s_cat, idx);
}

static void cat_back_evt(lv_event_t* e) { (void)e; macro_show_categories(); }

// Relative volume: BLE HID can't set an absolute level, so we send the number
// of Vol+/Vol- key presses needed to move from the last slider position.
static void vol_slider_evt(lv_event_t* e) {
    lv_obj_t* s = lv_event_get_target(e);
    int v = lv_slider_get_value(s);
    int delta = v - s_lastVol;
    s_lastVol = v;
    if (!ble_hid_connected() || delta == 0) return;
    int steps = abs(delta);
    if (steps > 50) steps = 50;
    for (int i = 0; i < steps; ++i) {
        ble_hid_media(delta > 0 ? KEY_MEDIA_VOLUME_UP : KEY_MEDIA_VOLUME_DOWN);
        delay(8);
    }
}

static void macro_show_actions(int cat) {
    s_cat = cat;
    lv_obj_t* c = ui_make_screen(CAT_NAMES[cat], cat_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    s_status = lv_label_create(c);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(s_status, status_del_cb, LV_EVENT_DELETE, NULL);
    lv_label_set_text(s_status, "...");

    // Scrollable 2-column grid: every action stays reachable even with 8 items.
    lv_obj_t* grid = lv_obj_create(c);
    lv_obj_set_size(grid, SCREEN_W - 8, SCREEN_H - 36 - 26);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);

    const Action* acts = cat_acts(cat);
    int n = cat_count(cat);
    const int16_t bw = 104, bh = 64, gap = 8;
    for (int i = 0; i < n; ++i) {
        int col = i % 2, row = i / 2;
        lv_obj_t* b = lv_btn_create(grid);
        lv_obj_set_size(b, bw, bh);
        lv_obj_set_pos(b, col * (bw + gap) + 2, row * (bh + gap) + 2);
        lv_obj_set_style_radius(b, 10, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x263340), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x3a5a76), LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, action_btn_evt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_t* ic = lv_label_create(b);
        lv_label_set_text(ic, acts[i].symbol);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_t* lb = lv_label_create(b);
        lv_label_set_text(lb, acts[i].label);
        lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
        lv_obj_align(lb, LV_ALIGN_BOTTOM_MID, 0, -2);
    }

    if (cat == 1) {                    // Media: relative volume slider below the buttons
        int rows = (n + 1) / 2;
        int y = rows * (bh + gap) + 8;
        lv_obj_t* vlabel = lv_label_create(grid);
        lv_label_set_text(vlabel, LV_SYMBOL_VOLUME_MAX " Volume (drag)");
        lv_obj_set_style_text_font(vlabel, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(vlabel, 4, y);
        lv_obj_t* sl = lv_slider_create(grid);
        lv_obj_set_width(sl, 2 * bw + gap - 6);
        lv_obj_set_pos(sl, 6, y + 24);
        lv_slider_set_range(sl, 0, 50);
        lv_slider_set_value(sl, 25, LV_ANIM_OFF);
        s_lastVol = 25;
        lv_obj_add_event_cb(sl, vol_slider_evt, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

// --- Type category: live keyboard + user-created text shortcuts ---
static void macro_show_type_home();
static void type_back_evt(lv_event_t* e) { (void)e; macro_show_type_home(); }

static lv_obj_t* s_typer_ta = nullptr;

static void typer_kb_evt(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_READY) return;
    if (!s_typer_ta) return;
    const char* txt = lv_textarea_get_text(s_typer_ta);
    if (txt && txt[0] && ble_hid_connected())
        ble_hid_type(txt);
    lv_textarea_set_text(s_typer_ta, "");
}

static void typer_enter_evt(lv_event_t* e) {
    (void)e;
    if (ble_hid_connected()) ble_hid_key(KEY_RETURN);
}

static void macro_show_typer() {
    lv_obj_t* c = ui_make_screen("Type", type_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    s_status = lv_label_create(c);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_obj_add_event_cb(s_status, status_del_cb, LV_EVENT_DELETE, NULL);
    lv_label_set_text(s_status, "...");

    s_typer_ta = lv_textarea_create(c);
    lv_obj_set_size(s_typer_ta, SCREEN_W - 60, 44);
    lv_obj_align(s_typer_ta, LV_ALIGN_TOP_LEFT, 2, 16);
    lv_textarea_set_placeholder_text(s_typer_ta, "Type, tap check to send");

    lv_obj_t* ent = lv_btn_create(c);
    lv_obj_set_size(ent, 46, 44);
    lv_obj_align(ent, LV_ALIGN_TOP_RIGHT, -2, 16);
    lv_obj_add_event_cb(ent, typer_enter_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* el = lv_label_create(ent);
    lv_label_set_text(el, LV_SYMBOL_NEW_LINE);
    lv_obj_center(el);

    lv_obj_t* kb = lv_keyboard_create(c);
    lv_obj_set_size(kb, SCREEN_W, 178);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 4);
    lv_keyboard_set_textarea(kb, s_typer_ta);
    lv_obj_add_event_cb(kb, typer_kb_evt, LV_EVENT_READY, NULL);
}

static int s_kb_idx = -1;
static lv_obj_t* s_kb_fields[2] = { nullptr, nullptr };
static lv_obj_t* s_kb_kb = nullptr;
static lv_obj_t* s_kb_msg = nullptr;

static void kb_ta_focus_evt(lv_event_t* e) {
    if (!s_kb_kb) return;
    lv_keyboard_set_textarea(s_kb_kb, lv_event_get_target(e));
    lv_obj_clear_flag(s_kb_kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_kb_evt(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_kb_kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_save_evt(lv_event_t* e) {
    (void)e;
    KbShortcut s;
    memset(&s, 0, sizeof(s));
    strncpy(s.name, lv_textarea_get_text(s_kb_fields[0]), KB_NAME_LEN - 1);
    strncpy(s.text, lv_textarea_get_text(s_kb_fields[1]), KB_TEXT_LEN - 1);
    if (!s.name[0] || !s.text[0]) {
        if (s_kb_msg) lv_label_set_text(s_kb_msg, LV_SYMBOL_WARNING " Name and text required");
        return;
    }
    if (kb_storage_full() || !kb_add(&s)) {
        if (s_kb_msg) lv_label_set_text(s_kb_msg, LV_SYMBOL_WARNING " Storage full or SD error");
        return;
    }
    macro_show_type_home();
}

static void macro_show_add_kb() {
    s_kb_kb = nullptr;
    s_kb_fields[0] = s_kb_fields[1] = nullptr;

    lv_obj_t* c = ui_make_screen("New shortcut", type_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    lv_obj_t* form = lv_obj_create(c);
    lv_obj_set_size(form, SCREEN_W - 8, SCREEN_H - 36 - 8);
    lv_obj_align(form, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(form, 4, 0);

    const char* labels[2] = { "Name", "Text to type" };
    for (int i = 0; i < 2; ++i) {
        lv_obj_t* lbl = lv_label_create(form);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_t* ta = lv_textarea_create(form);
        lv_textarea_set_one_line(ta, true);
        lv_obj_set_width(ta, SCREEN_W - 24);
        lv_obj_add_event_cb(ta, kb_ta_focus_evt, LV_EVENT_FOCUSED, NULL);
        s_kb_fields[i] = ta;
    }

    s_kb_msg = lv_label_create(form);
    lv_label_set_text(s_kb_msg, "");
    lv_obj_set_style_text_color(s_kb_msg, lv_color_hex(0xffaa44), 0);

    lv_obj_t* save = lv_btn_create(form);
    lv_obj_set_width(save, SCREEN_W - 24);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(save, kb_save_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(save);
    lv_label_set_text(sl, LV_SYMBOL_SAVE " Save shortcut");
    lv_obj_center(sl);

    s_kb_kb = lv_keyboard_create(c);
    lv_obj_set_size(s_kb_kb, SCREEN_W, 160);
    lv_obj_align(s_kb_kb, LV_ALIGN_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(s_kb_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_kb_kb, kb_kb_evt, LV_EVENT_ALL, NULL);
}

static void kb_send_evt(lv_event_t* e) {
    (void)e;
    const KbShortcut* s = kb_entry(s_kb_idx);
    if (s && ble_hid_connected())
        ble_hid_type(s->text);
}

static void kb_delete_evt(lv_event_t* e) {
    (void)e;
    kb_remove(s_kb_idx);
    macro_show_type_home();
}

static void show_kb_item(int idx) {
    s_kb_idx = idx;
    const KbShortcut* s = kb_entry(idx);
    if (!s) return;

    lv_obj_t* c = ui_make_screen(s->name, type_back_evt);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 8, 0);

    lv_obj_t* preview = lv_label_create(c);
    lv_label_set_text(preview, s->text);
    lv_label_set_long_mode(preview, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(preview, SCREEN_W - 32);
    lv_obj_set_style_text_color(preview, lv_color_hex(0x8899aa), 0);

    lv_obj_t* send = lv_btn_create(c);
    lv_obj_set_width(send, SCREEN_W - 32);
    lv_obj_set_height(send, 48);
    lv_obj_set_style_bg_color(send, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(send, kb_send_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(send);
    lv_label_set_text(sl, LV_SYMBOL_KEYBOARD " Type on PC");
    lv_obj_center(sl);

    lv_obj_t* del = lv_btn_create(c);
    lv_obj_set_width(del, SCREEN_W - 32);
    lv_obj_set_height(del, 40);
    lv_obj_set_style_bg_color(del, lv_color_hex(0x6a2222), 0);
    lv_obj_add_event_cb(del, kb_delete_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* dl = lv_label_create(del);
    lv_label_set_text(dl, LV_SYMBOL_TRASH " Delete shortcut");
    lv_obj_center(dl);
}

static void kb_row_evt(lv_event_t* e) {
    show_kb_item((int)(intptr_t)lv_event_get_user_data(e));
}

static void open_typer_evt(lv_event_t* e) { (void)e; macro_show_typer(); }
static void open_add_kb_evt(lv_event_t* e) { (void)e; macro_show_add_kb(); }

static char s_kb_clip[KB_TEXT_LEN];
static bool s_kb_cap = false;
static lv_obj_t* s_kb_cap_msg = nullptr;
static lv_obj_t* s_kb_clip_ta = nullptr;
static lv_obj_t* s_kb_clip_kb = nullptr;
static lv_obj_t* s_kb_clip_msg = nullptr;

static void show_kb_clip_name();

static void kb_clip_start_evt(lv_event_t* e) {
    (void)e;
    if (!ble_hid_connected()) {
        if (s_kb_cap_msg) lv_label_set_text(s_kb_cap_msg, LV_SYMBOL_BLUETOOTH " Pair first");
        return;
    }
    ble_hid_combo(KEY_LEFT_CTRL, 'c');
    delay(80);
    if (s_kb_cap_msg) lv_label_set_text(s_kb_cap_msg, "Starting helper...");
    clip_helper_start(1);
    clip_begin_request();
    s_kb_cap = true;
    if (s_kb_cap_msg) lv_label_set_text(s_kb_cap_msg, "Copying selection...");
}

static void kb_clip_focus(lv_event_t* e) {
    if (!s_kb_clip_kb) return;
    lv_keyboard_set_textarea(s_kb_clip_kb, lv_event_get_target(e));
    lv_obj_clear_flag(s_kb_clip_kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_clip_kb_evt(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_kb_clip_kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_clip_save_evt(lv_event_t* e) {
    (void)e;
    KbShortcut s;
    memset(&s, 0, sizeof(s));
    const char* name = s_kb_clip_ta ? lv_textarea_get_text(s_kb_clip_ta) : "";
    if (!name[0]) {
        if (s_kb_clip_msg) lv_label_set_text(s_kb_clip_msg, LV_SYMBOL_WARNING " Name required");
        return;
    }
    strlcpy(s.name, name, sizeof(s.name));
    strlcpy(s.text, s_kb_clip, sizeof(s.text));
    if (kb_storage_full() || !kb_add(&s)) {
        if (s_kb_clip_msg) lv_label_set_text(s_kb_clip_msg, LV_SYMBOL_WARNING " Storage full or SD error");
        return;
    }
    macro_show_type_home();
}

static void show_kb_clip_name() {
    s_kb_clip_kb = nullptr;
    lv_obj_t* c = ui_make_screen("Name shortcut", type_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);
    lv_obj_t* form = lv_obj_create(c);
    lv_obj_set_size(form, SCREEN_W - 8, SCREEN_H - 36 - 8);
    lv_obj_align(form, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* prev = lv_label_create(form);
    lv_label_set_text(prev, s_kb_clip);
    lv_label_set_long_mode(prev, LV_LABEL_LONG_DOT);
    lv_obj_set_width(prev, SCREEN_W - 24);
    lv_obj_set_style_text_font(prev, &lv_font_montserrat_12, 0);

    lv_obj_t* lbl = lv_label_create(form);
    lv_label_set_text(lbl, "Shortcut name");
    s_kb_clip_ta = lv_textarea_create(form);
    lv_textarea_set_one_line(s_kb_clip_ta, true);
    lv_obj_set_width(s_kb_clip_ta, SCREEN_W - 24);
    lv_obj_add_event_cb(s_kb_clip_ta, kb_clip_focus, LV_EVENT_FOCUSED, NULL);

    s_kb_clip_msg = lv_label_create(form);
    lv_label_set_text(s_kb_clip_msg, "");
    lv_obj_set_style_text_color(s_kb_clip_msg, lv_color_hex(0xffaa44), 0);

    lv_obj_t* save = lv_btn_create(form);
    lv_obj_set_width(save, SCREEN_W - 24);
    lv_obj_add_event_cb(save, kb_clip_save_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(save);
    lv_label_set_text(sl, LV_SYMBOL_SAVE " Save shortcut");
    lv_obj_center(sl);

    s_kb_clip_kb = lv_keyboard_create(c);
    lv_obj_set_size(s_kb_clip_kb, SCREEN_W, 150);
    lv_obj_align(s_kb_clip_kb, LV_ALIGN_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(s_kb_clip_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_kb_clip_kb, kb_clip_kb_evt, LV_EVENT_ALL, NULL);
}

static void macro_show_type_home() {
    if (sd_card_mount_ok() && !kb_ready())
        kb_load();

    lv_obj_t* c = ui_make_screen("Type", cat_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    s_status = lv_label_create(c);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(s_status, status_del_cb, LV_EVENT_DELETE, NULL);
    lv_label_set_text(s_status, "...");

    lv_obj_t* grab = lv_btn_create(c);
    lv_obj_set_size(grab, SCREEN_W - 12, 36);
    lv_obj_align(grab, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_bg_color(grab, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(grab, kb_clip_start_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* gl = lv_label_create(grab);
    lv_label_set_text(gl, LV_SYMBOL_COPY " Copy save from PC");
    lv_obj_center(gl);

    s_kb_cap_msg = lv_label_create(c);
    lv_label_set_text(s_kb_cap_msg, "Select text, then tap copy");
    lv_obj_set_style_text_font(s_kb_cap_msg, &lv_font_montserrat_12, 0);
    lv_obj_align(s_kb_cap_msg, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t* row = lv_obj_create(c);
    lv_obj_set_size(row, SCREEN_W - 12, 36);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t* typeBtn = lv_btn_create(row);
    lv_obj_set_size(typeBtn, 108, 36);
    lv_obj_align(typeBtn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(typeBtn, open_typer_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* tl = lv_label_create(typeBtn);
    lv_label_set_text(tl, LV_SYMBOL_KEYBOARD " Type");
    lv_obj_center(tl);

    lv_obj_t* addBtn = lv_btn_create(row);
    lv_obj_set_size(addBtn, 108, 36);
    lv_obj_align(addBtn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(addBtn, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(addBtn, open_add_kb_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* al = lv_label_create(addBtn);
    lv_label_set_text(al, LV_SYMBOL_PLUS " Add");
    lv_obj_center(al);

    lv_obj_t* list = lv_list_create(c);
    lv_obj_set_size(list, SCREEN_W - 12, SCREEN_H - 36 - 120);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x161c22), 0);

    if (!sd_card_mount_ok()) {
        lv_list_add_text(list, "Insert an SD card to save shortcuts.");
        return;
    }
    if (kb_count() == 0) {
        lv_list_add_text(list, "No shortcuts yet.\nSelect text on the PC,\nthen Copy save from PC.");
        return;
    }
    for (int i = 0; i < kb_count(); ++i) {
        const KbShortcut* s = kb_entry(i);
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_OK, s->name);
        lv_obj_add_event_cb(btn, kb_row_evt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

static int16_t s_pad_lx = -1, s_pad_ly = -1;

static void mouse_pad_evt(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    lv_point_t p;
    if (indev) lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        s_pad_lx = p.x;
        s_pad_ly = p.y;
        return;
    }
    if (code == LV_EVENT_PRESSING && s_pad_lx >= 0) {
        int dx = (p.x - s_pad_lx) * 3;
        int dy = (p.y - s_pad_ly) * 3;
        s_pad_lx = p.x;
        s_pad_ly = p.y;
        if (dx > 127) dx = 127;
        if (dx < -127) dx = -127;
        if (dy > 127) dy = 127;
        if (dy < -127) dy = -127;
        if (dx || dy) ble_hid_mouse_move((int8_t)dx, (int8_t)dy, 0);
        return;
    }
    if (code == LV_EVENT_RELEASED)
        s_pad_lx = s_pad_ly = -1;
}

static void mouse_l_evt(lv_event_t* e) { (void)e; ble_hid_mouse_click(MOUSE_LEFT); }
static void mouse_r_evt(lv_event_t* e) { (void)e; ble_hid_mouse_click(MOUSE_RIGHT); }

static void macro_show_mouse() {
    lv_obj_t* c = ui_make_screen("Mouse", cat_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    s_status = lv_label_create(c);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(s_status, status_del_cb, LV_EVENT_DELETE, NULL);
    lv_label_set_text(s_status, "...");

    lv_obj_t* pad = lv_obj_create(c);
    lv_obj_set_size(pad, SCREEN_W - 16, SCREEN_H - 36 - 78);
    lv_obj_align(pad, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x1a222b), 0);
    lv_obj_set_style_radius(pad, 12, 0);
    lv_obj_set_style_border_width(pad, 0, 0);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pad, mouse_pad_evt, LV_EVENT_ALL, NULL);
    lv_obj_t* pl = lv_label_create(pad);
    lv_label_set_text(pl, "Drag to move");
    lv_obj_center(pl);

    lv_obj_t* lbtn = lv_btn_create(c);
    lv_obj_set_size(lbtn, 108, 40);
    lv_obj_align(lbtn, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_obj_add_event_cb(lbtn, mouse_l_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ll = lv_label_create(lbtn);
    lv_label_set_text(ll, "Left click");
    lv_obj_center(ll);

    lv_obj_t* rbtn = lv_btn_create(c);
    lv_obj_set_size(rbtn, 108, 40);
    lv_obj_align(rbtn, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_add_event_cb(rbtn, mouse_r_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rl = lv_label_create(rbtn);
    lv_label_set_text(rl, "Right click");
    lv_obj_center(rl);
}

static void cat_tile_evt(lv_event_t* e) {
    int cat = (int)(intptr_t)lv_event_get_user_data(e);
    if (cat == CAT_TYPE)       macro_show_type_home();
    else if (cat == CAT_MOUSE) macro_show_mouse();
    else                       macro_show_actions(cat);
}

static void macro_home_back_evt(lv_event_t* e) { (void)e; ui_go_home(); }

static void macro_show_categories() {
    lv_obj_t* c = ui_make_screen("Macro Pad", macro_home_back_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    s_status = lv_label_create(c);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(s_status, status_del_cb, LV_EVENT_DELETE, NULL);
    lv_label_set_text(s_status, "BLE: starting...");

    // Scrollable 2-column grid so all categories (incl. Power) are reachable.
    lv_obj_t* grid = lv_obj_create(c);
    lv_obj_set_size(grid, SCREEN_W - 8, SCREEN_H - 36 - 30);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);

    const int16_t bw = 102, bh = 96, gap = 8;
    for (int i = 0; i < NUM_CATS; ++i) {
        int col = i % 2, row = i / 2;
        lv_obj_t* t = ui_make_tile(grid, CAT_SYMBOLS[i], CAT_NAMES[i],
                                   cat_tile_evt, (void*)(intptr_t)i);
        lv_obj_set_pos(t, col * (bw + gap) + 2, row * (bh + gap) + 2);
    }
}

// ---------------------------------------------------------------------------
// Poll + teardown
// ---------------------------------------------------------------------------
static void macro_poll() {
    if (s_status) {
        static bool last = false;
        bool now = ble_hid_connected();
        if (now != last || lv_label_get_text(s_status)[0] == 'B') {
            lv_label_set_text(s_status, now ? LV_SYMBOL_OK " Connected to PC"
                                            : LV_SYMBOL_BLUETOOTH " Pair 'Wallet Remote'");
            last = now;
        }
    }

    if (s_kb_cap) {
        char tmp[256];
        if (clip_poll(tmp, sizeof(tmp))) {
            s_kb_cap = false;
            if (!tmp[0]) {
                if (s_kb_cap_msg)
                    lv_label_set_text(s_kb_cap_msg,
                        LV_SYMBOL_WARNING " No clipboard.");
            } else {
                strlcpy(s_kb_clip, tmp, sizeof(s_kb_clip));
                show_kb_clip_name();
            }
            clip_helper_close();
        }
    }
}

static void macro_exit() {
    clip_cancel();
    clip_helper_close();
    s_kb_cap = false;
    s_status = nullptr;
    s_kb_cap_msg = nullptr;
}

void app_macropad_open() {
    if (sd_card_mount_ok())
        kb_load();
    macro_show_categories();
    ui_set_active_hooks(macro_poll, macro_exit);
}
