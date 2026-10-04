#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "game.h"

#define FPS            30
#define HIGHSCORE_PATH APP_DATA_PATH("highscore.bin")
#define HIGHSCORE_MAGIC 0x53504853u /* "SHPS" */

typedef enum {
    AppEventTick,
    AppEventInput,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    Game* game;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
} App;

static const NotificationSequence seq_kill = {
    &message_note_c7,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_hit = {
    &message_vibro_on,
    &message_red_255,
    &message_note_a4,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_power = {
    &message_green_255,
    &message_note_c6,
    &message_delay_25,
    &message_note_e6,
    &message_delay_25,
    &message_note_g6,
    &message_delay_25,
    &message_sound_off,
    &message_green_0,
    NULL,
};

static const NotificationSequence seq_boss = {
    &message_note_c5,
    &message_delay_100,
    &message_note_a4,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_boss_kill = {
    &message_vibro_on,
    &message_blue_255,
    &message_note_c6,
    &message_delay_50,
    &message_note_e6,
    &message_delay_50,
    &message_note_g6,
    &message_delay_50,
    &message_note_c7,
    &message_delay_100,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_game_over = {
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_note_a4,
    &message_delay_100,
    &message_vibro_off,
    &message_note_c5,
    &message_delay_250,
    &message_sound_off,
    NULL,
};

static uint32_t highscore_load(void) {
    uint32_t data[2] = {0, 0};
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, HIGHSCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, data, sizeof(data)) != sizeof(data)) data[0] = 0;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return data[0] == HIGHSCORE_MAGIC ? data[1] : 0;
}

static void highscore_save(uint32_t score) {
    uint32_t data[2] = {HIGHSCORE_MAGIC, score};
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, HIGHSCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, data, sizeof(data));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    game_draw(app->game, canvas);
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* ctx) {
    App* app = ctx;
    AppEvent event = {.type = AppEventInput, .input = *input};
    furi_message_queue_put(app->queue, &event, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    App* app = ctx;
    /* drop ticks instead of piling them up if a frame runs long */
    if(furi_message_queue_get_count(app->queue) > 2) return;
    AppEvent event = {.type = AppEventTick};
    furi_message_queue_put(app->queue, &event, 0);
}

static void play_sfx(NotificationApp* notif, uint8_t sfx) {
    if(sfx & SFX_GAME_OVER) {
        notification_message(notif, &seq_game_over);
    } else if(sfx & SFX_BOSS_KILL) {
        notification_message(notif, &seq_boss_kill);
    } else if(sfx & SFX_HIT) {
        notification_message(notif, &seq_hit);
    } else if(sfx & SFX_BOSS) {
        notification_message(notif, &seq_boss);
    } else if(sfx & SFX_POWER) {
        notification_message(notif, &seq_power);
    } else if(sfx & SFX_KILL) {
        notification_message(notif, &seq_kill);
    }
}

int32_t space_shooter_app(void* p) {
    UNUSED(p);

    App app;
    app.game = malloc(sizeof(Game));
    app.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app.queue = furi_message_queue_alloc(8, sizeof(AppEvent));

    game_init(app.game, furi_hal_random_get(), highscore_load());

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &app);
    view_port_input_callback_set(view_port, input_callback, &app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notif, &sequence_display_backlight_enforce_on);

    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, &app);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / FPS);

    AppEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app.queue, &event, FuriWaitForever) != FuriStatusOk) continue;

        uint8_t sfx = 0;
        bool save = false;
        uint32_t high = 0;

        furi_mutex_acquire(app.mutex, FuriWaitForever);
        if(event.type == AppEventTick) {
            game_update(app.game);
        } else {
            running = game_input(app.game, event.input.key, event.input.type);
        }
        sfx = app.game->sfx;
        app.game->sfx = 0;
        if(app.game->save_pending) {
            app.game->save_pending = false;
            save = true;
            high = app.game->high_score;
        }
        furi_mutex_release(app.mutex);

        if(sfx) play_sfx(notif, sfx);
        if(save) highscore_save(high);
        view_port_update(view_port);
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);

    notification_message(notif, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app.queue);
    furi_mutex_free(app.mutex);
    free(app.game);
    return 0;
}
