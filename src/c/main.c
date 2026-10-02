#include <pebble.h>

#define KEY_TRIGGER 0
#define KEY_UPDATE 1
#define KEY_STATUS 2
#define KEY_COUNT 3
#define KEY_ITEM_START 4
#define KEY_TRIGGER_ID 5     // id of the triggered webhook, lets the phone find it even if the list changed
#define KEY_AUTO_CLOSE 30
#define KEY_HEADER_COLOR 31
#define KEY_HIGHLIGHT_COLOR 32
#define KEY_DARK_LIST 33
#define KEY_TOUCH 34
#define KEY_SOUND_FEEDBACK 40

#define KEY_NAME_BASE 100    // + slot inside a batch
#define KEY_DESC_BASE 200    // + slot inside a batch
#define KEY_COLOR_BASE 300   // + slot inside a batch (RGB int, -1 = none)
#define KEY_FLAGS_BASE 400   // + slot inside a batch
#define KEY_ID_BASE 500      // + slot inside a batch (id hash, 0 = unknown)
#define ITEM_BATCH_SIZE 6    // must match BATCH_SIZE in index.js

#define FLAG_CONFIRM 0x01

#define PERSIST_HEADER_COLOR 1
#define PERSIST_HIGHLIGHT_COLOR 2
#define PERSIST_DARK_LIST 3
#define PERSIST_TOUCH_ENABLED 4
#define PERSIST_AUTO_CLOSE 5
#define PERSIST_SOUND_FEEDBACK 6
#define PERSIST_LIST_FORMAT 7
#define PERSIST_LIST_COUNT 8
#define PERSIST_ITEM_BASE 100       // + index, one key per webhook

#define LIST_FORMAT 1               // bump when the Webhook struct changes
#define MAX_PERSISTED_WEBHOOKS 32   // an app has 4 KB of persistent storage

#define TITLE_LEN 32
#define DESC_LEN 40

// Third-party apps use the platform's default text size: "Large" on the big displays
// (Pebble Time 2, Pebble Round 2) with 24px subtitles, "Medium" everywhere else (18px subtitles).
#if PBL_DISPLAY_WIDTH >= 200
#define CELL_HEIGHT_WITH_DESC 60
#define CELL_HEIGHT_TITLE_ONLY 38
#else
#define CELL_HEIGHT_WITH_DESC 52
#define CELL_HEIGHT_TITLE_ONLY 34
#endif

#define DEFAULT_HEADER_RGB 0x0055AA     // Cobalt Blue
#define DEFAULT_HIGHLIGHT_RGB 0x00FFFF  // Electric Blue

#define POPUP_DURATION_MS 1500
#define AUTO_CLOSE_DELAY_MS 5000
#define RESPONSE_TIMEOUT_MS 20000       // the phone gives up on a webhook after 10-12 s

// Popup backgrounds. Black on black & white watches: dithered gray would make white text unreadable.
#define POPUP_SUCCESS_COLOR PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack)
#define POPUP_ERROR_COLOR PBL_IF_COLOR_ELSE(GColorRed, GColorBlack)
#define POPUP_WARNING_COLOR PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack)
#define POPUP_NEUTRAL_COLOR PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)

typedef struct {
  char title[TITLE_LEN];
  char desc[DESC_LEN];
  int32_t color;   // RGB value, -1 = no button color
  int32_t id;      // hash of the webhook id on the phone, 0 = unknown
  uint8_t flags;
} Webhook;

static Window *s_main_window;
static MenuLayer *s_menu_layer;
static Layer *s_header_layer;

// Webhook list (allocated dynamically, size is given by the phone)
static Webhook *s_webhooks = NULL;
static uint16_t s_webhook_count = 0;
static bool s_list_known = false;   // false until the list was loaded from storage or the phone

static bool s_auto_close_enabled = false;
static bool s_sound_feedback_enabled = false;

// Theme
static int32_t s_header_rgb = DEFAULT_HEADER_RGB;
static int32_t s_highlight_rgb = DEFAULT_HIGHLIGHT_RGB;
static bool s_dark_list = false;

// Touch controls (only has an effect on watches with a touchscreen)
static bool s_touch_enabled = true;

static char s_time_text[8] = "00:00";

// Requests that were sent to the phone and have no result yet
static uint8_t s_pending = 0;
static AppTimer *s_response_timer = NULL;
static AppTimer *s_auto_close_timer = NULL;

// transient popup
static Window *s_popup_window = NULL;
static TextLayer *s_popup_text = NULL;
static AppTimer *s_popup_timer = NULL;

