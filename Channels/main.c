/*
 * Copyright (c) 2011-2012 Research In Motion Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Sample showing how to use BlackBerry(R) 10 Native SDK APIs for channels to
 * allow different threads to handle events from different services.
 *
 * Note that the main purpose of this sample is to show channels working
 * with services.  Please see the Accelerometer sample and the Geolocation
 * sample for a better understanding of how to use the accelerometer and
 * geolocation services.
 *
 */

#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/geolocation.h>
#include <bps/navigator.h>
#include <bps/sensor.h>
#include <pthread.h>
#include <screen/screen.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialogchannel.h"

#define MSG_SIZE 1024

/**
 * Microseconds between accelerometer reads.
 */
static const int ACCELEROMETER_RATE = 25000;

/**
 * Event code to tell the accelerometer thread to stop processing events.
 */
static const int STOP_REQUEST = 1;

/**
 * The channel ID for the channel that will handle accelerometer events.
 */
static int accel_chid = -1;

/**
 * A place to store the requested event domain
 */
static int local_event_domain;

/**
 * a buffer to store messages that will be passed to the dialog.
 */
static char msg[MSG_SIZE];

pthread_mutex_t chidMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  chidCond  = PTHREAD_COND_INITIALIZER;

/**
 * Print the Accelerometer reading.
 *
 * @param x The g-force along the x-axis.
 * @param y The g-force along the y-axis.
 * @param z The g-force along the z-axis.
 */
static void
display_accelerometer_reading(float x, float y, float z)
{
    char accel_msg[MSG_SIZE];
    /*
     * Display the accelerometer values
     */
    snprintf(accel_msg, MSG_SIZE, "\n\nx: %f, y: %f, z: %f\n", x, y, z);
    show_accelerometer_dialog_message(accel_msg);
}

/**
 * Handle a navigator event.
 *
 * @return @c true if application should exit, else false.
 */
static bool
handle_navigator_event(bps_event_t *event)
{
    bool should_exit = false;

    switch (bps_event_get_code(event)) {
    case NAVIGATOR_EXIT:
        should_exit = true;
        break;
    }

    return should_exit;
}

/**
 * Handle a geolocation response.
 *
 * @param event  The event containing geolocation data.
 */
static void
handle_geolocation_response(bps_event_t *event)
{
    static int count = 0;

    /*
     * Double check that the event is valid.
     */
    if (event == NULL || bps_event_get_code(event) != GEOLOCATION_INFO) {
        return;
    }

    double latitude = geolocation_event_get_latitude(event);
    double longitude = geolocation_event_get_longitude(event);
    double accuracy = geolocation_event_get_accuracy(event);

    snprintf(msg, MSG_SIZE,
             "Geolocation report #%d\n"
             "\tlatitude: % 13.8f degrees\n"
             "\tlongitude: % 13.8f degrees\n"
             "\taccuracy: % 7.3f m\n",
             count,
             latitude, longitude, accuracy);

    show_geolocation_dialog_message(msg);
    count++;
}

/**
 * Thread that handles accelerometer events and
 * sends relevant information to a dialog.
 *
 * @param p Unused.
 */
static void *
accel_main (void *p) {
    bool run = true;
    bps_event_t *event;
    float force_x, force_y, force_z;

    bps_initialize();

    /*
     * Each thread that calls bps_initialize() will have its
     * own unique channel ID.  Protect it inside a mutex and
     * condition variable to avoid race condition where main
     * thread tries to use it before we assign it.
     */
    pthread_mutex_lock(&chidMutex);
    accel_chid = bps_channel_get_active();
    pthread_cond_signal(&chidCond);
    pthread_mutex_unlock(&chidMutex);

    /*
     * Create and display a dialog that will show the data.
     */
    create_accelerometer_dialog();
    show_accelerometer_dialog_message("\n\nThis is the Accelerometer Dialog");

    if (BPS_SUCCESS != sensor_request_events(SENSOR_TYPE_ACCELEROMETER)) {
        fprintf(stderr, "Error requesting sensor's accelerometer events: %s", strerror(errno));
        bps_shutdown();
        return NULL;
    }

    sensor_set_rate(SENSOR_TYPE_ACCELEROMETER, ACCELEROMETER_RATE);
    sensor_set_skip_duplicates(SENSOR_TYPE_ACCELEROMETER, true);

    while (run) {
        /*
         * Block, at the very least we'll get the "STOP" event
         */
        bps_get_event(&event, -1);
        if (bps_event_get_domain(event) == local_event_domain) {
            if (bps_event_get_code(event) == STOP_REQUEST) {
                run = false;
            }
        }

        if (bps_event_get_domain(event) == sensor_get_domain()) {
            if (SENSOR_ACCELEROMETER_READING == bps_event_get_code(event)) {
                sensor_event_get_xyz(event, &force_x, &force_y, &force_z);
                display_accelerometer_reading(force_x, force_y, force_z);
            }
        }
    }

    sensor_stop_events(0);
    destroy_accelerometer_dialog();

    bps_shutdown();

    fprintf(stderr, "Exiting accelerometer thread\n");
    return NULL;


}

