#include <pebble.h>

#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168

#define GLOBAL_BORDER       4

#define TEXT_SIZE_OUTER     57
#define TEXT_SIZE_INNER     50
#define TEXT_POSITION_OUTER 94
#define TEXT_POSITION_INNER (TEXT_POSITION_OUTER + 3)

#define ANALOG_CENTER_X     (SCREEN_WIDTH / 4)
#define ANALOG_CENTER_Y     (TEXT_POSITION_OUTER / 2)
#define ANALOG_RADIUS       28 /* Without border */
#define ANALOG_STROKE       GLOBAL_BORDER

#define DATE_ONE_WIDTH      56 /* Including border */
#define DATE_ONE_HEIGHT     48 /* Including border */
#define DATE_BORDER         GLOBAL_BORDER
#define DATE_COUNT          2
#define DATE_STEP           (2 * DATE_BORDER)
#define DATE_FIRST_X        (3 * SCREEN_WIDTH / 4 - (DATE_ONE_WIDTH + (DATE_COUNT - 1) * DATE_STEP) / 2)
#define DATE_FIRST_Y        (ANALOG_CENTER_Y + ANALOG_RADIUS + ANALOG_STROKE - DATE_ONE_HEIGHT)

static Window    *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer     *s_analog_layer;

static GColor background_color;
static GColor connected_color;
static GColor battery_color;

static void update_background(struct Layer *layer, GContext *ctx) {
  //-- Prepare colors --
  // Init vars
  background_color = GColorDukeBlue;
  connected_color = GColorWhite;
  battery_color = GColorWhite;

  // Connected ?
  if (!connection_service_peek_pebble_app_connection()) {
    connected_color = GColorRed;    
  }

  // Battery status ?
  BatteryChargeState charge = battery_state_service_peek();
  if (charge.is_charging) {
    if (charge.charge_percent == 100) {
      // ... Charged!
      battery_color = GColorGreen;
    } else {
      // Charging ...
      battery_color = GColorYellow;
    }
  } else {
    if (charge.charge_percent <= 10) {
      battery_color = GColorRed;
    } else if (charge.charge_percent <= 30) {
      battery_color = GColorOrange;
    } else {
      battery_color = GColorWhite;
    }
  }
  
  //-- Paint backgrounds --
  // Global background
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Text black background
  bounds.origin.y = TEXT_POSITION_OUTER;
  bounds.size.h = TEXT_SIZE_OUTER;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  for (uint16_t offset = DATE_COUNT; offset--;) {
    // Date background
    bounds.origin.x = DATE_FIRST_X + offset * DATE_STEP;
    bounds.origin.y = DATE_FIRST_Y - offset * DATE_STEP;
    bounds.size.w = DATE_ONE_WIDTH;
    bounds.size.h = DATE_ONE_HEIGHT;
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 2 * DATE_BORDER, GCornersAll);
  
    bounds.origin.x = DATE_FIRST_X + DATE_BORDER + offset * DATE_STEP;
    bounds.origin.y = DATE_FIRST_Y + DATE_BORDER - offset * DATE_STEP;
    bounds.size.w = DATE_ONE_WIDTH - 2 * DATE_BORDER;
    bounds.size.h = DATE_ONE_HEIGHT - 2 * DATE_BORDER;
    graphics_context_set_fill_color(ctx, offset ? GColorLightGray : GColorWhite);
    graphics_fill_rect(ctx, bounds, DATE_BORDER, GCornersAll);
  }
  //-- Set numeric clock background --
  text_layer_set_background_color(s_time_layer, battery_color);
}

static void update_analog(struct Layer *layer, GContext *ctx) {
  GPoint center = (GPoint) {
    .x = ANALOG_CENTER_X,
    .y = ANALOG_CENTER_Y
  };

  // Draw background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, ANALOG_RADIUS + ANALOG_STROKE);
  graphics_context_set_fill_color(ctx, connected_color);
  graphics_fill_circle(ctx, center, ANALOG_RADIUS);
  
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
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, ANALOG_STROKE);
  graphics_draw_line(ctx, center, hour);
  graphics_draw_line(ctx, center, minute);
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer_time[6];
  strftime(s_buffer_time, sizeof(s_buffer_time), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer_time);
  
  // Write the current hours and minutes into a buffer
  static char s_buffer_date[6];
  strftime(s_buffer_date, sizeof(s_buffer_date), "%d", tick_time);
  // Display date on second TextLayer
  text_layer_set_text(s_date_layer, s_buffer_date);

  // Refresh analog layer
  layer_mark_dirty(s_analog_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void connection_handler(bool connected) {
  Layer * window_layer = window_get_root_layer(s_main_window);
  layer_mark_dirty(window_layer);
  
  static const uint32_t segments[] = { 100, 75, 100, 75, 100 };
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
  
  //-- Create the TextLayer for numeric clock --
  s_time_layer = text_layer_create(GRect(0, TEXT_POSITION_INNER, bounds.size.w, TEXT_SIZE_INNER));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorWhite);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  //-- Create the TextLayer for date --
  s_date_layer = text_layer_create(GRect(DATE_FIRST_X, DATE_FIRST_Y, DATE_ONE_WIDTH, DATE_ONE_HEIGHT));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_text(s_date_layer, "XX");
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  
  //-- Create analog layer --
  s_analog_layer = layer_create(bounds);
  layer_set_update_proc(s_analog_layer, &update_analog);
  layer_add_child(window_layer, s_analog_layer);
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
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