// confirmation window
static Window *s_confirm_window = NULL;
static TextLayer *s_confirm_title = NULL;
static TextLayer *s_confirm_hint = NULL;
static uint16_t s_confirm_number = 0;
static int32_t s_confirm_id = 0;
static char s_confirm_buf[TITLE_LEN + 16];
#if defined(PBL_TOUCH)
static Layer *s_confirm_buttons = NULL;
#endif

// ---------------------------------------------------------------- storage helpers

// Only write when the value changed: settings are re-sent on every app start
static void persist_int_if_changed(uint32_t key, int32_t value) {
  if (!persist_exists(key) || persist_read_int(key) != value) {
    persist_write_int(key, value);
  }
}

static void persist_bool_if_changed(uint32_t key, bool value) {
  if (!persist_exists(key) || persist_read_bool(key) != value) {
    persist_write_bool(key, value);
  }
}

// ---------------------------------------------------------------- theme

#if defined(PBL_COLOR)
// Black or white, whichever is easier to read on top of the given color
static GColor legible_over(GColor c) {
  int luma = c.r * 299 + c.g * 587 + c.b * 114;   // 0..3000
  return luma > 1400 ? GColorBlack : GColorWhite;
}
#endif

static GColor theme_bg(void) { return s_dark_list ? GColorBlack : GColorWhite; }
static GColor theme_fg(void) { return s_dark_list ? GColorWhite : GColorBlack; }

static GColor theme_hl_bg(void) {
#if defined(PBL_COLOR)
  return GColorFromHEX(s_highlight_rgb);
#else
  return s_dark_list ? GColorWhite : GColorBlack;
#endif
}

static GColor theme_hl_fg(void) {
#if defined(PBL_COLOR)
  return legible_over(GColorFromHEX(s_highlight_rgb));
#else
  return s_dark_list ? GColorBlack : GColorWhite;
#endif
}

static GColor theme_header_bg(void) {
#if defined(PBL_COLOR)
  return GColorFromHEX(s_header_rgb);
#else
  return GColorBlack;
#endif
}

static GColor theme_header_fg(void) {
#if defined(PBL_COLOR)
  return legible_over(GColorFromHEX(s_header_rgb));
#else
  return GColorWhite;
#endif
}

