#include "debounce.h"

#include <string.h>

void db_init(struct db_state *s, uint32_t threshold_ms, enum db_mode mode,
             unsigned enabled_mask)
{
    memset(s, 0, sizeof *s);
    s->threshold_ms = threshold_ms;
    s->mode = mode;
    s->enabled_mask = enabled_mask;
}

void db_reset(struct db_state *s)
{
    memset(s->btn, 0, sizeof s->btn);
}

static int within_threshold(const struct db_state *s,
                            const struct db_button_state *st, uint32_t time)
{
    return st->has_up && (uint32_t)(time - st->last_up) < s->threshold_ms;
}

static struct db_result block_press(struct db_state *s,
                                    struct db_button_state *st, int is_down,
                                    uint32_t time)
{
    struct db_result r = {0};

    if (is_down) {
        r.block = st->swallow_up = within_threshold(s, st, time);
    } else {
        st->last_up = time;
        st->has_up = 1;
        r.block = st->swallow_up;
        st->swallow_up = 0;
    }
    return r;
}

static struct db_result hold_release(struct db_state *s,
                                     struct db_button_state *st, int is_down,
                                     uint32_t time)
{
    struct db_result r = {0};

    if (!is_down) {
        /* Segura o "soltar"; o timer o libera se nada acontecer. */
        st->last_up = time;
        st->has_up = 1;
        st->pending_up = 1;
        r.block = 1;
        r.start_timer = 1;
        return r;
    }

    if (!st->pending_up)
        return r;

    st->pending_up = 0;
    r.block = 1;
    r.cancel_timer = 1;
    if (!within_threshold(s, st, time)) {
        /*
         * O timer atrasou e um clique legítimo chegou antes dele. Reproduz
         * a ordem real: o "soltar" segurado e depois este "apertar".
         */
        r.inject_up = 1;
        r.inject_down = 1;
    }
    return r;
}

struct db_result db_on_event(struct db_state *s, enum db_button b, int is_down,
                             uint32_t time)
{
    struct db_result pass = {0};

    if ((unsigned)b >= DB_BUTTON_COUNT || !(s->enabled_mask & (1u << b)))
        return pass;

    if (s->mode == DB_MODE_HOLD_RELEASE)
        return hold_release(s, &s->btn[b], is_down, time);
    return block_press(s, &s->btn[b], is_down, time);
}

int db_on_timer(struct db_state *s, enum db_button b)
{
    if ((unsigned)b >= DB_BUTTON_COUNT || !s->btn[b].pending_up)
        return 0;
    s->btn[b].pending_up = 0;
    return 1;
}

unsigned db_flush(struct db_state *s)
{
    unsigned mask = 0;
    int b;

    for (b = 0; b < DB_BUTTON_COUNT; b++)
        if (s->btn[b].pending_up)
            mask |= 1u << b;
    db_reset(s);
    return mask;
}
