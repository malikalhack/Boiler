/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    tasks.h
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef TASKS_H
#define TASKS_H

/********************************* Definitions ********************************/

/**
 * @enum ETask
 * @brief Unique scheduler descriptors; zero is reserved by AcroSched.
 */
typedef enum ETask {
    eTaskDisplay = 1, /**< Scan one display or LED position. */
    eTaskFeedback,    /**< Update the buzzer and error indication. */
    eTaskButtons,     /**< Poll and debounce the five buttons. */
    eTaskSensors,     /**< Advance sensor acquisition and diagnostics. */
    eTaskMenu         /**< Advance assignment and EEPROM handoff. */
} ETask_t;

/**
 * @def TASK_PRIORITY_DISPLAY
 * @brief Display dispatch priority; equal priorities use registration order.
 */
#define TASK_PRIORITY_DISPLAY (1U)
/**
 * @def TASK_PRIORITY_FEEDBACK
 * @brief Feedback task dispatch priority.
 */
#define TASK_PRIORITY_FEEDBACK (0U)
/**
 * @def TASK_PRIORITY_BUTTONS
 * @brief Button polling dispatch priority.
 */
#define TASK_PRIORITY_BUTTONS (0U)
/**
 * @def TASK_PRIORITY_SENSORS
 * @brief Sensor acquisition and diagnostic dispatch priority.
 */
#define TASK_PRIORITY_SENSORS (0U)
/**
 * @def TASK_PRIORITY_MENU
 * @brief Assignment service dispatch priority.
 */
#define TASK_PRIORITY_MENU (0U)
/******************************************************************************/
#endif // !TASKS_H
