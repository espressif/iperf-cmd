/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * NOTE: This file is a stub for esp_timer functions for Linux host to pass the build.
 * This file is added to pass build for the preview target linux and should not be used in production code.
 * ESP-IDF may implement esp_timer for linux in the future.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/queue.h>
#include <pthread.h>

#if !DEBUG_TEST
#include "esp_timer.h"
#endif

/* debug this file on linux host: gcc -g -Wall -fsanitize=address -o a.out iperf/esp_timer_stub.c -DDEBUG_TEST */
#if DEBUG_TEST
typedef struct esp_timer* esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void* arg);
typedef struct {
    esp_timer_cb_t callback;        //!< Callback function to execute when timer expires
    void* arg;                      //!< Argument to pass to callback
    // esp_timer_dispatch_t dispatch_method;   //!< Dispatch callback from task or ISR; if not specified, esp_timer task
    //                                !< is used; for ISR to work, also set Kconfig option
    //                                !< `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD`
    const char* name;               //!< Timer name, used in esp_timer_dump() function
    bool skip_unhandled_events;     //!< Setting to skip unhandled events in light sleep for periodic timers
} esp_timer_create_args_t;
#define esp_err_t int
#define ESP_OK 0
#define ESP_FAIL 1
#endif



typedef enum {
    FL_ISR_DISPATCH_METHOD   = (1 << 0),  //!< 0=Callback is called from timer task, 1=Callback is called from timer ISR
    FL_SKIP_UNHANDLED_EVENTS = (1 << 1),  //!< 0=NOT skip unhandled events for periodic timers, 1=Skip unhandled events for periodic timers
} flags_t;


struct esp_timer {
    timer_t timerid;
    uint64_t period: 56;
    flags_t flags: 8;
    union {
        esp_timer_cb_t callback;
        uint32_t event_id;
    };
    void* arg;
    LIST_ENTRY(esp_timer) entries;
};


#define CLOCKID CLOCK_MONOTONIC
#define SIG SIGRTMIN

static struct sigaction s_sigaction = {0};
static bool s_sig_created = false;

LIST_HEAD(timer_list, esp_timer);
static struct timer_list timer_list_head;
static bool s_timer_list_inited = false;
static pthread_mutex_t* p_timer_lock;


static void s_sig_handler(int sig, siginfo_t *si, void *uc)
{
    /* Note: calling printf() from a signal handler is not safe
        (and should not be done in production programs), since
        printf() is not async-signal-safe; see signal-safety(7).
        Nevertheless, we use printf() here as a simple way of
        showing that the handler was called. */

    timer_t *tidp = si->si_value.sival_ptr;
    pthread_mutex_lock(p_timer_lock);
    struct esp_timer *ptimer;
    LIST_FOREACH(ptimer, &timer_list_head, entries) {
        if (ptimer->timerid == *tidp) {
            if (ptimer->callback != NULL) {
                ptimer->callback(ptimer->arg);
            }
            break;
        }
    }
    pthread_mutex_unlock(p_timer_lock);
}


/* use weak attribute for all esp_timer functions, esp-idf may support esp_timer for linux in the future */
__attribute__((weak)) esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle)
{
    if (s_sig_created == false)
    {
        s_sigaction.sa_flags = SA_SIGINFO;
        s_sigaction.sa_sigaction = s_sig_handler;
        if (sigaction(SIG, &s_sigaction, NULL) == -1) {
            return ESP_FAIL;
        }
        s_sig_created = true;
    }
    if (s_timer_list_inited == false)
    {
        LIST_INIT(&timer_list_head);
        s_timer_list_inited = true;
    }
    if (p_timer_lock == NULL) {
        p_timer_lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        if (p_timer_lock == NULL) {
            printf("Failed to create mutex");
            return ESP_FAIL;
        }
        if (pthread_mutex_init(p_timer_lock, NULL) != 0) {
            printf("\n mutex init has failed\n");
            free(p_timer_lock);
            p_timer_lock = NULL;
            return ESP_FAIL;
        }
    }

    struct esp_timer* new_timer = (struct esp_timer*) calloc(sizeof(struct esp_timer), 1);
    if (new_timer == NULL) {
        printf("Failed to malloc timer\n");
        return ESP_FAIL;
    }
    *out_handle = new_timer;
    new_timer->callback = create_args->callback;
    new_timer->arg = create_args->arg;

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIG;
    sev.sigev_value.sival_ptr = &(new_timer->timerid);
    if( timer_create(CLOCKID, &sev, &(new_timer->timerid)) == -1) {
        printf("timer_create failed\n");
        free(new_timer);
        *out_handle = NULL;
        return ESP_FAIL;
    }

    pthread_mutex_lock(p_timer_lock);
    LIST_INSERT_HEAD(&timer_list_head, new_timer, entries);
    pthread_mutex_unlock(p_timer_lock);

    return ESP_OK;
}


__attribute__((weak)) esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period)
{
    if (timer == NULL) {
        printf("Invalid Handler!\n");
        return ESP_FAIL;
    }
    timer_t timerid = timer->timerid;
    struct itimerspec  its;
    /* unit of period is us */
    its.it_value.tv_sec = period / 1000000;
    its.it_value.tv_nsec = (period % 1000000) * 1000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        printf("timer_settime failed\n");
        return ESP_FAIL;
    }
    return ESP_OK;

}

__attribute__((weak)) esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    if (timer == NULL) {
        printf("Invalid Handler!\n");
        return ESP_FAIL;
    }
    uint64_t period = 0xFFFFFFFFFFFFFF;
    return esp_timer_start_periodic(timer, period);
}


__attribute__((weak)) esp_err_t esp_timer_delete(esp_timer_handle_t timer)
{
    if (timer == NULL) {
        printf("Invalid Handler!\n");
        return ESP_FAIL;
    }
    pthread_mutex_lock(p_timer_lock);
    struct esp_timer *ptimer;
    LIST_FOREACH(ptimer, &timer_list_head, entries) {
        if (ptimer->timerid == timer->timerid) {
            LIST_REMOVE(ptimer, entries);
            free(ptimer);
            break;
        }
    }
    pthread_mutex_unlock(p_timer_lock);
    return ESP_OK;
}


#if DEBUG_TEST
void my_callback1(void* arg)
{
    printf("Timer expired 1 \n");
}
void my_callback2(void* arg)
{
    printf("Timer expired 2 \n");
}
int main()
{
    #include <unistd.h>
    esp_timer_handle_t timer;
    esp_timer_create_args_t args;
    args.callback = my_callback1;
    args.arg = NULL;
    args.name = "test";
    esp_timer_create(&args, &timer);
    esp_timer_start_periodic(timer, 100000); /* 100 ms */


    esp_timer_handle_t timer2;
    args.callback = my_callback2;
    args.arg = NULL;
    args.name = "test2";
    esp_timer_create(&args, &timer2);
    esp_timer_start_periodic(timer2, 50000); /* 50 ms */

    sleep(10); /* will be interrupted by signal */
    sleep(10);
    sleep(10);
    sleep(10);
    sleep(10);
    esp_timer_stop(timer);
    esp_timer_delete(timer);
    esp_timer_stop(timer2);
    esp_timer_delete(timer2);
}
#endif
