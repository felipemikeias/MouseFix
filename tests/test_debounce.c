/*
 * Filter tests. They run on any system with a C compiler:
 *   cc -std=c99 -Wall -Isrc src/debounce.c tests/test_debounce.c -o test && ./test
 */
#include "debounce.h"

#include <stdio.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                   \
        }                                                                 \
    } while (0)

#define LEFT_ONLY (1u << DB_LEFT)
#define DOWN 1
#define UP 0

static void test_block_press_filters_bounce(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, LEFT_ONLY);

    CHECK(!db_on_event(&s, DB_LEFT, DOWN, 1000).block);
    CHECK(!db_on_event(&s, DB_LEFT, UP, 1100).block);
    /* new press 10 ms after the release: chatter */
    CHECK(db_on_event(&s, DB_LEFT, DOWN, 1110).block);
    /* its release is dropped too */
    CHECK(db_on_event(&s, DB_LEFT, UP, 1150).block);
}

static void test_block_press_allows_real_double_click(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, LEFT_ONLY);

    CHECK(!db_on_event(&s, DB_LEFT, DOWN, 1000).block);
    CHECK(!db_on_event(&s, DB_LEFT, UP, 1080).block);
    CHECK(!db_on_event(&s, DB_LEFT, DOWN, 1200).block);
    CHECK(!db_on_event(&s, DB_LEFT, UP, 1280).block);
}

static void test_threshold_boundary(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, LEFT_ONLY);

    db_on_event(&s, DB_LEFT, DOWN, 0);
    db_on_event(&s, DB_LEFT, UP, 100);
    CHECK(db_on_event(&s, DB_LEFT, DOWN, 139).block);
    db_on_event(&s, DB_LEFT, UP, 160);
    /* exactly 40 ms: passes, like the original (cmp eax,0x28 / jae) */
    CHECK(!db_on_event(&s, DB_LEFT, DOWN, 200).block);
}

static void test_first_press_never_blocked(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, LEFT_ONLY);

    /* no previous release, even with a low clock value */
    CHECK(!db_on_event(&s, DB_LEFT, DOWN, 5).block);
}

static void test_tick_wraparound(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, LEFT_ONLY);

    db_on_event(&s, DB_LEFT, DOWN, 0xFFFFFF00u);
    db_on_event(&s, DB_LEFT, UP, 0xFFFFFFF0u);
    /* 0xFFFFFFF0 -> 0x00000005 = 21 ms */
    CHECK(db_on_event(&s, DB_LEFT, DOWN, 0x00000005u).block);
}

static void test_disabled_button_passes(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, LEFT_ONLY);

    db_on_event(&s, DB_RIGHT, DOWN, 0);
    db_on_event(&s, DB_RIGHT, UP, 100);
    CHECK(!db_on_event(&s, DB_RIGHT, DOWN, 105).block);
}

static void test_buttons_are_independent(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_BLOCK_PRESS, (1u << DB_LEFT) | (1u << DB_RIGHT));

    db_on_event(&s, DB_LEFT, DOWN, 0);
    db_on_event(&s, DB_LEFT, UP, 100);
    CHECK(!db_on_event(&s, DB_RIGHT, DOWN, 110).block);
}

static void test_hold_release_absorbs_bounce_during_drag(void)
{
    struct db_state s;
    struct db_result r;
    db_init(&s, 40, DB_MODE_HOLD_RELEASE, LEFT_ONLY);

    CHECK(!db_on_event(&s, DB_LEFT, DOWN, 1000).block);
    r = db_on_event(&s, DB_LEFT, UP, 1500);     /* chatter in the middle of a drag */
    CHECK(r.block && r.start_timer);
    r = db_on_event(&s, DB_LEFT, DOWN, 1510);
    CHECK(r.block && r.cancel_timer && !r.inject_up && !r.inject_down);
    CHECK(db_on_timer(&s, DB_LEFT) == 0);       /* nothing to let go */

    r = db_on_event(&s, DB_LEFT, UP, 2000);     /* real release */
    CHECK(r.block && r.start_timer);
    CHECK(db_on_timer(&s, DB_LEFT) == 1);       /* let go by the timer */
}

static void test_hold_release_late_timer_keeps_order(void)
{
    struct db_state s;
    struct db_result r;
    db_init(&s, 40, DB_MODE_HOLD_RELEASE, LEFT_ONLY);

    db_on_event(&s, DB_LEFT, DOWN, 1000);
    db_on_event(&s, DB_LEFT, UP, 1100);
    /* timer has not run yet, but 60 ms passed: legitimate click */
    r = db_on_event(&s, DB_LEFT, DOWN, 1160);
    CHECK(r.block && r.cancel_timer && r.inject_up && r.inject_down);
    CHECK(db_on_timer(&s, DB_LEFT) == 0);
}

static void test_flush_releases_pending(void)
{
    struct db_state s;
    db_init(&s, 40, DB_MODE_HOLD_RELEASE,
            (1u << DB_LEFT) | (1u << DB_MIDDLE));

    db_on_event(&s, DB_LEFT, DOWN, 0);
    db_on_event(&s, DB_LEFT, UP, 50);
    CHECK(db_flush(&s) == (1u << DB_LEFT));
    CHECK(db_flush(&s) == 0);
}

int main(void)
{
    test_block_press_filters_bounce();
    test_block_press_allows_real_double_click();
    test_threshold_boundary();
    test_first_press_never_blocked();
    test_tick_wraparound();
    test_disabled_button_passes();
    test_buttons_are_independent();
    test_hold_release_absorbs_bounce_during_drag();
    test_hold_release_late_timer_keeps_order();
    test_flush_releases_pending();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
