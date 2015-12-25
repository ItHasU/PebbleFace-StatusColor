#include <pebble.h>

#define SCREEN_WIDTH  144
#define SCREEN_HEIGHT 168

#define TEXT_SIZE_OUTER 57
#define TEXT_SIZE_INNER 50
#define TEXT_POSITION_OUTER 104
#define TEXT_POSITION_INNER 107

#define ANALOG_CENTER_X (SCREEN_WIDTH / 2)
#define ANALOG_CENTER_Y (TEXT_POSITION_OUTER / 2)
#define ANALOG_RADIUS   44
#define ANALOG_STROKE   4

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_analog_layer;

static void update_background(struct Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Prepare background rect before modifying bounds
  GRect back = bounds;
  back.origin.y = TEXT_POSITION_OUTER;
  back.size.h = TEXT_SIZE_OUTER;

  // Connection status
  bool connected = connection_service_peek_pebble_app_connection();
  graphics_context_set_fill_color(ctx, connected ? GColorBlue : GColorRed);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Battery status
  BatteryChargeState charge = battery_state_service_peek();
  bounds.origin.y = TEXT_POSITION_OUTER + TEXT_SIZE_INNER;
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
  GPoint center = (GPoint) {
    .x = ANALOG_CENTER_X,
    .y = ANALOG_CENTER_Y
  };

  // Draw background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, ANALOG_RADIUS + ANALOG_STROKE);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, ANALOG_RADIUS);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, ANALOG_STROKE);
  
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *t = localtime(&temp);

  int32_t hour_angle = TRIG_MAX_ANGLE * (t->tm_hour % 12) / 12;
  int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  
  // Prepare lines
  GPoint hour = (GPoint) {
    .x = (sin_lookup(hour_angle) * ANALOG_RADIUS / TRIG_MAX_RATIO / 2) + center.x,
    .y = (-cos_lookup(hour_angle) * ANALOG_RADIUS / TRIG_MAX_RATIO / 2) + center.y
  };
  GPoint minute = (GPoint) {
    .x = (sin_lookup(minute_angle) * (ANALOG_RADIUS - ANALOG_STROKE) / TRIG_MAX_RATIO) + center.x,
    .y = (-cos_lookup(minute_angle) * (ANALOG_RADIUS - ANALOG_STROKE) / TRIG_MAX_RATIO) + center.y
  };

  // Draw lines
  graphics_draw_line(ctx, center, hour);
  graphics_draw_line(ctx, center, minute);
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
  
  layer_mark_dirty(s_analog_layer);
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
  
  //-- Create the TextLayer with specific bounds --
  s_time_layer = text_layer_create(GRect(0, TEXT_POSITION_INNER, bounds.size.w, TEXT_SIZE_INNER));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorWhite);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  //-- Create analog layer --
  s_analog_layer = layer_create(bounds);
  layer_set_update_proc(s_analog_layer, &update_analog);
  layer_add_child(window_layer, s_analog_layer);
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  layer_destroy(s_analog_layer);
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