static void apply_theme(void) {
  // The window background is what shows below the last row when the list is short
  if (s_main_window) {
    window_set_background_color(s_main_window, theme_bg());
  }
  if (s_menu_layer) {
    menu_layer_set_normal_colors(s_menu_layer, theme_bg(), theme_fg());
    menu_layer_set_highlight_colors(s_menu_layer, theme_hl_bg(), theme_hl_fg());
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
  if (s_header_layer) {
    layer_mark_dirty(s_header_layer);
  }
}

static void load_settings(void) {
  if (persist_exists(PERSIST_HEADER_COLOR)) s_header_rgb = persist_read_int(PERSIST_HEADER_COLOR);
  if (persist_exists(PERSIST_HIGHLIGHT_COLOR)) s_highlight_rgb = persist_read_int(PERSIST_HIGHLIGHT_COLOR);
  if (persist_exists(PERSIST_DARK_LIST)) s_dark_list = persist_read_bool(PERSIST_DARK_LIST);
  if (persist_exists(PERSIST_TOUCH_ENABLED)) s_touch_enabled = persist_read_bool(PERSIST_TOUCH_ENABLED);
  if (persist_exists(PERSIST_AUTO_CLOSE)) s_auto_close_enabled = persist_read_bool(PERSIST_AUTO_CLOSE);
  if (persist_exists(PERSIST_SOUND_FEEDBACK)) s_sound_feedback_enabled = persist_read_bool(PERSIST_SOUND_FEEDBACK);
}

static void save_theme(void) {
  persist_int_if_changed(PERSIST_HEADER_COLOR, s_header_rgb);
  persist_int_if_changed(PERSIST_HIGHLIGHT_COLOR, s_highlight_rgb);
  persist_bool_if_changed(PERSIST_DARK_LIST, s_dark_list);
  persist_bool_if_changed(PERSIST_TOUCH_ENABLED, s_touch_enabled);
}

// Touch navigation: the system scrolls the MenuLayer and activates rows by touch.
// Third-party apps are opted out by default, so we opt in (or back out) here.
static void apply_touch_setting(void) {
#if defined(PBL_TOUCH)
  app_touch_navigation_enable(s_touch_enabled);
#endif
}

#if defined(PBL_TOUCH)
// True when touch is switched on in Hooky AND available on this watch
static bool touch_ui_active(void) {
  return s_touch_enabled && touch_service_is_enabled();
}
#endif

// ---------------------------------------------------------------- sound (watches with a speaker)

#if defined(PBL_SPEAKER)
static const SpeakerNote s_success_notes[] = {
  { .midi_note = 60, .waveform = SpeakerWaveformSine,     .duration_ms = 200 }, // C4
  { .midi_note = 64, .waveform = SpeakerWaveformSine,     .duration_ms = 200 }, // E4
  { .midi_note = 67, .waveform = SpeakerWaveformSine,     .duration_ms = 200 }, // G4
  { .midi_note = 72, .waveform = SpeakerWaveformTriangle, .duration_ms = 400 }, // C5
};

static AppTimer *s_sound_timer = NULL;

static void play_rejected_part2_cb(void *data) {
  s_sound_timer = NULL;
  (void)speaker_play_tone(450, 400, 80, SpeakerWaveformSquare);
}
#endif

static void play_feedback_sound(bool success) {
#if defined(PBL_SPEAKER)
  if (!s_sound_feedback_enabled) return;
  if (s_sound_timer) {
    app_timer_cancel(s_sound_timer);
    s_sound_timer = NULL;
  }
  if (success) {
    (void)speaker_play_notes(s_success_notes, ARRAY_LENGTH(s_success_notes), 80);
  } else {
    (void)speaker_play_tone(600, 150, 80, SpeakerWaveformSquare);
    s_sound_timer = app_timer_register(170, play_rejected_part2_cb, NULL);
  }
#else
  (void)success;
#endif
}

// ---------------------------------------------------------------- timers and popups

static void auto_close_timer_cb(void *data) {
  s_auto_close_timer = NULL;
  window_stack_pop_all(true);
}

static void cancel_auto_close(void) {
  if (s_auto_close_timer) {
    app_timer_cancel(s_auto_close_timer);
    s_auto_close_timer = NULL;
  }
}

static void schedule_auto_close(void) {
  cancel_auto_close();
  s_auto_close_timer = app_timer_register(AUTO_CLOSE_DELAY_MS, auto_close_timer_cb, NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  strftime(s_time_text, sizeof(s_time_text), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  if (s_header_layer) {
    layer_mark_dirty(s_header_layer);
  }
}

static void destroy_popup(bool animated) {
  if (s_popup_timer) {
    app_timer_cancel(s_popup_timer);
    s_popup_timer = NULL;
  }
  if (s_popup_window) {
    window_stack_remove(s_popup_window, animated);
    if (s_popup_text) {
      text_layer_destroy(s_popup_text);
      s_popup_text = NULL;
    }
    window_destroy(s_popup_window);
    s_popup_window = NULL;
  }
}

static void popup_timer_cb(void *data) {
  s_popup_timer = NULL;
  destroy_popup(true);
}

static void show_transient_popup(const char *message, GColor bg_color) {
  destroy_popup(false);

  s_popup_window = window_create();
  window_set_background_color(s_popup_window, bg_color);

  Layer *root = window_get_root_layer(s_popup_window);
  GRect bounds = layer_get_bounds(root);

  s_popup_text = text_layer_create(GRect(0, (bounds.size.h - 28) / 2, bounds.size.w, 28));
  text_layer_set_text_alignment(s_popup_text, GTextAlignmentCenter);
  text_layer_set_text_color(s_popup_text, GColorWhite);
  text_layer_set_background_color(s_popup_text, GColorClear);
  text_layer_set_font(s_popup_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_popup_text, message);

  layer_add_child(root, text_layer_get_layer(s_popup_text));
  window_stack_push(s_popup_window, true);

  s_popup_timer = app_timer_register(POPUP_DURATION_MS, popup_timer_cb, NULL);
}

static void show_result(const char *message, GColor bg_color, bool success) {
  play_feedback_sound(success);
  show_transient_popup(message, bg_color);
}

// ---------------------------------------------------------------- trigger

static void set_pending(uint8_t pending) {
  s_pending = pending;
  if (s_header_layer) {
    layer_mark_dirty(s_header_layer);
  }
}

// A request got its result (or failed): stop waiting for it
static void request_finished(void) {
  if (s_pending > 0) {
    set_pending(s_pending - 1);
  }
  if (s_pending == 0 && s_response_timer) {
    app_timer_cancel(s_response_timer);
    s_response_timer = NULL;
  }
}

// The phone never answered, e.g. because the app on the phone was closed in the meantime
static void response_timeout_cb(void *data) {
  s_response_timer = NULL;
  set_pending(0);
  show_result("No response", POPUP_WARNING_COLOR, false);
}

// webhook_number is the 1-based position in the list (the phone uses the same order),
// webhook_id identifies the webhook in case the list on the phone changed in the meantime
static void send_trigger(uint16_t webhook_number, int32_t webhook_id) {
  if (!connection_service_peek_pebble_app_connection()) {
    vibes_double_pulse();
    show_result("No phone", POPUP_NEUTRAL_COLOR, false);
    return;
  }

  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    // Usually the previous request is still being delivered
    vibes_double_pulse();
    show_result("Busy", POPUP_NEUTRAL_COLOR, false);
    return;
  }
  dict_write_uint16(iter, KEY_TRIGGER, webhook_number);
  if (webhook_id != 0) {
    dict_write_int32(iter, KEY_TRIGGER_ID, webhook_id);
  }
  if (app_message_outbox_send() != APP_MSG_OK) {
    vibes_double_pulse();
    show_result("Not sent", POPUP_ERROR_COLOR, false);
    return;
  }
  vibes_short_pulse();

  // Don't close the app while a newer request is running
  cancel_auto_close();
  if (s_pending < UINT8_MAX) {
    set_pending(s_pending + 1);
  }
  if (s_response_timer) {
    app_timer_reschedule(s_response_timer, RESPONSE_TIMEOUT_MS);
  } else {
    s_response_timer = app_timer_register(RESPONSE_TIMEOUT_MS, response_timeout_cb, NULL);
  }
}

// ---------------------------------------------------------------- confirmation window

static void confirm_run(void) {
  uint16_t number = s_confirm_number;
  if (number == 0) return;  // already confirmed or cancelled (guards against a double press)
  s_confirm_number = 0;
  window_stack_pop(true);   // the unload handler cleans the window up
  send_trigger(number, s_confirm_id);
}

static void confirm_select_handler(ClickRecognizerRef recognizer, void *context) {
  confirm_run();
}

static void confirm_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, confirm_select_handler);
  // BACK keeps its default behaviour: close the window without running the webhook
}

