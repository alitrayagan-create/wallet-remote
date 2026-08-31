#include "ui.h"
#include "config.h"
#include "sd_card.h"
#include "wallet_store.h"
#include "wallet_lock.h"
#include "ble_hid.h"
#include "clip_bridge.h"

static lv_obj_t* s_status = nullptr;
static int s_pick_idx = -1;

enum TxAction {
    TX_USER = 0,
    TX_PASS,
    TX_LOGIN,
    TX_COPY,
};

static void send_text(const char* text) {
    if (!text || !text[0] || !ble_hid_connected()) return;
    ble_hid_type(text);
}

static void copy_password(const WalletEntry* e) {
    if (!e || !e->password[0] || !ble_hid_connected()) return;
    ble_hid_type(e->password);
    delay(40);
    ble_hid_combo(KEY_LEFT_CTRL, 'a');
    delay(30);
    ble_hid_combo(KEY_LEFT_CTRL, 'c');
}

static void send_action(const WalletEntry* e, TxAction a) {
    if (!e) return;
    switch (a) {
        case TX_USER:  send_text(e->username); break;
        case TX_PASS:  send_text(e->password); break;
        case TX_LOGIN:
            send_text(e->username);
            delay(40);
            ble_hid_key(KEY_TAB);
            delay(40);
            send_text(e->password);
            break;
        case TX_COPY:  copy_password(e); break;
    }
}

static lv_obj_t* limited_banner(lv_obj_t* parent) {
    if (wallet_ready()) return nullptr;
    lv_obj_t* b = lv_label_create(parent);
    if (sd_card_present()) {
        lv_label_set_text(b, LV_SYMBOL_WARNING " SD error — check data.json\n"
                             "Format the card as FAT32.");
    } else {
        lv_label_set_text(b, LV_SYMBOL_WARNING " No SD card\n"
                             "Insert a FAT32 microSD to store passwords.");
    }
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(b, SCREEN_W - 24);
    lv_obj_set_style_text_color(b, lv_color_hex(0xffaa44), 0);
    return b;
}

static void wallet_show_passwords();
static void wallet_show_shortcuts();
static void wallet_show_settings();
static void wallet_show_home();
static void show_add_entry();
static void show_capture_login();
static void show_capture_name();
static void show_entry(int idx);
static void show_wallet_lock();
static void show_change_lock();

static void enter_wallet() {
    if (sd_card_mount_ok())
        wallet_load();
    wallet_show_home();
}

static void wallet_back_home_evt(lv_event_t* e) { (void)e; wallet_show_home(); }
static void wallet_exit_home_evt(lv_event_t* e) { (void)e; ui_go_home(); }

static void status_del_cb(lv_event_t* e) {
    (void)e;
    s_status = nullptr;
}

static void refresh_ble_label() {
    if (!s_status) return;
    lv_label_set_text(s_status, ble_hid_connected()
        ? LV_SYMBOL_OK " BLE connected"
        : LV_SYMBOL_BLUETOOTH " Pair 'Wallet Remote'");
}

enum CapState { CAP_IDLE, CAP_WAIT_USER, CAP_WAIT_PASS };
static CapState s_cap = CAP_IDLE;
static char s_cap_user[WALLET_USER_LEN];
static char s_cap_pass[WALLET_PASS_LEN];
static lv_obj_t* s_cap_msg = nullptr;

static void hid_select_copy() {
    if (!ble_hid_connected()) return;
    ble_hid_combo(KEY_LEFT_CTRL, 'a');
    delay(120);
    ble_hid_combo(KEY_LEFT_CTRL, 'c');
}

static void cap_set_msg(const char* text) {
    if (s_cap_msg) lv_label_set_text(s_cap_msg, text);
}

static void wallet_poll() {
    static bool last = false;
    bool now = ble_hid_connected();
    if (now != last) {
        refresh_ble_label();
        last = now;
    }

    if (s_cap == CAP_IDLE) return;
    char tmp[256];
    if (!clip_poll(tmp, sizeof(tmp))) return;

    if (s_cap == CAP_WAIT_USER) {
        if (!tmp[0]) {
            cap_set_msg(LV_SYMBOL_WARNING " No clipboard.");
            clip_helper_close();
            s_cap = CAP_IDLE;
            return;
        }
        strlcpy(s_cap_user, tmp, sizeof(s_cap_user));
        cap_set_msg("Copied username.\nCopying password...");
        ble_hid_key(KEY_TAB);
        delay(200);
        hid_select_copy();
        delay(280);
        clip_begin_request();
        s_cap = CAP_WAIT_PASS;
        return;
    }

    if (s_cap == CAP_WAIT_PASS) {
        strlcpy(s_cap_pass, tmp, sizeof(s_cap_pass));
        s_cap = CAP_IDLE;
        clip_helper_close();
        if (!s_cap_pass[0] && !s_cap_user[0]) {
            cap_set_msg(LV_SYMBOL_WARNING " Nothing was copied.");
            return;
        }
        show_capture_name();
    }
}

