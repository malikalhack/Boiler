/**
 * @file    tasks.h
 * @version 0.1.0
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
    eTaskDisplay = 1,
    eTaskFeedback
} ETask_t;

/**
 * @def TASK_PRIORITY_DISPLAY
 * @brief Display dispatch priority; equal priorities use registration order.
 */
#define TASK_PRIORITY_DISPLAY (0U)
/**
 * @def TASK_PRIORITY_FEEDBACK
 * @brief Feedback task dispatch priority.
 */
#define TASK_PRIORITY_FEEDBACK (0U)
/******************************************************************************/
#endif // !TASKS_H