#if defined(PBL_TOUCH)
static void confirm_cancel(void) {
  if (s_confirm_number == 0) return;
  s_confirm_number = 0;
  window_stack_pop(true);
}

// Two touch buttons at the bottom of the confirmation window: Cancel (left) and Run (right).
// On round displays they are kept inside the circle.
static GRect confirm_button_rect(GRect bounds, bool run) {
  int16_t margin_x = PBL_IF_ROUND_ELSE(bounds.size.w * 17 / 100, 6);
  int16_t margin_bottom = PBL_IF_ROUND_ELSE(bounds.size.h / 7, 6);
  int16_t gap = 6;
  int16_t w = (bounds.size.w - 2 * margin_x - gap) / 2;
  return GRect(run ? margin_x + w + gap : margin_x, bounds.size.h - 44 - margin_bottom, w, 44);
}

static void confirm_buttons_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect cancel = confirm_button_rect(bounds, false);
  GRect run = confirm_button_rect(bounds, true);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

#if defined(PBL_COLOR)
  GColor cancel_bg = GColorDarkGray;
  GColor run_bg = GColorIslamicGreen;
  GColor text_color = GColorWhite;
#else
  GColor cancel_bg = GColorWhite;
  GColor run_bg = GColorWhite;
  GColor text_color = GColorBlack;
