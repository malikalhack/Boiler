/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    task_priorities.h
 * @version 0.1.6
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef TASK_PRIORITIES_H
#define TASK_PRIORITIES_H

/******************************** Included files ******************************/
#include <stdint.h>

#include "tasks.h"
/****************************** Module variables ******************************/

/**
 * @var task_priorities
 * @brief Expected configured dispatch priorities indexed by task descriptor.
 */
static const uint8_t task_priorities[eTaskMenu + 1U] = {
    [eTaskDisplay] = TASK_PRIORITY_DISPLAY,
    [eTaskFeedback] = TASK_PRIORITY_FEEDBACK,
    [eTaskButtons] = TASK_PRIORITY_BUTTONS,
    [eTaskSensors] = TASK_PRIORITY_SENSORS,
    [eTaskMenu] = TASK_PRIORITY_MENU
};
/******************************************************************************/
#endif // !TASK_PRIORITIES_H
