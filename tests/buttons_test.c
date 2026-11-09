/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    buttons_test.c
 * @version 0.1.12
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <stdio.h>

#include "buttons.h"
#include "bsp.h"
/********************************* Definitions ********************************/

/**
 * @def TEST_EVENT_CAPACITY
 * @brief Space for event traces including a hold spanning tick wrap.
 */
#define TEST_EVENT_CAPACITY (256U)

/**
 * @def TEST_BUTTON_MASK
 * @brief All five logical button bits.
 */
#define TEST_BUTTON_MASK (eButtonDown | eButtonRight | eButtonUp | \
                          eButtonLeft | eButtonEnter)

typedef struct SEvent {
    EButton_t button;
    EButtonEvent_t event;
    AcroTick_t time;
} SEvent_t;

/****************************** Module variables ******************************/

static AcroTick_t tick;
static uint8_t raw;
static SEvent_t events[TEST_EVENT_CAPACITY];
static unsigned int event_count;

/********************** Application Programming Interface *********************/
AcroTick_t bsp_get_time(void) {
    return tick;
}
/*----------------------------------------------------------------------------*/

uint8_t bsp_get_buttons(void) {
    return raw;
}

/****************************** Private functions *****************************/

static void record_event(EButton_t button, EButtonEvent_t event) {
    assert(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count].button = button;
    events[event_count].event = event;
    events[event_count].time = tick;
    event_count++;
    if (event != eButtonReleased) {
        assert((buttons_get_state() & button) != 0U);
    }
    else {
        assert((buttons_get_state() & button) == 0U);
    }
}
/*----------------------------------------------------------------------------*/

static void advance(uint32_t ticks) {
    uint32_t count;

    for (count = 0U; count < ticks; count++) {
        tick++;
        buttons_poll(NULL);
    }
}
/*----------------------------------------------------------------------------*/

static void reset(uint8_t initial, AcroTick_t time) {
    raw = initial;
    tick = time;
    event_count = 0U;
    buttons_init(record_event);
    assert(buttons_get_state() == 0U);
}
/*----------------------------------------------------------------------------*/

static void set_raw(uint8_t value) {
    raw = value;
    buttons_poll(NULL);
}
/*----------------------------------------------------------------------------*/

static void test_all_combinations(void) {
    uint8_t mask;
    uint8_t index;
    uint8_t bit;
    unsigned int count;

    for (mask = 0U; mask <= TEST_BUTTON_MASK; mask++) {
        reset(mask, 0U);
        advance(BUTTON_DEBOUNCE_TICKS - 1U);
        assert(event_count == 0U);
        assert(buttons_get_state() == 0U);
        advance(1U);
        assert(buttons_get_state() == mask);
        count = 0U;
        for (bit = 1U; bit <= eButtonEnter; bit <<= 1U) {
            if ((mask & bit) != 0U) {
                assert(events[count].button == (EButton_t)bit);
                assert(events[count].event == eButtonPressed);
                count++;
            }
        }
        assert(event_count == count);
        set_raw(0U);
        advance(BUTTON_DEBOUNCE_TICKS - 1U);
        assert(event_count == count);
        assert(buttons_get_state() == mask);
        advance(1U);
        assert(buttons_get_state() == 0U);
        assert(event_count == 2U * count);
        for (index = 0U; index < count; index++) {
            assert(events[count + index].button == events[index].button);
            assert(events[count + index].event == eButtonReleased);
        }
    }
}
/*----------------------------------------------------------------------------*/

static void test_bounce_and_independence(void) {
    reset(0U, 0U);
    set_raw(eButtonEnter | eButtonUp);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    set_raw(eButtonEnter);
    advance(1U);
    assert(event_count == 1U);
    assert(events[0].button == eButtonEnter);
    assert(buttons_get_state() == eButtonEnter);
    set_raw(eButtonEnter | eButtonUp);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(event_count == 2U);
    assert(events[1].button == eButtonUp);
    set_raw(eButtonUp);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    set_raw(eButtonEnter | eButtonUp);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(event_count == 2U);
    set_raw(eButtonUp);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(event_count == 3U);
    assert(events[2].button == eButtonEnter);
    assert(events[2].event == eButtonReleased);
    assert(buttons_get_state() == eButtonUp);
}
/*----------------------------------------------------------------------------*/