#endif

  graphics_context_set_fill_color(ctx, cancel_bg);
  graphics_fill_rect(ctx, cancel, 8, GCornersAll);
  graphics_context_set_fill_color(ctx, run_bg);
  graphics_fill_rect(ctx, run, 8, GCornersAll);

  graphics_context_set_text_color(ctx, text_color);
  graphics_draw_text(ctx, "Cancel", font, GRect(cancel.origin.x, cancel.origin.y + 6, cancel.size.w, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "Run", font, GRect(run.origin.x, run.origin.y + 6, run.size.w, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void confirm_tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event != RecognizerEvent_Completed || !s_confirm_window) return;

  GPoint p = tap_recognizer_get_tap_point(recognizer);
  GRect bounds = layer_get_bounds(window_get_root_layer(s_confirm_window));
  GRect cancel = confirm_button_rect(bounds, false);
  GRect run = confirm_button_rect(bounds, true);

  if (grect_contains_point(&run, &p)) {
    confirm_run();
  } else if (grect_contains_point(&cancel, &p)) {
    confirm_cancel();
  }
}
#endif

static void confirm_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  bool touch_ui = false;
#if defined(PBL_TOUCH)
  touch_ui = touch_ui_active();
#endif

  // The question fills the space above the buttons (touch) or the button hint
  int16_t hint_h = 44;
  int16_t hint_y = bounds.size.h - PBL_IF_ROUND_ELSE(bounds.size.h / 10, 6) - hint_h;
  int16_t bottom = hint_y;
#if defined(PBL_TOUCH)
  if (touch_ui) {
    bottom = confirm_button_rect(bounds, false).origin.y;
  }
#endif
  int16_t inset_x = PBL_IF_ROUND_ELSE(bounds.size.w / 8, 12);
  int16_t top = PBL_IF_ROUND_ELSE(bounds.size.h / 6, 14);

  s_confirm_title = text_layer_create(GRect(inset_x, top, bounds.size.w - 2 * inset_x, bottom - top - 4));
  text_layer_set_text_alignment(s_confirm_title, GTextAlignmentCenter);
  text_layer_set_text_color(s_confirm_title, GColorWhite);
  text_layer_set_background_color(s_confirm_title, GColorClear);
  text_layer_set_font(s_confirm_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_overflow_mode(s_confirm_title, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_confirm_title, s_confirm_buf);
  layer_add_child(root, text_layer_get_layer(s_confirm_title));

  if (touch_ui) {
#if defined(PBL_TOUCH)
    s_confirm_buttons = layer_create(bounds);
    layer_set_update_proc(s_confirm_buttons, confirm_buttons_update_proc);
    layer_add_child(root, s_confirm_buttons);

    // This window handles its touches itself instead of the system's touch-to-button bridge.
    // The buttons (SELECT = run, BACK = cancel) keep working as well.
    // The window owns the recognizer and destroys it together with the window.
    window_set_touch_bridge_disabled(window, true);
    window_attach_recognizer(window, tap_recognizer_create(confirm_tap_handler, NULL));
#endif
  } else {
    s_confirm_hint = text_layer_create(GRect(0, hint_y, bounds.size.w, hint_h));
    text_layer_set_text_alignment(s_confirm_hint, GTextAlignmentCenter);
    text_layer_set_text_color(s_confirm_hint, GColorWhite);
    text_layer_set_background_color(s_confirm_hint, GColorClear);
    text_layer_set_font(s_confirm_hint, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text(s_confirm_hint, "SELECT: run\nBACK: cancel");
    layer_add_child(root, text_layer_get_layer(s_confirm_hint));
  }
}

static void confirm_window_unload(Window *window) {
  if (s_confirm_title) {
    text_layer_destroy(s_confirm_title);
    s_confirm_title = NULL;
  }
  if (s_confirm_hint) {
    text_layer_destroy(s_confirm_hint);
    s_confirm_hint = NULL;
  }
#if defined(PBL_TOUCH)
  if (s_confirm_buttons) {
    layer_destroy(s_confirm_buttons);
    s_confirm_buttons = NULL;
  }
#endif
  s_confirm_number = 0;
  window_destroy(window);
  s_confirm_window = NULL;
}

static void open_confirm_window(uint16_t webhook_number, int32_t webhook_id, const char *name) {
  if (s_confirm_window) return;

  s_confirm_number = webhook_number;
  s_confirm_id = webhook_id;
  snprintf(s_confirm_buf, sizeof(s_confirm_buf), "Run \"%s\"?", name);

  s_confirm_window = window_create();
#if defined(PBL_COLOR)
  window_set_background_color(s_confirm_window, GColorOxfordBlue);
#else
  window_set_background_color(s_confirm_window, GColorBlack);
#endif
  window_set_click_config_provider(s_confirm_window, confirm_click_config_provider);
  window_set_window_handlers(s_confirm_window, (WindowHandlers){
    .load = confirm_window_load,
    .unload = confirm_window_unload
  });
  window_stack_push(s_confirm_window, true);
}

// ---------------------------------------------------------------- list

static void free_webhooks(void) {
  if (s_webhooks) {
    free(s_webhooks);
    s_webhooks = NULL;
  }
  s_webhook_count = 0;
}

// Allocates an empty list. If memory is short (e.g. on Aplite), *count is reduced to what fits.
static Webhook *alloc_webhooks(int32_t *count) {
  Webhook *list = NULL;
  while (*count > 0) {
    list = (Webhook *)malloc(sizeof(Webhook) * (size_t)*count);
    if (list) break;
    *count /= 2;
  }
  if (!list) {
    *count = 0;
    return NULL;
  }
  memset(list, 0, sizeof(Webhook) * (size_t)*count);
  for (int32_t i = 0; i < *count; i++) {
    list[i].color = -1;
  }
  return list;
}

// Resizes the list for a new sync. Existing entries are kept until the phone sends their
// replacement, so the list does not flicker; each entry still triggers by its own id.
static void resize_webhooks(int32_t count) {
  if (count < 0) count = 0;
  if (count > 65000) count = 65000;

  int32_t wanted = count;
  Webhook *list = alloc_webhooks(&count);
  if (count < wanted && s_webhooks) {
    // Not enough memory for both lists: drop the old one first
    free(list);
    free_webhooks();
    count = wanted;
    list = alloc_webhooks(&count);
  }
  if (s_webhooks && list) {
    uint16_t keep = s_webhook_count < count ? s_webhook_count : (uint16_t)count;
    memcpy(list, s_webhooks, sizeof(Webhook) * keep);
  }
  free_webhooks();
  s_webhooks = list;
  s_webhook_count = (uint16_t)count;
}

// ---- offline copy of the list, so it shows up instantly and without a phone

static void persist_list_count(uint16_t count) {
  persist_int_if_changed(PERSIST_LIST_FORMAT, LIST_FORMAT);
  persist_int_if_changed(PERSIST_LIST_COUNT, count);
  for (uint16_t i = count; i < MAX_PERSISTED_WEBHOOKS; i++) {
    if (persist_exists(PERSIST_ITEM_BASE + i)) {
      persist_delete(PERSIST_ITEM_BASE + i);
    }
  }
}

static void persist_webhook(uint16_t idx) {
  if (idx >= MAX_PERSISTED_WEBHOOKS || idx >= s_webhook_count) return;
  uint32_t key = PERSIST_ITEM_BASE + idx;
  Webhook stored;
  if (persist_read_data(key, &stored, sizeof(stored)) == (int)sizeof(stored) &&
      memcmp(&stored, &s_webhooks[idx], sizeof(Webhook)) == 0) {
    return;   // unchanged: spare the flash
  }
  persist_write_data(key, &s_webhooks[idx], sizeof(Webhook));
}

static void load_webhooks(void) {
  if (!persist_exists(PERSIST_LIST_COUNT) || persist_read_int(PERSIST_LIST_FORMAT) != LIST_FORMAT) {
    return;
  }
  s_list_known = true;

  int32_t count = persist_read_int(PERSIST_LIST_COUNT);
  if (count > MAX_PERSISTED_WEBHOOKS) count = MAX_PERSISTED_WEBHOOKS;
  Webhook *list = alloc_webhooks(&count);
  if (!list) return;

  int32_t loaded = 0;
  while (loaded < count &&
         persist_read_data(PERSIST_ITEM_BASE + loaded, &list[loaded], sizeof(Webhook)) == (int)sizeof(Webhook)) {
    list[loaded].title[TITLE_LEN - 1] = '\0';
    list[loaded].desc[DESC_LEN - 1] = '\0';
    loaded++;
  }
  if (loaded == 0) {
    free(list);
    return;
  }
  s_webhooks = list;
  s_webhook_count = (uint16_t)loaded;
}

// ---- menu

static void update_menu_data(void) {
  if (!s_menu_layer) return;
  MenuIndex selected = menu_layer_get_selected_index(s_menu_layer);
  menu_layer_reload_data(s_menu_layer);
  uint16_t rows = s_webhook_count > 0 ? s_webhook_count : 1;
  if (selected.row >= rows) {
    menu_layer_set_selected_index(s_menu_layer, MenuIndex(0, rows - 1), MenuRowAlignCenter, false);
  }
}

static void header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const char *title = s_pending > 0 ? "Sending..." : "Hooky";

  graphics_context_set_fill_color(ctx, theme_header_bg());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, theme_header_fg());

  #if defined(PBL_ROUND)
    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 4, bounds.size.w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, s_time_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(0, 20, bounds.size.w, 24), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  #else
    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(6, 0, bounds.size.w - 55, 24), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_time_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(bounds.size.w - 52, 0, 46, 24), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  #endif
}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // One placeholder row when the list is empty
  return s_webhook_count > 0 ? s_webhook_count : 1;
}

