/*
 * debounce.h - switch "chatter" (phantom double click) filter, free of Win32.
 *
 * Every decision to block or pass a button event lives here, without any
 * system call, so it can be tested on any platform. The Win32 code (main.c)
 * only translates hook events into these calls and carries out what they ask.
 */
#ifndef MOUSEFIX_DEBOUNCE_H
#define MOUSEFIX_DEBOUNCE_H

#include <stdint.h>

enum db_button {
    DB_LEFT,
    DB_RIGHT,
    DB_MIDDLE,
    DB_X1,
    DB_X2,
    DB_BUTTON_COUNT
};

enum db_mode {
    /*
     * Same rule as the original MouseFix: a press arriving less than
     * threshold_ms after the last release is dropped. Unlike the original,
     * the matching release is dropped too, so programs never receive a
     * release without a press.
     */
    DB_MODE_BLOCK_PRESS,

    /*
     * Drag-safe: every release is held back for threshold_ms. If the button
     * is pressed again within that time, both events are dropped and the
     * click goes on as if it had never been released.
     * Cost: releases arrive threshold_ms later.
     */
    DB_MODE_HOLD_RELEASE
};

struct db_button_state {
    uint32_t last_up;       /* time of the last physical release (ms) */
    int has_up;             /* last_up is valid */
    int swallow_up;         /* BLOCK_PRESS: drop the next release */
    int pending_up;         /* HOLD_RELEASE: a release is being held back */
};

struct db_state {
    uint32_t threshold_ms;
    enum db_mode mode;
    unsigned enabled_mask;  /* bit (1u << db_button) = button is filtered */
    struct db_button_state btn[DB_BUTTON_COUNT];
};

/* Actions the caller must carry out, in field order. */
struct db_result {
    int block;              /* drop the received event */
    int inject_up;          /* inject a release of this button */
    int inject_down;        /* then inject a press of this button */
    int start_timer;        /* schedule db_on_timer() in threshold_ms */
    int cancel_timer;       /* cancel this button's scheduled timer */
};

void db_init(struct db_state *s, uint32_t threshold_ms, enum db_mode mode,
             unsigned enabled_mask);

/* Clears the state of every button (use when pausing or changing mode). */
void db_reset(struct db_state *s);

/*
 * Processes a physical event. `time` is the event timestamp in ms
 * (MSLLHOOKSTRUCT.time); the arithmetic is unsigned, so the counter
 * wrapping around every ~49 days is handled correctly.
 */
struct db_result db_on_event(struct db_state *s, enum db_button b, int is_down,
                             uint32_t time);

/*
 * Called when a button's timer fires (HOLD_RELEASE mode).
 * Returns 1 if the caller must inject the release that was held back.
 */
int db_on_timer(struct db_state *s, enum db_button b);

/*
 * Lets go of every held-back release (when pausing or exiting) so no button
 * stays stuck. Returns the mask of buttons that need a release injected.
 */
unsigned db_flush(struct db_state *s);

#endif