static void test_repeat(EButton_t button) {
    AcroTick_t press_time;
    unsigned int count;

    reset(0U, 0U);
    set_raw(button);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(event_count == 1U);
    press_time = tick;
    advance(BUTTON_HOLD_TICKS - 1U);
    assert(event_count == 1U);
    advance(1U);
    assert(event_count == 2U);
    assert(events[1].event == eButtonRepeat);
    assert(events[1].time == (AcroTick_t)(press_time + BUTTON_HOLD_TICKS));
    for (count = 2U; count < 8U; count++) {
        advance(BUTTON_REPEAT_TICKS - 1U);
        assert(event_count == count);
        advance(1U);
        assert(event_count == count + 1U);
        assert(events[count].button == button);
        assert(events[count].event == eButtonRepeat);
        assert(events[count].time == (AcroTick_t)(
            press_time + BUTTON_HOLD_TICKS + (count - 1U) *
            BUTTON_REPEAT_TICKS));
    }
    advance(BUTTON_REPEAT_TICKS - 1U);
    set_raw(0U);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    assert(event_count == 8U);
    advance(1U);
    assert(event_count == 9U);
    assert(events[8].event == eButtonReleased);
    advance(BUTTON_HOLD_TICKS);
    assert(event_count == 9U);
    set_raw(button);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(event_count == 10U);
    advance(BUTTON_HOLD_TICKS - 1U);
    assert(event_count == 10U);
    advance(1U);
    assert(event_count == 11U);
}
/*----------------------------------------------------------------------------*/

static void test_nonrepeating_and_simultaneous(void) {
    reset(eButtonLeft | eButtonRight | eButtonEnter, 0U);
    advance(BUTTON_DEBOUNCE_TICKS + 2U * BUTTON_HOLD_TICKS);
    assert(event_count == 3U);
    set_raw(eButtonUp | eButtonDown);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(event_count == 8U);
    advance(BUTTON_HOLD_TICKS);
    assert(event_count == 10U);
    assert(events[8].button == eButtonDown);
    assert(events[9].button == eButtonUp);
    assert(events[8].event == eButtonRepeat);
    assert(events[9].event == eButtonRepeat);
    assert(events[8].time == events[9].time);
}
/*----------------------------------------------------------------------------*/

static void test_wrap_late_poll_and_reset(void) {
    unsigned int count;

    reset(eButtonDown, (AcroTick_t)-10);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    assert(event_count == 0U);
    advance(1U);
    assert(event_count == 1U);
    for (count = 0U; count < 140U; count++) {
        advance(count == 0U ? BUTTON_HOLD_TICKS : BUTTON_REPEAT_TICKS);
        assert(event_count == count + 2U);
        assert(events[count + 1U].event == eButtonRepeat);
    }

    reset(eButtonUp, 0U);
    advance(BUTTON_DEBOUNCE_TICKS);
    tick += BUTTON_HOLD_TICKS + 10U * BUTTON_REPEAT_TICKS;
    buttons_poll(NULL);
    assert(event_count == 2U);
    advance(BUTTON_REPEAT_TICKS - 1U);
    assert(event_count == 2U);
    advance(1U);
    assert(event_count == 3U);
    buttons_init(NULL);
    assert(buttons_get_state() == 0U);
    advance(BUTTON_DEBOUNCE_TICKS + BUTTON_HOLD_TICKS);
    assert(buttons_get_state() == eButtonUp);
    assert(event_count == 3U);
    set_raw(0U);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(buttons_get_state() == 0U);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    test_all_combinations();
    test_bounce_and_independence();
    test_repeat(eButtonUp);
    test_repeat(eButtonDown);
    test_nonrepeating_and_simultaneous();
    test_wrap_late_poll_and_reset();
    puts("Button tests passed");
    return 0;
}
/******************************************************************************/