static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (s_webhook_count == 0 || cell_index->row >= s_webhook_count) {
    return CELL_HEIGHT_WITH_DESC;
  }
  // No description -> slimmer button
  return s_webhooks[cell_index->row].desc[0] ? CELL_HEIGHT_WITH_DESC : CELL_HEIGHT_TITLE_ONLY;
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  GRect bounds = layer_get_bounds(cell_layer);
  bool selected = menu_cell_layer_is_highlighted(cell_layer);
  bool is_item = s_webhook_count > 0 && cell_index->row < s_webhook_count;
  const Webhook *w = is_item ? &s_webhooks[cell_index->row] : NULL;

  GColor bg = selected ? theme_hl_bg() : theme_bg();
  GColor fg = selected ? theme_hl_fg() : theme_fg();

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

#if defined(PBL_COLOR)
  // Per-webhook button color: a rounded "button" behind the text (not while selected)
  if (!selected && w && w->color >= 0) {
    GColor tint = GColorFromHEX(w->color);
    graphics_context_set_fill_color(ctx, tint);
    graphics_fill_rect(ctx, GRect(2, 1, bounds.size.w - 4, bounds.size.h - 2), 6, GCornersAll);
    fg = legible_over(tint);
  }
#endif

  graphics_context_set_text_color(ctx, fg);

  if (!w) {
    if (s_list_known) {
      menu_cell_basic_draw(ctx, cell_layer, "No webhooks", "Add one in the settings", NULL);
    } else {
      menu_cell_basic_draw(ctx, cell_layer, "Hooky", "Waiting for phone", NULL);
    }
    return;
  }
  // An empty title means the entry is still on its way from the phone
  menu_cell_basic_draw(ctx, cell_layer, w->title[0] ? w->title : "...", w->desc[0] ? w->desc : NULL, NULL);
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (s_webhook_count == 0 || cell_index->row >= s_webhook_count) {
    return;
  }
  const Webhook *w = &s_webhooks[cell_index->row];
  if (w->flags & FLAG_CONFIRM) {
    open_confirm_window(cell_index->row + 1, w->id, w->title);
  } else {
    send_trigger(cell_index->row + 1, w->id);
  }
}

