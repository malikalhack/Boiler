/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    menu.c
 * @version 0.1.5
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "boiler_config.h"

#if ((SENSORS_USE_FAKE == 1) && (CONFIG_USE_TEST == 0))
#include <string.h>

#include "acrosched_defs.h"
#include "bsp.h"
#include "display.h"
#include "menu.h"
#include "sensors_fake.h"

/********************************* Definitions ********************************/
/**
 * @def MENU_PRESENCE_MASK
 * @brief Two fixture presence bits used by Right and decimal-point indication.
 */
#define MENU_PRESENCE_MASK (3U)

/****************************** Module variables ******************************/
static EMenuState_t state;
static EMenuState_t retry_state;
static EError_t idle_error;
static EError_t shown_error;
static EResult_t last_result;
static uint8_t presence_mask;
static SBoilerConfig_t candidate;

/***************************** Private prototypes *****************************/
static void render(void);
static void fail(EMenuState_t retry, EError_t error, EResult_t result);
static void measure(uint8_t assignment);
static void verify(bool save);

/****************************** Private functions *****************************/
static void render(void) {
    SDisplayFrame_t frame = {{eGlyphBlank, eGlyphBlank,
                             eGlyphBlank, eGlyphBlank, 0U}};

    switch (state) {
        case eMenuProfile:
            frame.slots[0] = eGlyphP;
            frame.slots[3] = candidate.operating_profile == eProfileSingle ?
                eGlyph1 : eGlyph2;
            break;
        case eMenuSensor1:
        case eMenuMeasure1:
            frame.slots[0] = eGlyph5;
            frame.slots[1] = eGlyph1;
            break;
        case eMenuSensor2:
        case eMenuMeasure2:
            frame.slots[0] = eGlyph5;
            frame.slots[1] = eGlyph2;
            break;
        case eMenuCheck:
        case eMenuVerify:
            frame = (SDisplayFrame_t){
                {eGlyphC, eGlyph0, eGlyphN, eGlyphF, 0U}
            };
            break;
        case eMenuSave:
        case eMenuPreSave:
        case eMenuSaving:
            frame = (SDisplayFrame_t){
                {eGlyph5, eGlyphA, eGlyphV, eGlyphE, 0U}
            };
            break;
        case eMenuFailure:
            IGNORE_RETURN(display_show_error(shown_error));
            display_set_points(presence_mask);
            return;
        default:
            return;
    }
    display_set_digits(frame);
    display_set_points(presence_mask);
}
/*----------------------------------------------------------------------------*/

static void fail(EMenuState_t retry, EError_t error, EResult_t result) {
    retry_state = retry;
    shown_error = error;
    last_result = result;
    state = eMenuFailure;
    render();
}
/*----------------------------------------------------------------------------*/

static void measure(uint8_t assignment) {
    SSensorRom_t roms[SENSOR_FAKE_COUNT];
    SBoilerConfig_t trial;
    uint8_t count;
    EResult_t result;
    EMenuState_t retry = assignment == 0U ? eMenuMeasure1 : eMenuMeasure2;

    result = sensors_search(roms, SENSOR_FAKE_COUNT, &count);
    if (result != eResultOk) {
        fail(retry, result == eResultOutOfRange ?
             eErrorDeviceCount : eErrorSoft1Wire, result);
        return;
    }
    if (count != 1U) {
        fail(retry, eErrorDeviceCount, eResultOk);
        return;
    }
    trial = candidate;
    memcpy(trial.sensor_rom[assignment], roms[0].bytes, SENSOR_ROM_SIZE);
    if (assignment == 0U) {
        trial.operating_profile = eProfileSingle;
    }
    result = config_validate(&trial);
    if (result != eResultOk) {
        fail(retry, eErrorConfigInvalid, result);
        return;
    }
    memcpy(candidate.sensor_rom[assignment], roms[0].bytes, SENSOR_ROM_SIZE);
    last_result = eResultOk;
    state = (assignment == 0U &&
             candidate.operating_profile == eProfileTwo) ?
        eMenuSensor2 : eMenuCheck;
    render();
}
/*----------------------------------------------------------------------------*/

