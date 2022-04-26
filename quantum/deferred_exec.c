// Copyright 2021 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <timer.h>
#include <deferred_exec.h>

// This needs to be a power of 2, so 2, 4, 8 or 16 with a max of 16
#ifndef MAX_DEFERRED_EXECUTORS
#    define MAX_DEFERRED_EXECUTORS 4
#endif

// Make sure MAX_DEFERRED_EXECUTORS is a power of 2
#if (MAX_DEFERRED_EXECUTORS & (MAX_DEFERRED_EXECUTORS - 1)) != 0
#    if MAX_DEFERRED_EXECUTORS < 4
#        undef MAX_DEFERRED_EXECUTORS
#        define MAX_DEFERRED_EXECUTORS 4
#    elif MAX_DEFERRED_EXECUTORS < 8
#        undef MAX_DEFERRED_EXECUTORS
#        define MAX_DEFERRED_EXECUTORS 8
#    elif MAX_DEFERRED_EXECUTORS < 16
#        undef MAX_DEFERRED_EXECUTORS
#        define MAX_DEFERRED_EXECUTORS 16
#    else
#        undef MAX_DEFERRED_EXECUTORS
#        define MAX_DEFERRED_EXECUTORS 32
#    endif
#endif

#if MAX_DEFERRED_EXECUTORS < 9
typedef uint8_t free_executors_t;
#define MAX_TOKEN_VALUE 256
#elif MAX_DEFERRED_EXECUTORS < 17
typedef uint16_t free_executors_t;
#define MAX_TOKEN_VALUE 65536
#else
#error "MAX_DEFERRED_EXECUTORS must be a power of 2 and less than 17"
#endif

/**
 * @struct Structure for containing self-hosted deferred executor tables.
 * @brief Core-side code can use this to create their own tables without impacting on the use of users' ability to add deferred execution.
 *        Code outside deferred_exec.c should not worry about internals of this struct, and should just allocate the required number in an array.
 */
typedef struct deferred_executor_t {
    deferred_token         token;
    uint32_t               trigger_time;
    deferred_exec_callback callback;
    void                  *cb_arg;
} deferred_executor_t;

//------------------------------------
// Helpers
//

static uint32_t            last_deferred_exec_check                = 0;
static deferred_executor_t basic_executors[MAX_DEFERRED_EXECUTORS] = {0};
static free_executors_t    basic_executors_free                    = 0;

// A token is rotating for a limited amount of values according to the bitsize of the free executors type.
// So for a uint8_t type and a MAX_DEFERRED_EXECUTORS of 4, the token will rotate through N values, where N is:
//    256 / 4 = 64
//         entry 0 will rotate the token as  1...63
//         entry 1 will rotate the token as 64..127
// The value '0' is reserved for the invalid token 'INVALID_DEFERRED_TOKEN'.

//------------------------------------
// Advanced API: used when a custom-allocated table is used, primarily for core code.
//

/**
 * Forward declaration for the main loop in order to execute any custom table deferred executors. Should not be invoked by keyboard/user code.
 * Needed for any custom-allocated deferred execution tables. Any core tasks should add appropriate invocation to quantum/main.c.
 *
 * @param table[in] the custom table used for storage
 * @param table_count[in] the number of available items in the table
 * @param last_execution_time[in,out] the last execution time -- this will be checked first to determine if execution is needed, and updated if execution occurred
 */
static deferred_token defer_exec_advanced(uint32_t delay_ms, deferred_exec_callback callback, void *cb_arg) {
    // Ignore queueing if it's a zero-time delay, or the token is not valid
    if (delay_ms == 0 || !callback) {
        return INVALID_DEFERRED_TOKEN;
    }

    free_executors_t free = basic_executors_free;
    for (uint8_t i = 0; i < MAX_DEFERRED_EXECUTORS; ++i) {
        if ((free & 1) == 0) {
            // This is an unused slot, claim it
            basic_executors_free |= 1 << i;
            deferred_executor_t *entry = &basic_executors[i];

            // Work out the new token value
            entry->token = (entry->token + 1);
            if (entry->token >= ((i * (MAX_TOKEN_VALUE / MAX_DEFERRED_EXECUTORS)) + (MAX_TOKEN_VALUE / MAX_DEFERRED_EXECUTORS) - 1)) {
                entry->token = i * (MAX_TOKEN_VALUE / MAX_DEFERRED_EXECUTORS);

                // Do not use the INVALID_DEFERRED_TOKEN value, as it is used to indicate an error
                if (entry->token == INVALID_DEFERRED_TOKEN) {
                    entry->token = INVALID_DEFERRED_TOKEN + 1;
                }
            }

            // Set up the executor entry
            entry->trigger_time = timer_read32() + delay_ms;
            entry->callback     = callback;
            entry->cb_arg       = cb_arg;
            return entry->token;
        }
        free = free >> 1;
    }

    // None available
    return INVALID_DEFERRED_TOKEN;
}

