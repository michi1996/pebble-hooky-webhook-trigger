#include <pebble.h>

#define KEY_TRIGGER 0
#define KEY_UPDATE 1
#define KEY_STATUS 2
#define KEY_COUNT 3
#define KEY_ITEM_START 4
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
#define ITEM_BATCH_SIZE 6    // must match BATCH_SIZE in index.js

#define FLAG_CONFIRM 0x01

#define PERSIST_HEADER_COLOR 1
#define PERSIST_HIGHLIGHT_COLOR 2
#define PERSIST_DARK_LIST 3
#define PERSIST_TOUCH_ENABLED 4

#define TITLE_LEN 32
#define DESC_LEN 40

#define CELL_HEIGHT_WITH_DESC 52
#define CELL_HEIGHT_TITLE_ONLY 34

#define DEFAULT_HEADER_RGB 0x0055AA     // Cobalt Blue
#define DEFAULT_HIGHLIGHT_RGB 0x00FFFF  // Electric Blue

typedef struct {
  char title[TITLE_LEN];
  char desc[DESC_LEN];
  int32_t color;   // RGB value, -1 = no button color
  uint8_t flags;
} Webhook;

static Window *s_main_window;
static MenuLayer *s_menu_layer;
static Layer *s_header_layer; 

// Webhook list (allocated dynamically, size is given by the phone)
static Webhook *s_webhooks = NULL;
static uint16_t s_webhook_count = 0;

static bool s_auto_close_enabled = false; 
static bool s_sound_feedback_enabled = false;

// Theme
static int32_t s_header_rgb = DEFAULT_HEADER_RGB;
static int32_t s_highlight_rgb = DEFAULT_HIGHLIGHT_RGB;
static bool s_dark_list = false;

// Touch controls (only has an effect on watches with a touchscreen)
static bool s_touch_enabled = true;

static char s_time_text[8] = "00:00";

// transient popup
static Window *s_popup_window = NULL;
static TextLayer *s_popup_text = NULL;
static AppTimer *s_popup_timer = NULL;

// confirmation window
static Window *s_confirm_window = NULL;
static TextLayer *s_confirm_title = NULL;
static TextLayer *s_confirm_hint = NULL;
static uint16_t s_confirm_number = 0;
static char s_confirm_buf[TITLE_LEN + 16];
#if defined(PBL_TOUCH)
static Layer *s_confirm_buttons = NULL;
#endif

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

static void load_theme(void) {
  if (persist_exists(PERSIST_HEADER_COLOR)) s_header_rgb = persist_read_int(PERSIST_HEADER_COLOR);
  if (persist_exists(PERSIST_HIGHLIGHT_COLOR)) s_highlight_rgb = persist_read_int(PERSIST_HIGHLIGHT_COLOR);
  if (persist_exists(PERSIST_DARK_LIST)) s_dark_list = persist_read_bool(PERSIST_DARK_LIST);
  if (persist_exists(PERSIST_TOUCH_ENABLED)) s_touch_enabled = persist_read_bool(PERSIST_TOUCH_ENABLED);
}

static void save_theme(void) {
  persist_write_int(PERSIST_HEADER_COLOR, s_header_rgb);
  persist_write_int(PERSIST_HIGHLIGHT_COLOR, s_highlight_rgb);
  persist_write_bool(PERSIST_DARK_LIST, s_dark_list);
  persist_write_bool(PERSIST_TOUCH_ENABLED, s_touch_enabled);
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

// ---------------------------------------------------------------- timers and popups

static void auto_close_timer_cb(void *data) {
  window_stack_pop_all(true);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  strftime(s_time_text, sizeof(s_time_text), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  if (s_header_layer) {
    layer_mark_dirty(s_header_layer);
  }
}

static void popup_timer_cb(void *data) {
  if (s_popup_window) {
    window_stack_remove(s_popup_window, true);
    if (s_popup_text) {
      text_layer_destroy(s_popup_text);
      s_popup_text = NULL;
    }
    window_destroy(s_popup_window);
    s_popup_window = NULL;
  }
  s_popup_timer = NULL;
}

static void show_transient_popup(const char *message, GColor bg_color) {
  if (s_popup_window) {
    if (s_popup_timer) {
      app_timer_cancel(s_popup_timer);
      s_popup_timer = NULL;
    }
    window_stack_remove(s_popup_window, false);
    if (s_popup_text) {
      text_layer_destroy(s_popup_text);
      s_popup_text = NULL;
    }
    window_destroy(s_popup_window);
    s_popup_window = NULL;
  }

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

  s_popup_timer = app_timer_register(1500, popup_timer_cb, NULL);
}

// ---------------------------------------------------------------- trigger

// webhook_number is the 1-based position in the list (the phone uses the same order)
static void send_trigger(uint16_t webhook_number) {
  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    vibes_short_pulse();
    return;
  }
  dict_write_uint16(iter, KEY_TRIGGER, webhook_number);
  app_message_outbox_send();
  vibes_short_pulse();
}

// ---------------------------------------------------------------- confirmation window

static void confirm_run(void) {
  uint16_t number = s_confirm_number;
  if (number == 0) return;  // already confirmed or cancelled (guards against a double press)
  s_confirm_number = 0;
  window_stack_pop(true);   // the unload handler cleans the window up
  send_trigger(number);
}

static void confirm_cancel(void) {
  s_confirm_number = 0;
  window_stack_pop(true);
}

static void confirm_select_handler(ClickRecognizerRef recognizer, void *context) {
  confirm_run();
}

static void confirm_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, confirm_select_handler);
  // BACK keeps its default behaviour: close the window without running the webhook
}