// ---------------------------------------------------------------- messages

// Integers can arrive with 1, 2 or 4 bytes depending on the phone app
static int32_t tuple_to_int32(const Tuple *t) {
  if (!t) return 0;
  switch (t->type) {
    case TUPLE_CSTRING:
      if (t->length > 0) return atoi(t->value->cstring);
      return 0;
    case TUPLE_INT:
      if (t->length == 1) return t->value->int8;
      if (t->length == 2) return t->value->int16;
      if (t->length >= 4) return t->value->int32;
      return 0;
    case TUPLE_UINT:
      if (t->length == 1) return t->value->uint8;
      if (t->length == 2) return t->value->uint16;
      if (t->length >= 4) return (int32_t)t->value->uint32;
      return 0;
    default:
      return 0;
  }
}

static void copy_tuple_string(char *dst, size_t dst_size, const Tuple *t) {
  dst[0] = '\0';
  if (t && t->type == TUPLE_CSTRING && t->length > 0) {
    strncpy(dst, t->value->cstring, dst_size - 1);
    dst[dst_size - 1] = '\0';
  }
}

static void handle_status(int32_t status) {
  static char buf[24];

  APP_LOG(APP_LOG_LEVEL_INFO, "Webhook status %ld received", (long)status);
  request_finished();

  switch (status) {
    case 200:
      show_result("Success", POPUP_SUCCESS_COLOR, true);
      if (s_auto_close_enabled && s_pending == 0) {
        schedule_auto_close();
      }
      break;
    case 0:
      show_result("Error", POPUP_ERROR_COLOR, false);
      break;
    case -1:
      show_result("Timeout", POPUP_WARNING_COLOR, false);
      break;
    case -2:
      show_result("Not found", POPUP_NEUTRAL_COLOR, false);
      break;
    default:
      snprintf(buf, sizeof(buf), "Status %ld", (long)status);
      show_result(buf, POPUP_ERROR_COLOR, false);
      break;
  }
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *t_autoclose = dict_find(iter, KEY_AUTO_CLOSE);
  if (t_autoclose) {
    s_auto_close_enabled = tuple_to_int32(t_autoclose) > 0;
    persist_bool_if_changed(PERSIST_AUTO_CLOSE, s_auto_close_enabled);
    if (!s_auto_close_enabled) cancel_auto_close();
  }

  Tuple *t_sound = dict_find(iter, KEY_SOUND_FEEDBACK);
  if (t_sound) {
    s_sound_feedback_enabled = tuple_to_int32(t_sound) > 0;
    persist_bool_if_changed(PERSIST_SOUND_FEEDBACK, s_sound_feedback_enabled);
  }

  // Start of a sync: theme + size of the list
  Tuple *t_update = dict_find(iter, KEY_UPDATE);
  if (t_update) {
    Tuple *t_header = dict_find(iter, KEY_HEADER_COLOR);
    Tuple *t_highlight = dict_find(iter, KEY_HIGHLIGHT_COLOR);
    Tuple *t_dark = dict_find(iter, KEY_DARK_LIST);
    Tuple *t_touch = dict_find(iter, KEY_TOUCH);
    if (t_header) s_header_rgb = tuple_to_int32(t_header);
    if (t_highlight) s_highlight_rgb = tuple_to_int32(t_highlight);
    if (t_dark) s_dark_list = tuple_to_int32(t_dark) > 0;
    if (t_touch) s_touch_enabled = tuple_to_int32(t_touch) > 0;
    save_theme();
    apply_theme();
    apply_touch_setting();

    resize_webhooks(tuple_to_int32(dict_find(iter, KEY_COUNT)));
    s_list_known = true;
    persist_list_count(s_webhook_count);
    APP_LOG(APP_LOG_LEVEL_INFO, "Sync started, %d webhooks", (int)s_webhook_count);

    update_menu_data();
    return;
  }

  // Batch of webhooks (name, description, color, flags, id)
  Tuple *t_start = dict_find(iter, KEY_ITEM_START);
  if (t_start) {
    int32_t start = tuple_to_int32(t_start);
    for (int slot = 0; slot < ITEM_BATCH_SIZE; slot++) {
      int32_t idx = start + slot;
      if (!s_webhooks || idx < 0 || idx >= s_webhook_count) break;

      Tuple *tname = dict_find(iter, KEY_NAME_BASE + slot);
      if (!tname) continue;
      Tuple *tdesc = dict_find(iter, KEY_DESC_BASE + slot);
      Tuple *tcolor = dict_find(iter, KEY_COLOR_BASE + slot);
      Tuple *tflags = dict_find(iter, KEY_FLAGS_BASE + slot);
      Tuple *tid = dict_find(iter, KEY_ID_BASE + slot);

      Webhook item;
      memset(&item, 0, sizeof(item));
      copy_tuple_string(item.title, TITLE_LEN, tname);
      if (item.title[0] == '\0') {
        snprintf(item.title, TITLE_LEN, "Webhook %d", (int)(idx + 1));
      }
      copy_tuple_string(item.desc, DESC_LEN, tdesc);
      item.color = tcolor ? tuple_to_int32(tcolor) : -1;
      item.flags = tflags ? (uint8_t)tuple_to_int32(tflags) : 0;
      item.id = tid ? tuple_to_int32(tid) : 0;

      memcpy(&s_webhooks[idx], &item, sizeof(Webhook));
      persist_webhook((uint16_t)idx);
    }
    update_menu_data();
    return;
  }

  Tuple *tstatus = dict_find(iter, KEY_STATUS);
  if (tstatus) {
    handle_status(tuple_to_int32(tstatus));
    return;
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message from phone dropped: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Trigger not delivered: %d", (int)reason);
  request_finished();
  show_result("Not sent", POPUP_ERROR_COLOR, false);
}

// ---------------------------------------------------------------- main window

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int16_t header_height = PBL_IF_ROUND_ELSE(46, 24);

  #if defined(PBL_ROUND)
    GRect menu_bounds = bounds;
  #else
    GRect menu_bounds = GRect(0, header_height, bounds.size.w, bounds.size.h - header_height);
  #endif

  s_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
  });

  menu_layer_set_click_config_onto_window(s_menu_layer, window);

  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_header_layer = layer_create(GRect(0, 0, bounds.size.w, header_height));
  layer_set_update_proc(s_header_layer, header_update_proc);

  layer_add_child(window_layer, s_header_layer);

  apply_theme();
  update_menu_data();

  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  tick_handler(tick_time, MINUTE_UNIT);
}

static void main_window_unload(Window *window) {
  if (s_menu_layer) {
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  if (s_header_layer) {
    layer_destroy(s_header_layer);
    s_header_layer = NULL;
  }
}

static void init(void) {
  load_settings();
  load_webhooks();
  apply_touch_setting();

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  // Inbox: a batch of 6 webhooks is ~750 bytes. Outbox: a trigger is ~25 bytes.
  app_message_open(1024, 64);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  s_main_window = window_create();
  window_set_background_color(s_main_window, theme_bg());
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_main_window);
  free_webhooks();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