/**
 * Allows for extending the timeframe before an existing deferred execution is invoked.
 *
 * @param token[in] the returned value from defer_exec for the deferred execution you wish to extend
 * @param delay_ms[in] the number of milliseconds before executing the callback
 * @return true if the token was extended successfully, otherwise false
 */
static bool extend_deferred_exec_advanced(deferred_token token, uint32_t delay_ms) {
    // Ignore queueing if it's a zero-time delay, or the token is not valid
    if (delay_ms == 0 || token == INVALID_DEFERRED_TOKEN) {
        return false;
    }

    // the entry corresponding to the token
    uint8_t index = token / (MAX_TOKEN_VALUE / MAX_DEFERRED_EXECUTORS);
    {
        deferred_executor_t *entry = &basic_executors[index];
        if (entry->token == token) {
            // extend the delay
            entry->trigger_time = timer_read32() + delay_ms;
            return true;
        }
    }

    // Not found
    return false;
}

/**
 * Allows for cancellation of an existing deferred execution.
 *
 * @param token[in] the returned value from defer_exec for the deferred execution you wish to cancel
 * @return true if the token was cancelled successfully, otherwise false
 */
static bool cancel_deferred_exec_advanced(deferred_token token) {
    // We can determine the index in the table from the token
    uint8_t index = token / (MAX_TOKEN_VALUE / MAX_DEFERRED_EXECUTORS);
    {
        deferred_executor_t *entry = &basic_executors[index];
        if (entry->token == token) {
            // Found it, cancel and clear the table entry
            entry->callback     = NULL;
            entry->cb_arg       = NULL;
            return true;
        }
    }

    // Not found
    return false;
}

/**
 * Configures the supplied deferred executor to be executed after the required number of milliseconds.
 *
 * @param table[in] the custom table used for storage
 * @param table_count[in] the number of available items in the table
 * @param delay_ms[in] the number of milliseconds before executing the callback
 * @param callback[in] the executor to invoke
 * @param cb_arg[in] the argument to pass to the executor, may be NULL if unused by the executor
 * @return a token usable for extension/cancellation, or INVALID_DEFERRED_TOKEN if an error occurred
 */
static void deferred_exec_advanced_task(void) {
    uint32_t now = timer_read32();

    // Throttle only once per millisecond
    if (((int32_t)TIMER_DIFF_32(now, (last_deferred_exec_check))) > 0) {
        last_deferred_exec_check = now;

        // Run through each of the executors
        free_executors_t free = basic_executors_free;
        uint8_t          i    = 0;
        while (free != 0) {
            if ((free & 1) == 1) {
                deferred_executor_t *entry = &basic_executors[i];

                // Check if we're supposed to execute this entry
                if (entry->token != INVALID_DEFERRED_TOKEN && ((int32_t)TIMER_DIFF_32(entry->trigger_time, now)) <= 0) {
                    // Invoke the callback and work work out if we should be requeued
                    uint32_t delay_ms = entry->callback(entry->trigger_time, entry->cb_arg);

                    // Update the trigger time if we have to repeat, otherwise clear it out
                    if (delay_ms > 0) {
                        // Intentionally add just the delay to the existing trigger time -- this ensures the next
                        // invocation is with respect to the previous trigger, rather than when it got to execution. Under
                        // normal circumstances this won't cause issue, but if another executor is invoked that takes a
                        // considerable length of time, then this ensures best-effort timing between invocations.
                        entry->trigger_time += delay_ms;
                    } else {
                        // If it was zero, then the callback is cancelling repeated execution. Free up the slot.
                        basic_executors_free &= ~(1 << i);
                        entry->callback     = NULL;
                        entry->cb_arg       = NULL;
                    }
                }
            }
            free = free >> 1;
            i += 1;
        }
    }
}

//------------------------------------
// Basic API: used by user-mode code, guaranteed to not collide with core deferred execution
//

deferred_token defer_exec(uint32_t delay_ms, deferred_exec_callback callback, void *cb_arg) {
    return defer_exec_advanced(delay_ms, callback, cb_arg);
}
bool extend_deferred_exec(deferred_token token, uint32_t delay_ms) {
    return extend_deferred_exec_advanced(token, delay_ms);
}
bool cancel_deferred_exec(deferred_token token) {
    return cancel_deferred_exec_advanced(token);
}
void deferred_exec_task(void) {
    deferred_exec_advanced_task();
}
