#include <pebble.h>

static Window *s_window;
static Layer *s_layer;
static GRect s_layer_frame;
static InverterLayer *s_inverter_layer;

// for accelerometer data
typedef struct {
    int32_t x;
    int32_t y;
} APoint;

#define OLD     (0)
#define NEW     (1)
#define GEN(n)  (n+2)
#define MIN_GEN (0)
#define MAX_GEN (5)
static APoint s_accel_data[GEN(MAX_GEN)+1];
static int s_gen;

// for pointer
static GPoint s_point;

// for grid
#define MAX_RECT_W  (7)
#define MAX_RECT_H  (7)
#define MAX_RECT    (MAX_RECT_W * MAX_RECT_H)
static GRect s_rect[MAX_RECT];

static void s_layer_update_callback(struct Layer *layer, GContext *ctx) {
    (void)layer;

    // set color
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorBlack);

    // draw grid
    for (int i = 0; i < MAX_RECT; i++) {
        graphics_draw_rect(ctx, s_rect[i]);
    }

    // draw pointer
    graphics_draw_circle(ctx, s_point, 5);
    graphics_draw_pixel(ctx, s_point);
}

static void s_accel_data_handler(AccelData *data, uint32_t num_samples) {
    s_accel_data[GEN(s_gen)].x = data[0].x;
    s_accel_data[GEN(s_gen)].y = data[0].y;

    s_gen++;
    if (MAX_GEN < s_gen) {
        // calc NEW data
        // The NEW is average of five(MAX_GEN) latest data
        int32_t nx = s_accel_data[GEN(MIN_GEN)].x;
        int32_t ny = s_accel_data[GEN(MIN_GEN)].y;
        for (int i = MIN_GEN + 1; i <= MAX_GEN; i++) {
            nx += s_accel_data[GEN(i)].x;
            ny += s_accel_data[GEN(i)].y;
            nx /= 2;
            ny /= 2;
        }
        s_accel_data[NEW].x = nx;
        s_accel_data[NEW].y = ny;

        // calc point
        int32_t dx = s_accel_data[OLD].x - s_accel_data[NEW].x;
        int32_t dy = s_accel_data[OLD].y - s_accel_data[NEW].y;
        dx /= 10;
        dy /= 10;
        int32_t x = s_point.x - dx;
        int32_t y = s_point.y + dy;

        if (x < 0) {
            x = 0;
        } else if (s_layer_frame.size.w < x) {
            x = s_layer_frame.size.w;
        }
        if (y < 0) {
            y = 0;
        } else if (s_layer_frame.size.h < y) {
            y = s_layer_frame.size.h;
        }

        s_point.x = x;
        s_point.y = y;

        // in rect
        for (int i = 0; i < MAX_RECT; i++) {
            if (grect_contains_point(&s_rect[i], &s_point) == true) {
                layer_set_frame(inverter_layer_get_layer(s_inverter_layer), s_rect[i]);
                break;
            }
        }
        
        // upadte
        s_gen = MIN_GEN;
        s_accel_data[OLD] = s_accel_data[NEW];
        layer_mark_dirty(s_layer);
    }
}

static void s_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    // reset point
    s_point.x = s_layer_frame.size.w / 2;
    s_point.y = s_layer_frame.size.h / 2;
    layer_mark_dirty(s_layer);
}

static void s_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, s_select_single_click_handler);
}

static void s_window_load(Window *window) {
    memset(&s_accel_data[OLD], 0, sizeof(AccelData));
    
    Layer *window_layer = window_get_root_layer(window);
    GRect window_frame = layer_get_frame(window_layer);
    window_set_click_config_provider(window, s_config_provider);

    // create canvas
    s_layer = layer_create(window_frame);
    s_layer_frame = layer_get_frame(s_layer);
    layer_set_update_proc(s_layer, s_layer_update_callback);
    layer_add_child(window_layer, s_layer);

    s_inverter_layer = inverter_layer_create(s_layer_frame);
    layer_add_child(window_layer, inverter_layer_get_layer(s_inverter_layer));

    // reset point
    s_point.x = s_layer_frame.size.w / 2;
    s_point.y = s_layer_frame.size.h / 2;
    
    // grid
    uint16_t rw = s_layer_frame.size.w / MAX_RECT_W;
    uint16_t rh = s_layer_frame.size.h / MAX_RECT_H;
    for (int h = 0; h < MAX_RECT_H; h++) {
        for (int w = 0; w < MAX_RECT_W; w++) {
            s_rect[(h * MAX_RECT_W) + w].origin.x = w * rw;
            s_rect[(h * MAX_RECT_W) + w].origin.y = h * rh;
            s_rect[(h * MAX_RECT_W) + w].size.w = rw + 1;
            s_rect[(h * MAX_RECT_W) + w].size.h = rh + 1;
        }
    }

    // start accelerometer
    s_gen = MIN_GEN;
    accel_data_service_subscribe(1, s_accel_data_handler);
}

static void s_window_unload(Window *window) {
    // stop accelerometer
    accel_data_service_unsubscribe();

    inverter_layer_destroy(s_inverter_layer);
    layer_destroy(s_layer);
}

static void s_init() {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = s_window_load,
        .unload = s_window_unload,
    });
    window_stack_push(s_window, true /* Animated */);
}

static void s_deinit() {
    window_destroy(s_window);
}

int main(void) {
    s_init();
    app_event_loop();
    s_deinit();
}