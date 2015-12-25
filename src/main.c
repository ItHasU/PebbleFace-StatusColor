#include <pebble.h>

#define TEXT_SIZE 50
#define MARGIN 5

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_analog_layer;

static uint16_t min(uint16_t a, uint16_t b) {
  return a < b ? a : b;
}

static void update_background(struct Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  // Prepare background rect before modifying bounds
  GRect back = bounds;
  back.origin.y = bounds.size.h / 2 - MARGIN;
  back.size.h = TEXT_SIZE + 2 * MARGIN;

  // Connection status
  bool connected = connection_service_peek_pebble_app_connection();
  graphics_context_set_fill_color(ctx, connected ? GColorBlue : GColorRed);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Battery status
  BatteryChargeState charge = battery_state_service_peek();
  bounds.origin.y = bounds.size.h / 2;
  if (charge.is_charging) {
    graphics_context_set_fill_color(ctx, GColorGreen);
  } else {
    if (charge.charge_percent <= 10) {
      graphics_context_set_fill_color(ctx, GColorRed);
    } else if (charge.charge_percent <= 30) {
      graphics_context_set_fill_color(ctx, GColorOrange);
    } else {
      graphics_context_set_fill_color(ctx, GColorBlue);
    }
  }
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Text background's background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, back, 0, GCornerNone);
} 

static void update_analog(struct Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  // Prepare center, radius
  GPoint center = (GPoint) {
    .x = bounds.size.w / 4,
    .y = bounds.size.h / 4
  };
  uint16_t radius = min(bounds.size.w, bounds.size.h) / 4 - MARGIN;
  // Draw background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, radius);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, radius - MARGIN);
  
  // Draw lines
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[6];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void connection_handler(bool connected) {
  Layer * window_layer = window_get_root_layer(s_main_window);
  layer_mark_dirty(window_layer);
  
  static const uint32_t segments[] = { 100, 50, 100, 50, 100 };
  VibePattern pat = {
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  };
  vibes_enqueue_custom_pattern(pat);
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer * window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  layer_set_update_proc(window_layer, &update_background);
  
  // Create the TextLayer with specific bounds
  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h/2, bounds.size.w, TEXT_SIZE));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorWhite);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorDarkGray);
  
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Register with connect / disconnect
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = &connection_handler,
    .pebblekit_connection_handler = NULL
  });
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}