#if defined(PBL_TOUCH)
// Two touch buttons at the bottom of the confirmation window: Cancel (left) and Run (right)
static GRect confirm_button_rect(GRect bounds, bool run) {
  int16_t margin_x = PBL_IF_ROUND_ELSE(34, 6);
  int16_t margin_bottom = PBL_IF_ROUND_ELSE(30, 6);
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

  s_confirm_title = text_layer_create(GRect(12, PBL_IF_ROUND_ELSE(28, 14), bounds.size.w - 24,
                                            bounds.size.h - PBL_IF_ROUND_ELSE(104, 90)));
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
    window_set_touch_bridge_disabled(window, true);
    window_attach_recognizer(window, tap_recognizer_create(confirm_tap_handler, NULL));
#endif
  } else {
    s_confirm_hint = text_layer_create(GRect(0, bounds.size.h - PBL_IF_ROUND_ELSE(64, 50), bounds.size.w, 44));
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
  window_destroy(window);
  s_confirm_window = NULL;
}

static void open_confirm_window(uint16_t webhook_number, const char *name) {
  if (s_confirm_window) return;

  s_confirm_number = webhook_number;
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

static void update_menu_data(void) {
  if (s_menu_layer) {
    menu_layer_reload_data(s_menu_layer);
  }
}

static void header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  graphics_context_set_fill_color(ctx, theme_header_bg());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, theme_header_fg());

  #if defined(PBL_ROUND)
    graphics_draw_text(ctx, "Hooky", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 4, bounds.size.w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, s_time_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(0, 20, bounds.size.w, 24), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  #else
    graphics_draw_text(ctx, "Hooky", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
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
    menu_cell_basic_draw(ctx, cell_layer, "No webhooks", "Add one in the settings", NULL);
    return;
  }
  menu_cell_basic_draw(ctx, cell_layer, w->title, w->desc[0] ? w->desc : NULL, NULL);
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (s_webhook_count == 0 || cell_index->row >= s_webhook_count) {
    return;
  }
  const Webhook *w = &s_webhooks[cell_index->row];
  if (w->flags & FLAG_CONFIRM) {
    open_confirm_window(cell_index->row + 1, w->title);
  } else {
    send_trigger(cell_index->row + 1);
  }
}

// ---------------------------------------------------------------- messages