static void verify(bool save) {
    EError_t error;
    EResult_t result;
    EMenuState_t retry = save ? eMenuPreSave : eMenuVerify;

    result = config_check_sensors(&candidate, &error);
    if (result != eResultOk) {
        fail(retry, eErrorSoft1Wire, result);
        return;
    }
    if (error != eErrorNone) {
        fail(retry, error, eResultOk);
        return;
    }
    if (save) {
        result = storage_save_begin(&candidate);
        if (result == eResultBusy) {
            return;
        }
        if (result != eResultOk) {
            fail(eMenuPreSave, eErrorConfigInvalid, result);
            return;
        }
        state = eMenuSaving;
        last_result = eResultBusy;
    }
    else {
        state = eMenuSave;
        last_result = eResultOk;
    }
    render();
}
/*----------------------------------------------------------------------------*/

/********************** Application Programming Interface *********************/
void menu_init(EError_t error, uint8_t presence) {
    state = eMenuIdle;
    idle_error = error;
    presence_mask = presence & MENU_PRESENCE_MASK;
    last_result = eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t menu_open(EError_t error, uint8_t presence) {
    if ((error < eErrorNone) || (error > eErrorWatchdog) ||
        (presence > MENU_PRESENCE_MASK)) {
        return eResultInvalidArgument;
    }
    if (state != eMenuIdle) {
        return eResultBusy;
    }
    idle_error = error;
    presence_mask = presence;
    bspHeaterOff(eBspHeater1);
    bspHeaterOff(eBspHeater2);
    candidate = (SBoilerConfig_t){0};
    candidate.operating_profile = eProfileSingle;
    last_result = eResultOk;
    state = eMenuProfile;
    render();
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

void menu_on_button(EButton_t button, EButtonEvent_t event) {
    uint8_t index;
    EResult_t result;

    if (
        (state == eMenuSaving)  ||
        (state == eMenuApplied) ||
        (event == eButtonReleased)
    ) {
        return;
    }
    if (state == eMenuIdle) {
        if ((button == eButtonEnter) && (event == eButtonPressed) &&
            (idle_error == eErrorConfigInvalid)) {
            IGNORE_RETURN(menu_open(idle_error, presence_mask));
        }
        return;
    }
    if ((button == eButtonLeft) && (event == eButtonPressed)) {
        state = eMenuIdle;
        if (idle_error == eErrorNone) {
            display_show_dashes();
        }
        else {
            IGNORE_RETURN(display_show_error(idle_error));
        }
        return;
    }
    if ((button == eButtonRight) && (event == eButtonPressed)) {
        presence_mask = (presence_mask + 1U) & MENU_PRESENCE_MASK;
        for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
            result = sensors_fake_set_present(
                index, (presence_mask & (1U << index)) != 0U);
            if (result != eResultOk) {
                fail(state, eErrorSoft1Wire, result);
                return;
            }
        }
        render();
        return;
    }
    if (state == eMenuProfile) {
        if (button == eButtonUp) {
            candidate.operating_profile = eProfileTwo;
        }
        else if (button == eButtonDown) {
            candidate.operating_profile = eProfileSingle;
        }
    }
    if ((button == eButtonEnter) && (event == eButtonPressed)) {
        switch (state) {
            case eMenuProfile: state = eMenuSensor1; break;
            case eMenuSensor1: state = eMenuMeasure1; break;
            case eMenuSensor2: state = eMenuMeasure2; break;
            case eMenuCheck: state = eMenuVerify; break;
            case eMenuSave: state = eMenuPreSave; break;
            case eMenuFailure: state = retry_state; break;
            default: break;
        }
    }
    render();
}
/*----------------------------------------------------------------------------*/

void menu_update(void *context) {
    EResult_t result;

    (void)context;
    switch (state) {
        case eMenuMeasure1: measure(0U); break;
        case eMenuMeasure2: measure(1U); break;
        case eMenuVerify: verify(false); break;
        case eMenuPreSave: verify(true); break;
        case eMenuSaving:
            result = storage_save_poll();
            last_result = result;
            if (result == eResultOk) {
                state = eMenuApplied;
            }
            else if (result != eResultBusy) {
                fail(eMenuPreSave, eErrorConfigInvalid, result);
            }
            break;
        default: break;
    }
}
/*----------------------------------------------------------------------------*/

EMenuState_t menu_get_state(void) {
    return state;
}
/*----------------------------------------------------------------------------*/

bool menu_is_active(void) {
    return state != eMenuIdle;
}
/*----------------------------------------------------------------------------*/

EResult_t menu_get_result(void) {
    return last_result;
}
/******************************************************************************/
#endif /* SENSORS_USE_FAKE && CONFIG_USE_TEST */
