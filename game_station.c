#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "station.h"

#define FPS           30
#define SAVE_PATH     APP_DATA_PATH("scores.bin")
#define SAVE_MAGIC    0x47535431u /* GST1 */

typedef enum {
    EvtTick,
    EvtInput,
} EvtType;

typedef struct {
    EvtType type;
    InputEvent input;
} Evt;

typedef struct {
    Station* st;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
} App;

/* ---- sound sequences ---- */

static const NotificationSequence s_blip = {
    &message_note_c6, &message_delay_10, &message_sound_off, NULL};
static const NotificationSequence s_tick = {
    &message_note_g5, &message_delay_10, &message_sound_off, NULL};
static const NotificationSequence s_hit = {
    &message_vibro_on, &message_red_255, &message_note_a4, &message_delay_50,
    &message_vibro_off, &message_red_0, &message_sound_off, NULL};
static const NotificationSequence s_power = {
    &message_note_c6, &message_delay_25, &message_note_e6, &message_delay_25,
    &message_note_g6, &message_delay_25, &message_sound_off, NULL};
static const NotificationSequence s_alarm = {
    &message_note_c5, &message_delay_100, &message_note_a4, &message_delay_100,
    &message_sound_off, NULL};
static const NotificationSequence s_over = {
    &message_vibro_on, &message_note_c5, &message_delay_100, &message_vibro_off,
    &message_note_a4, &message_delay_100, &message_note_f4, &message_delay_250,
    &message_sound_off, NULL};
static const NotificationSequence s_win = {
    &message_green_255, &message_note_c6, &message_delay_50, &message_note_e6,
    &message_delay_50, &message_note_g6, &message_delay_50, &message_note_c7,
    &message_delay_100, &message_green_0, &message_sound_off, NULL};

/* Simon pads: four distinct tones with a lit LED */
static const NotificationSequence s_pad1 = {
    &message_blue_255, &message_note_c5, &message_delay_250, &message_note_c5,
    &message_sound_off, &message_blue_0, NULL};
static const NotificationSequence s_pad2 = {
    &message_green_255, &message_note_e5, &message_delay_250, &message_note_e5,
    &message_sound_off, &message_green_0, NULL};
static const NotificationSequence s_pad3 = {
    &message_red_255, &message_note_g5, &message_delay_250, &message_note_g5,
    &message_sound_off, &message_red_0, NULL};
static const NotificationSequence s_pad4 = {
    &message_blue_255, &message_green_255, &message_note_c6, &message_delay_250,
    &message_note_c6, &message_sound_off, &message_blue_0, &message_green_0, NULL};

static void play_sfx(NotificationApp* n, uint8_t sfx, uint8_t tone) {
    if(sfx & SFX_GAME_OVER) notification_message(n, &s_over);
    else if(sfx & SFX_WIN) notification_message(n, &s_win);
    else if(sfx & SFX_HIT) notification_message(n, &s_hit);
    else if(sfx & SFX_ALARM) notification_message(n, &s_alarm);
    else if(sfx & SFX_POWER) notification_message(n, &s_power);
    else if(sfx & SFX_BLIP) notification_message(n, &s_blip);
    else if(sfx & SFX_TICK) notification_message(n, &s_tick);

    switch(tone) {
    case 1: notification_message(n, &s_pad1); break;
    case 2: notification_message(n, &s_pad2); break;
    case 3: notification_message(n, &s_pad3); break;
    case 4: notification_message(n, &s_pad4); break;
    default: break;
    }
}

/* ---- persistence ---- */

static void scores_load(uint32_t* best) {
    uint32_t hdr = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &hdr, sizeof(hdr)) == sizeof(hdr) && hdr == SAVE_MAGIC) {
            storage_file_read(file, best, sizeof(uint32_t) * GAME_COUNT);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void scores_save(const uint32_t* best) {
    uint32_t hdr = SAVE_MAGIC;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &hdr, sizeof(hdr));
        storage_file_write(file, best, sizeof(uint32_t) * GAME_COUNT);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ---- callbacks ---- */

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    station_draw(app->st, canvas);
    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* input, void* ctx) {
    App* app = ctx;
    Evt e = {.type = EvtInput, .input = *input};
    furi_message_queue_put(app->queue, &e, FuriWaitForever);
}

static void timer_cb(void* ctx) {
    App* app = ctx;
    if(furi_message_queue_get_count(app->queue) > 2) return;
    Evt e = {.type = EvtTick};
    furi_message_queue_put(app->queue, &e, 0);
}

int32_t game_station_app(void* p) {
    UNUSED(p);

    App app;
    app.st = malloc(sizeof(Station));
    app.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app.queue = furi_message_queue_alloc(8, sizeof(Evt));

    uint32_t best[GAME_COUNT] = {0};
    scores_load(best);
    station_init(app.st, furi_hal_random_get(), best);

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, &app);
    view_port_input_callback_set(vp, input_cb, &app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notif, &sequence_display_backlight_enforce_on);

    FuriTimer* timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, &app);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / FPS);

    Evt e;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app.queue, &e, FuriWaitForever) != FuriStatusOk) continue;

        uint8_t sfx = 0, tone = 0;
        bool save = false;
        uint32_t now = furi_get_tick() * 1000 / furi_kernel_get_tick_frequency();

        furi_mutex_acquire(app.mutex, FuriWaitForever);
        if(e.type == EvtTick) {
            station_tick(app.st, now);
        } else {
            running = station_input(app.st, e.input.key, e.input.type, now);
        }
        sfx = app.st->sfx;
        tone = app.st->tone;
        app.st->sfx = 0;
        app.st->tone = 0;
        if(app.st->save_pending) {
            app.st->save_pending = false;
            save = true;
            memcpy(best, app.st->best, sizeof(best));
        }
        furi_mutex_release(app.mutex);

        if(sfx || tone) play_sfx(notif, sfx, tone);
        if(save) scores_save(best);
        view_port_update(vp);
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    notification_message(notif, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app.queue);
    furi_mutex_free(app.mutex);
    station_free(app.st);
    free(app.st);
    return 0;
}