static int32_t tuple_to_int32(const Tuple *t) {
  if (!t) return 0;
  switch (t->type) {
    case TUPLE_CSTRING:
      if (t->length > 0) return atoi(t->value->cstring);
      return 0;
    case TUPLE_INT:
      return t->value->int32;
    case TUPLE_UINT:
      return (int32_t)t->value->uint32;
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

static const SpeakerNote __attribute__((unused)) s_arpeggio[] = {
  { .midi_note = 60, .waveform = SpeakerWaveformSine,     .duration_ms = 200 }, // C4
  { .midi_note = 64, .waveform = SpeakerWaveformSine,     .duration_ms = 200 }, // E4
  { .midi_note = 67, .waveform = SpeakerWaveformSine,     .duration_ms = 200 }, // G4
  { .midi_note = 72, .waveform = SpeakerWaveformTriangle, .duration_ms = 400 }, // C5
};

static void play_rejected_part2_cb(void *data) {
  (void)speaker_play_tone(450, 400, 80, SpeakerWaveformSquare);
}

static void play_rejected_sound() {
  (void)speaker_play_tone(600, 150, 80, SpeakerWaveformSquare);
  app_timer_register(170, play_rejected_part2_cb, NULL);
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *t_autoclose = dict_find(iter, KEY_AUTO_CLOSE);
  if (t_autoclose) {
    s_auto_close_enabled = tuple_to_int32(t_autoclose) > 0;
    APP_LOG(APP_LOG_LEVEL_INFO, "Config-Update: Auto-Close is now %s", s_auto_close_enabled ? "ON" : "OFF");
  }

  Tuple *t_sound = dict_find(iter, KEY_SOUND_FEEDBACK);
  if (t_sound) {
    s_sound_feedback_enabled = tuple_to_int32(t_sound) > 0;
    APP_LOG(APP_LOG_LEVEL_INFO, "Config-Update: Sound Feedback is now %s", s_sound_feedback_enabled ? "ON" : "OFF");
  }

  // Start of a sync: theme + allocate the list for the announced number of webhooks
  Tuple *t_update = dict_find(iter, KEY_UPDATE);
  if (t_update) {
    Tuple *t_header = dict_find(iter, KEY_HEADER_COLOR);
    Tuple *t_highlight = dict_find(iter, KEY_HIGHLIGHT_COLOR);
    Tuple *t_dark = dict_find(iter, KEY_DARK_LIST);
    if (t_header) s_header_rgb = tuple_to_int32(t_header);
    if (t_highlight) s_highlight_rgb = tuple_to_int32(t_highlight);
    if (t_dark) s_dark_list = tuple_to_int32(t_dark) > 0;
    Tuple *t_touch = dict_find(iter, KEY_TOUCH);
    if (t_touch) s_touch_enabled = tuple_to_int32(t_touch) > 0;
    save_theme();
    apply_theme();
    apply_touch_setting();

    free_webhooks();
    int32_t count = tuple_to_int32(dict_find(iter, KEY_COUNT));
    if (count < 0) count = 0;
    if (count > 65000) count = 65000;

    // If memory is short (e.g. on Aplite), keep as many entries as fit
    while (count > 0) {
      s_webhooks = (Webhook *)malloc(sizeof(Webhook) * (size_t)count);
      if (s_webhooks) break;
      count /= 2;
    }
    if (s_webhooks) {
      memset(s_webhooks, 0, sizeof(Webhook) * (size_t)count);
      for (int32_t i = 0; i < count; i++) {
        s_webhooks[i].color = -1;
      }
      s_webhook_count = (uint16_t)count;
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "Sync started, %d webhooks", (int)s_webhook_count);

    update_menu_data();
    if (s_menu_layer) {
      menu_layer_set_selected_index(s_menu_layer, MenuIndex(0, 0), MenuRowAlignTop, false);
    }
    return;
  }

  // Batch of webhooks (name, description, color, flags)
  Tuple *t_start = dict_find(iter, KEY_ITEM_START);
  if (t_start) {
    int32_t start = tuple_to_int32(t_start);
    for (int slot = 0; slot < ITEM_BATCH_SIZE; slot++) {
      int32_t idx = start + slot;
      if (!s_webhooks || idx < 0 || idx >= s_webhook_count) break;

      Tuple *tname = dict_find(iter, KEY_NAME_BASE + slot);
      Tuple *tdesc = dict_find(iter, KEY_DESC_BASE + slot);
      Tuple *tcolor = dict_find(iter, KEY_COLOR_BASE + slot);
      Tuple *tflags = dict_find(iter, KEY_FLAGS_BASE + slot);
      if (!tname) continue;

      copy_tuple_string(s_webhooks[idx].title, TITLE_LEN, tname);
      if (s_webhooks[idx].title[0] == '\0') {
        snprintf(s_webhooks[idx].title, TITLE_LEN, "Webhook %d", (int)(idx + 1));
      }
      copy_tuple_string(s_webhooks[idx].desc, DESC_LEN, tdesc);
      s_webhooks[idx].color = tcolor ? tuple_to_int32(tcolor) : -1;
      s_webhooks[idx].flags = tflags ? (uint8_t)tuple_to_int32(tflags) : 0;
    }
    update_menu_data();
    return;
  }

  Tuple *tstatus = dict_find(iter, KEY_STATUS);
  if (tstatus) {
    int32_t status = tuple_to_int32(tstatus);
    static char buf[64];
    GColor popup_color = GColorBlack;

    APP_LOG(APP_LOG_LEVEL_INFO, "Webhook Status %ld received.", (long)status);

    switch (status) {
      case 200:
        snprintf(buf, sizeof(buf), "Success");
        popup_color = GColorIslamicGreen;
        
        if (s_sound_feedback_enabled) {
          // Play the predefined C-major arpeggio sequence
          (void)speaker_play_notes(s_arpeggio, ARRAY_LENGTH(s_arpeggio), 80); 
        }

        if (s_auto_close_enabled) {
          APP_LOG(APP_LOG_LEVEL_INFO, "Start 5s Auto-Close Timer...");
          app_timer_register(5000, auto_close_timer_cb, NULL);
        }
        break;
      case 0:
        snprintf(buf, sizeof(buf), "Error");
        popup_color = GColorRed;
        if (s_sound_feedback_enabled) {
          play_rejected_sound(); 
        }
        break;
      case -1:
        snprintf(buf, sizeof(buf), "Timeout");
        popup_color = GColorOrange;
        if (s_sound_feedback_enabled) {
          play_rejected_sound(); 
        }
        break;
      case -2:
        snprintf(buf, sizeof(buf), "Not found");
        popup_color = GColorDarkGray; 
        if (s_sound_feedback_enabled) {
          play_rejected_sound(); 
        }
        break;
      default:
        snprintf(buf, sizeof(buf), "Status %ld", (long)status);
        popup_color = GColorRed;
        if (s_sound_feedback_enabled) {
          play_rejected_sound(); 
        }
        break;
    }

    show_transient_popup(buf, popup_color);
    return;
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  (void)reason;
  (void)context;
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
  load_theme();
  apply_touch_setting();

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_open(1024, 1024);

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
  window_destroy(s_main_window);
  free_webhooks();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}