/**
 * The main entry point.
 */
int
main(int argc, char *argv[])
{
    pthread_t accel_thread;

    bool exit_application = false;
    int rc;

    /*
     * Before we can listen for events from the BlackBerry(R) 10 OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

    /*
     * Initialize the screen so that the window group Id is properly set,
     * to allow the dialogs to be displayed.
     */
    if (setup_screen() != EXIT_SUCCESS) {
        fprintf(stderr, "Unable to initialize screen.");
        exit(-1);
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry(R) 10 OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     *
     * We register a custom event domain so that we can communicate with the
     * the accelerometer thread.  We will need to tell it to quit once we get
     * the NAVIGATOR_EXIT.
     *
     * We request Navigator events so that we can track when the system is
     * terminating the application (NAVIGATOR_EXIT event).
     *
     */
    local_event_domain = bps_register_domain();
    if (local_event_domain == -1) {
        fprintf(stderr, "Error registering custom event domain: %s", strerror(errno));
        exit(-1);
    }
    if (BPS_SUCCESS != navigator_request_events(0)) {
        fprintf(stderr, "Error requesting navigator events: %s", strerror(errno));
        exit(-1);
    }
    if (BPS_SUCCESS != dialog_request_events(0)) {
        fprintf(stderr, "Error requesting dialog events: %s", strerror(errno));
        exit(-1);
    }
    if (BPS_SUCCESS != geolocation_request_events(0)) {
        fprintf(stderr, "Error requesting geolocation events: %s", strerror(errno));
        exit(-1);
    }

    geolocation_set_period(1);

    /*
     * Create and display the dialogs that will show the data.
     */
    create_geolocation_dialog();
    show_geolocation_dialog_message("Geolocation getting first fix");

    /*
     * Before initializing the accelerometer service we must ensure the device
     * supports it.
     */
    if (!sensor_is_supported(SENSOR_TYPE_ACCELEROMETER)) {
        /**
         * If the device does not support accelerometer then notify the user,
         * clean up and exit
         */
        snprintf(msg, MSG_SIZE, "Accelerometer not supported by device!");
        show_geolocation_dialog_message(msg);

        /*
         * Destroy the dialog, if it exists and cleanup screen resources.
         */
        destroy_geolocation_dialog();
        cleanup_screen();
        bps_shutdown();
        return EXIT_FAILURE;
    }

    /*
     * Create the accelerometer event thread.
     */
    rc = pthread_create(&accel_thread, NULL, accel_main, NULL);
    if (rc != 0) {
        fprintf(stderr, "Error in pthread_create: %s", strerror(errno));
        exit(-1);
    }

    while (!exit_application) {
        /*
         * Using a negative timeout (-1) in the call to bps_get_event(...)
         * ensures that we don't busy wait by blocking until an event is
         * available.
         */
        bps_event_t *event = NULL;
        bps_get_event(&event, -1);

        if (event) {
            if (bps_event_get_domain(event) == geolocation_get_domain()) {
                handle_geolocation_response(event);
            }
            else if (bps_event_get_domain(event) == navigator_get_domain()) {
                exit_application = handle_navigator_event(event);
            }
        }
    }

    geolocation_stop_events(0);

    /*
     * Avoid a possible race condition where accel_chid has not yet
     * been assigned a valid channel ID.
     */
    pthread_mutex_lock(&chidMutex);
    while (accel_chid == -1) {
        pthread_cond_wait(&chidCond, &chidMutex);
    }
    pthread_mutex_unlock(&chidMutex);

    bps_event_t *stop_request_event = NULL;

    if (BPS_SUCCESS != bps_event_create(&stop_request_event, local_event_domain, STOP_REQUEST, NULL, NULL)) {
        fprintf(stderr, "Unable to create event: %s", strerror(errno));
        exit(-1);
    }

    if (BPS_SUCCESS != bps_channel_push_event(accel_chid, stop_request_event)) {
        fprintf(stderr, "Unable to push event: %s", strerror(errno));
    }

    pthread_join(accel_thread, NULL);

    /*
     * Destroy the dialog, if it exists.
     */
    destroy_geolocation_dialog();

    bps_shutdown();
    cleanup_screen();

    return 0;
}