static void wallet_exit() {
    clip_cancel();
    clip_helper_close();
    s_cap = CAP_IDLE;
    s_status = nullptr;
    s_cap_msg = nullptr;
}

static void tx_btn_evt(lv_event_t* e) {
    send_action(wallet_entry(s_pick_idx),
                (TxAction)(intptr_t)lv_event_get_user_data(e));
}

static void pin_evt(lv_event_t* e) {
    (void)e;
    const WalletEntry* ent = wallet_entry(s_pick_idx);
    if (!ent) return;
    wallet_set_pinned(s_pick_idx, !ent->pinned);
    show_entry(s_pick_idx);
}

static void delete_confirm_evt(lv_event_t* e) {
    (void)e;
    wallet_remove(s_pick_idx);
    wallet_show_passwords();
}

static void entry_back_evt(lv_event_t* e) { (void)e; wallet_show_passwords(); }

static void show_entry(int idx) {
    s_pick_idx = idx;
    const WalletEntry* e = wallet_entry(idx);
    if (!e) return;

    lv_obj_t* c = ui_make_screen(e->name, entry_back_evt);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 6, 0);

    if (!ble_hid_connected()) {
        lv_obj_t* w = lv_label_create(c);
        lv_label_set_text(w, LV_SYMBOL_BLUETOOTH " Pair 'Wallet Remote' first");
        lv_obj_set_style_text_color(w, lv_color_hex(0xff6644), 0);
        lv_obj_set_style_text_font(w, &lv_font_montserrat_12, 0);
    }

    if (e->username[0]) {
        lv_obj_t* u = lv_label_create(c);
        lv_label_set_text(u, e->username);
        lv_label_set_long_mode(u, LV_LABEL_LONG_DOT);
        lv_obj_set_width(u, SCREEN_W - 32);
        lv_obj_set_style_text_font(u, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(u, lv_color_hex(0x8899aa), 0);
    }

    struct { const char* lbl; TxAction a; uint32_t color; } opts[] = {
        {LV_SYMBOL_EDIT     " Type username", TX_USER,  0x263340},
        {LV_SYMBOL_EYE_CLOSE " Type password", TX_PASS,  0x263340},
        {LV_SYMBOL_KEYBOARD " Type login",     TX_LOGIN, 0x263340},
        {LV_SYMBOL_COPY     " Copy password",  TX_COPY,  0x1a4d2e},
    };
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* btn = lv_btn_create(c);
        lv_obj_set_width(btn, SCREEN_W - 32);
        lv_obj_set_height(btn, 40);
        lv_obj_set_style_bg_color(btn, lv_color_hex(opts[i].color), 0);
        lv_obj_add_event_cb(btn, tx_btn_evt, LV_EVENT_CLICKED, (void*)(intptr_t)opts[i].a);
        lv_obj_t* lb = lv_label_create(btn);
        lv_label_set_text(lb, opts[i].lbl);
        lv_obj_center(lb);
    }

    lv_obj_t* pin = lv_btn_create(c);
    lv_obj_set_width(pin, SCREEN_W - 32);
    lv_obj_set_height(pin, 36);
    lv_obj_set_style_bg_color(pin, lv_color_hex(e->pinned ? 0x4d3a1a : 0x33404d), 0);
    lv_obj_add_event_cb(pin, pin_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* pl = lv_label_create(pin);
    lv_label_set_text(pl, e->pinned ? LV_SYMBOL_OK " Shortcut pinned"
                                    : LV_SYMBOL_PLUS " Pin as shortcut");
    lv_obj_center(pl);

    lv_obj_t* del = lv_btn_create(c);
    lv_obj_set_width(del, SCREEN_W - 32);
    lv_obj_set_height(del, 36);
    lv_obj_set_style_bg_color(del, lv_color_hex(0x6a2222), 0);
    lv_obj_add_event_cb(del, delete_confirm_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* dl = lv_label_create(del);
    lv_label_set_text(dl, LV_SYMBOL_TRASH " Delete entry");
    lv_obj_center(dl);
}

static void pass_row_evt(lv_event_t* e) {
    show_entry((int)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t* s_add_kb = nullptr;
static lv_obj_t* s_add_fields[3] = { nullptr, nullptr, nullptr };
static lv_obj_t* s_add_msg = nullptr;

static void add_ta_focus_evt(lv_event_t* e) {
    if (!s_add_kb) return;
    lv_keyboard_set_textarea(s_add_kb, lv_event_get_target(e));
    lv_obj_clear_flag(s_add_kb, LV_OBJ_FLAG_HIDDEN);
}

static void add_kb_evt(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_add_kb, LV_OBJ_FLAG_HIDDEN);
}

static void add_save_evt(lv_event_t* e) {
    (void)e;
    WalletEntry ent;
    memset(&ent, 0, sizeof(ent));
    strncpy(ent.name,     lv_textarea_get_text(s_add_fields[0]), WALLET_NAME_LEN - 1);
    strncpy(ent.username, lv_textarea_get_text(s_add_fields[1]), WALLET_USER_LEN - 1);
    strncpy(ent.password, lv_textarea_get_text(s_add_fields[2]), WALLET_PASS_LEN - 1);
    ent.pinned = false;

    if (!ent.name[0]) {
        if (s_add_msg) lv_label_set_text(s_add_msg, LV_SYMBOL_WARNING " Name is required");
        return;
    }
    if (wallet_storage_full() || !wallet_add(&ent)) {
        if (s_add_msg) lv_label_set_text(s_add_msg, LV_SYMBOL_WARNING " Storage full or SD error");
        return;
    }
    wallet_show_passwords();
}

static void show_add_entry() {
    if (!wallet_ready()) {
        wallet_show_home();
        return;
    }

    s_add_kb = nullptr;
    for (int i = 0; i < 3; ++i) s_add_fields[i] = nullptr;

    lv_obj_t* c = ui_make_screen("Type manually", wallet_back_home_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    lv_obj_t* form = lv_obj_create(c);
    lv_obj_set_size(form, SCREEN_W - 8, SCREEN_H - 36 - 8);
    lv_obj_align(form, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(form, 4, 0);

    const char* labels[3] = { "Name", "Username", "Password" };
    for (int i = 0; i < 3; ++i) {
        lv_obj_t* lbl = lv_label_create(form);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_t* ta = lv_textarea_create(form);
        lv_textarea_set_one_line(ta, true);
        lv_obj_set_width(ta, SCREEN_W - 24);
        if (i == 2) lv_textarea_set_password_mode(ta, true);
        lv_obj_add_event_cb(ta, add_ta_focus_evt, LV_EVENT_FOCUSED, NULL);
        s_add_fields[i] = ta;
    }

    s_add_msg = lv_label_create(form);
    lv_label_set_text(s_add_msg, "");
    lv_obj_set_style_text_color(s_add_msg, lv_color_hex(0xffaa44), 0);

    lv_obj_t* save = lv_btn_create(form);
    lv_obj_set_width(save, SCREEN_W - 24);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(save, add_save_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(save);
    lv_label_set_text(sl, LV_SYMBOL_SAVE " Save to SD");
    lv_obj_center(sl);

    s_add_kb = lv_keyboard_create(c);
    lv_obj_set_size(s_add_kb, SCREEN_W, 160);
    lv_obj_align(s_add_kb, LV_ALIGN_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(s_add_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_add_kb, add_kb_evt, LV_EVENT_ALL, NULL);
}

static void add_entry_evt(lv_event_t* e) { (void)e; show_add_entry(); }

static void capture_start_evt(lv_event_t* e) {
    (void)e;
    if (!ble_hid_connected()) {
        cap_set_msg(LV_SYMBOL_BLUETOOTH " Pair Wallet Remote first");
        return;
    }
    memset(s_cap_user, 0, sizeof(s_cap_user));
    memset(s_cap_pass, 0, sizeof(s_cap_pass));
    cap_set_msg("Starting helper...");
    clip_helper_start(2);
    cap_set_msg("Copying username...");
    hid_select_copy();
    delay(280);
    clip_begin_request();
    s_cap = CAP_WAIT_USER;
}

static void show_capture_login() {
    s_cap = CAP_IDLE;
    clip_cancel();
    lv_obj_t* c = ui_make_screen("Add from PC", wallet_back_home_evt);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 8, 0);

    lv_obj_t* how = lv_label_create(c);
    lv_label_set_text(how,
        "Click the username box on the PC.\n"
        "Leave that window focused.\n"
        "Then tap the button below.\n"
        "PowerShell opens by itself.");
    lv_label_set_long_mode(how, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(how, SCREEN_W - 28);
    lv_obj_set_style_text_font(how, &lv_font_montserrat_12, 0);

    lv_obj_t* go = lv_btn_create(c);
    lv_obj_set_width(go, SCREEN_W - 32);
    lv_obj_set_height(go, 48);
    lv_obj_set_style_bg_color(go, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(go, capture_start_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* gl = lv_label_create(go);
    lv_label_set_text(gl, LV_SYMBOL_COPY " Add username and password");
    lv_label_set_long_mode(gl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(gl, SCREEN_W - 48);
    lv_obj_set_style_text_align(gl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(gl);

    s_cap_msg = lv_label_create(c);
    lv_label_set_text(s_cap_msg, "");
    lv_label_set_long_mode(s_cap_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_cap_msg, SCREEN_W - 28);
    lv_obj_set_style_text_color(s_cap_msg, lv_color_hex(0xffaa44), 0);
}

static lv_obj_t* s_cap_name_ta = nullptr;
static lv_obj_t* s_cap_name_kb = nullptr;
static lv_obj_t* s_cap_name_msg = nullptr;

static void cap_name_focus(lv_event_t* e) {
    if (!s_cap_name_kb) return;
    lv_keyboard_set_textarea(s_cap_name_kb, lv_event_get_target(e));
    lv_obj_clear_flag(s_cap_name_kb, LV_OBJ_FLAG_HIDDEN);
}

static void cap_name_kb_evt(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_cap_name_kb, LV_OBJ_FLAG_HIDDEN);
}

static void cap_name_save_evt(lv_event_t* e) {
    (void)e;
    WalletEntry ent;
    memset(&ent, 0, sizeof(ent));
    const char* name = s_cap_name_ta ? lv_textarea_get_text(s_cap_name_ta) : "";
    if (!name[0]) {
        if (s_cap_name_msg) lv_label_set_text(s_cap_name_msg, LV_SYMBOL_WARNING " Name is required");
        return;
    }
    strlcpy(ent.name, name, sizeof(ent.name));
    strlcpy(ent.username, s_cap_user, sizeof(ent.username));
    strlcpy(ent.password, s_cap_pass, sizeof(ent.password));
    ent.pinned = true;
    if (wallet_storage_full() || !wallet_add(&ent)) {
        if (s_cap_name_msg) lv_label_set_text(s_cap_name_msg, LV_SYMBOL_WARNING " Storage full or SD error");
        return;
    }
    wallet_show_passwords();
}

static void show_capture_name() {
    s_cap_name_kb = nullptr;
    lv_obj_t* c = ui_make_screen("Name this login", wallet_back_home_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    lv_obj_t* form = lv_obj_create(c);
    lv_obj_set_size(form, SCREEN_W - 8, SCREEN_H - 36 - 8);
    lv_obj_align(form, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(form, 6, 0);

    char preview[96];
    snprintf(preview, sizeof(preview), "User: %s", s_cap_user[0] ? s_cap_user : "(empty)");
    lv_obj_t* prev = lv_label_create(form);
    lv_label_set_text(prev, preview);
    lv_label_set_long_mode(prev, LV_LABEL_LONG_DOT);
    lv_obj_set_width(prev, SCREEN_W - 24);
    lv_obj_set_style_text_font(prev, &lv_font_montserrat_12, 0);

    lv_obj_t* lbl = lv_label_create(form);
    lv_label_set_text(lbl, "Shortcut name");
    s_cap_name_ta = lv_textarea_create(form);
    lv_textarea_set_one_line(s_cap_name_ta, true);
    lv_obj_set_width(s_cap_name_ta, SCREEN_W - 24);
    lv_obj_add_event_cb(s_cap_name_ta, cap_name_focus, LV_EVENT_FOCUSED, NULL);

    s_cap_name_msg = lv_label_create(form);
    lv_label_set_text(s_cap_name_msg, "");
    lv_obj_set_style_text_color(s_cap_name_msg, lv_color_hex(0xffaa44), 0);

    lv_obj_t* save = lv_btn_create(form);
    lv_obj_set_width(save, SCREEN_W - 24);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(save, cap_name_save_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(save);
    lv_label_set_text(sl, LV_SYMBOL_SAVE " Save shortcut");
    lv_obj_center(sl);

    s_cap_name_kb = lv_keyboard_create(c);
    lv_obj_set_size(s_cap_name_kb, SCREEN_W, 150);
    lv_obj_align(s_cap_name_kb, LV_ALIGN_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(s_cap_name_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_cap_name_kb, cap_name_kb_evt, LV_EVENT_ALL, NULL);
}

static void wallet_show_passwords() {
    lv_obj_t* c = ui_make_screen("Passwords", wallet_back_home_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    if (lv_obj_t* b = limited_banner(c)) {
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 0);
        return;
    }

    int listTop = 0;
    if (!wallet_storage_full()) {
        lv_obj_t* addBtn = lv_btn_create(c);
        lv_obj_set_size(addBtn, SCREEN_W - 12, 34);
        lv_obj_align(addBtn, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(addBtn, lv_color_hex(0x1a4d2e), 0);
        lv_obj_add_event_cb(addBtn, add_entry_evt, LV_EVENT_CLICKED, NULL);
        lv_obj_t* al = lv_label_create(addBtn);
        lv_label_set_text(al, LV_SYMBOL_EDIT " Type manually");
        lv_obj_center(al);
        listTop = 40;
    }

    lv_obj_t* list = lv_list_create(c);
    lv_obj_set_size(list, SCREEN_W - 12, SCREEN_H - 36 - 8 - listTop);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x161c22), 0);

    if (wallet_count() == 0) {
        lv_list_add_text(list, "No entries yet.\nTap Add user and pass.");
        return;
    }

    for (int i = 0; i < wallet_count(); ++i) {
        const WalletEntry* e = wallet_entry(i);
        char line[80];
        snprintf(line, sizeof(line), "%s%s", e->pinned ? LV_SYMBOL_OK " " : "", e->name);
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_DIRECTORY, line);
        lv_obj_add_event_cb(btn, pass_row_evt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

static void shortcut_send_evt(lv_event_t* e) {
    const WalletEntry* ent = wallet_entry((int)(intptr_t)lv_event_get_user_data(e));
    if (ent) send_text(ent->password);
}

static void wallet_show_shortcuts() {
    lv_obj_t* c = ui_make_screen("Shortcuts", wallet_back_home_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    if (limited_banner(c)) return;

    lv_obj_t* hint = lv_label_create(c);
    lv_label_set_text(hint, "Tap to type the password on the PC");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* list = lv_list_create(c);
    lv_obj_set_size(list, SCREEN_W - 12, SCREEN_H - 36 - 28);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x161c22), 0);

    bool any = false;
    for (int i = 0; i < wallet_count(); ++i) {
        const WalletEntry* e = wallet_entry(i);
        if (!e->pinned) continue;
        any = true;
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_KEYBOARD, e->name);
        lv_obj_add_event_cb(btn, shortcut_send_evt, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    if (!any)
        lv_list_add_text(list, "No pinned shortcuts.\nOpen an entry and tap\nPin as shortcut.");
}

static void change_lock_evt(lv_event_t* e) { (void)e; show_change_lock(); }

static void wallet_show_settings() {
    lv_obj_t* c = ui_make_screen("Wallet Settings", wallet_back_home_evt);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 10, 0);

    char info[160];
    if (sd_card_mount_ok()) {
        char freeStr[32];
        sd_card_free_str(freeStr, sizeof(freeStr));
        snprintf(info, sizeof(info),
                 LV_SYMBOL_OK " SD card: %s\n"
                 "Passwords: %d  (max %d)\n"
                 "Pinned shortcuts: %d",
                 freeStr, wallet_count(), WALLET_MAX_ENTRIES, wallet_pinned_count());
    } else if (sd_card_present()) {
        snprintf(info, sizeof(info), LV_SYMBOL_WARNING " SD detected but not mounted.\nUse FAT32.");
    } else {
        snprintf(info, sizeof(info), LV_SYMBOL_WARNING " No SD card.\nInsert FAT32 microSD.");
    }
    lv_obj_t* sdInfo = lv_label_create(c);
    lv_label_set_text(sdInfo, info);
    lv_label_set_long_mode(sdInfo, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sdInfo, SCREEN_W - 24);

    lv_obj_t* chg = lv_btn_create(c);
    lv_obj_set_width(chg, SCREEN_W - 32);
    lv_obj_add_event_cb(chg, change_lock_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* cl = lv_label_create(chg);
    lv_label_set_text(cl, LV_SYMBOL_EYE_CLOSE " Change wallet password");
    lv_obj_center(cl);

    lv_obj_t* hint = lv_label_create(c);
    lv_label_set_text(hint,
        "The wallet password is stored on\n"
        "the board (not the SD card).\n"
        "You set it the first time you open\n"
        "Password Wallet.");
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, SCREEN_W - 24);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
}

static void open_pass_evt(lv_event_t* e) { (void)e; wallet_show_passwords(); }
static void open_add_evt(lv_event_t* e)  { (void)e; show_capture_login(); }
static void open_sc_evt(lv_event_t* e)   { (void)e; wallet_show_shortcuts(); }
static void open_wset_evt(lv_event_t* e) { (void)e; wallet_show_settings(); }

static void wallet_show_home() {
    lv_obj_t* c = ui_make_screen("Password Wallet", wallet_exit_home_evt);
    lv_obj_set_style_pad_all(c, 4, 0);

    if (!wallet_ready()) {
        lv_obj_t* b = limited_banner(c);
        if (b) lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 0);
    }

    s_status = lv_label_create(c);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, wallet_ready() ? 0 : 56);
    lv_obj_add_event_cb(s_status, status_del_cb, LV_EVENT_DELETE, NULL);
    refresh_ble_label();

    const int16_t colX[2] = {14, 120};
    const int16_t rowY[2] = {(int16_t)(wallet_ready() ? 28 : 80),
                             (int16_t)(wallet_ready() ? 136 : 188)};

    lv_obj_t* t0 = ui_make_tile(c, LV_SYMBOL_DIRECTORY, "Passwords", open_pass_evt, NULL);
    lv_obj_set_pos(t0, colX[0], rowY[0]);
    lv_obj_t* t1 = ui_make_tile(c, LV_SYMBOL_PLUS, "Add user\nand pass", open_add_evt, NULL);
    lv_obj_set_pos(t1, colX[1], rowY[0]);
    lv_obj_t* t2 = ui_make_tile(c, LV_SYMBOL_KEYBOARD, "Shortcuts", open_sc_evt, NULL);
    lv_obj_set_pos(t2, colX[0], rowY[1]);
    lv_obj_t* t3 = ui_make_tile(c, LV_SYMBOL_SETTINGS, "Settings", open_wset_evt, NULL);
    lv_obj_set_pos(t3, colX[1], rowY[1]);
}

// ---------------------------------------------------------------------------
// Wallet access password (create / unlock / change)
// ---------------------------------------------------------------------------
static lv_obj_t* s_lock_kb = nullptr;
static lv_obj_t* s_lock_msg = nullptr;
static lv_obj_t* s_lock_a = nullptr;
static lv_obj_t* s_lock_b = nullptr;
static lv_obj_t* s_lock_c = nullptr;   // current password (change only)
static bool s_lock_changing = false;

static void lock_ta_focus_evt(lv_event_t* e) {
    if (!s_lock_kb) return;
    lv_keyboard_set_textarea(s_lock_kb, lv_event_get_target(e));
    lv_obj_clear_flag(s_lock_kb, LV_OBJ_FLAG_HIDDEN);
}

static void lock_kb_evt(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_lock_kb, LV_OBJ_FLAG_HIDDEN);
}

static void lock_set_msg(const char* text) {
    if (s_lock_msg) lv_label_set_text(s_lock_msg, text);
}

static bool lock_pass_ok(const char* p) {
    if (!p) return false;
    size_t n = strlen(p);
    return n >= WALLET_LOCK_MIN && n <= WALLET_LOCK_MAX;
}

static void lock_ok_evt(lv_event_t* e) {
    (void)e;
    const char* a = s_lock_a ? lv_textarea_get_text(s_lock_a) : "";
    const char* b = s_lock_b ? lv_textarea_get_text(s_lock_b) : "";

    if (s_lock_changing) {
        const char* cur = s_lock_c ? lv_textarea_get_text(s_lock_c) : "";
        if (!wallet_lock_check(cur)) {
            lock_set_msg(LV_SYMBOL_WARNING " Current password is wrong");
            return;
        }
        if (!lock_pass_ok(a) || strcmp(a, b) != 0) {
            lock_set_msg(LV_SYMBOL_WARNING " New passwords must match (4+ chars)");
            return;
        }
        if (!wallet_lock_set(a)) {
            lock_set_msg(LV_SYMBOL_WARNING " Could not save password");
            return;
        }
        wallet_show_settings();
        return;
    }

    if (!wallet_lock_is_set()) {
        if (!lock_pass_ok(a) || strcmp(a, b) != 0) {
            lock_set_msg(LV_SYMBOL_WARNING " Passwords must match (4+ chars)");
            return;
        }
        if (!wallet_lock_set(a)) {
            lock_set_msg(LV_SYMBOL_WARNING " Could not save password");
            return;
        }
        enter_wallet();
        return;
    }

    if (!wallet_lock_check(a)) {
        lock_set_msg(LV_SYMBOL_WARNING " Wrong password");
        return;
    }
    enter_wallet();
}

static lv_obj_t* lock_field(lv_obj_t* form, const char* label, bool secret) {
    lv_obj_t* lbl = lv_label_create(form);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_t* ta = lv_textarea_create(form);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, WALLET_LOCK_MAX);
    lv_textarea_set_password_mode(ta, secret);
    lv_obj_set_width(ta, SCREEN_W - 24);
    lv_obj_add_event_cb(ta, lock_ta_focus_evt, LV_EVENT_FOCUSED, NULL);
    return ta;
}

static void show_lock_form(const char* title, lv_event_cb_t back_cb,
                           bool creating, bool changing) {
    s_lock_kb = nullptr;
    s_lock_a = s_lock_b = s_lock_c = nullptr;
    s_lock_changing = changing;

    lv_obj_t* c = ui_make_screen(title, back_cb);
    lv_obj_set_style_pad_all(c, 4, 0);

    lv_obj_t* form = lv_obj_create(c);
    lv_obj_set_size(form, SCREEN_W - 8, SCREEN_H - 36 - 8);
    lv_obj_align(form, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(form, 4, 0);

    if (changing)
        s_lock_c = lock_field(form, "Current password", true);
    s_lock_a = lock_field(form, creating || changing ? "New password (4+ chars)" : "Password", true);
    if (creating || changing)
        s_lock_b = lock_field(form, "Confirm password", true);

    s_lock_msg = lv_label_create(form);
    lv_label_set_text(s_lock_msg, creating
        ? "This password unlocks the wallet."
        : "");
    lv_label_set_long_mode(s_lock_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lock_msg, SCREEN_W - 24);
    lv_obj_set_style_text_color(s_lock_msg, lv_color_hex(0xffaa44), 0);
    lv_obj_set_style_text_font(s_lock_msg, &lv_font_montserrat_12, 0);

    lv_obj_t* ok = lv_btn_create(form);
    lv_obj_set_width(ok, SCREEN_W - 24);
    lv_obj_set_style_bg_color(ok, lv_color_hex(0x1a4d2e), 0);
    lv_obj_add_event_cb(ok, lock_ok_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ol = lv_label_create(ok);
    lv_label_set_text(ol, creating ? LV_SYMBOL_OK " Set password"
                                   : changing ? LV_SYMBOL_SAVE " Save password"
                                              : LV_SYMBOL_OK " Unlock");
    lv_obj_center(ol);

    s_lock_kb = lv_keyboard_create(c);
    lv_obj_set_size(s_lock_kb, SCREEN_W, 150);
    lv_obj_align(s_lock_kb, LV_ALIGN_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(s_lock_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_lock_kb, lock_kb_evt, LV_EVENT_ALL, NULL);
}

static void show_wallet_lock() {
    bool creating = !wallet_lock_is_set();
    show_lock_form(creating ? "Set wallet password" : "Unlock wallet",
                   wallet_exit_home_evt, creating, false);
}

static void show_change_lock() {
    show_lock_form("Change password", wallet_back_home_evt, false, true);
}

void app_wallet_open() {
    ui_set_active_hooks(wallet_poll, wallet_exit);
    show_wallet_lock();
}
