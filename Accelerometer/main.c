/*
 * Copyright (c) 2011 Research In Motion Limited.
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

#include <bps/sensor.h>
#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/navigator.h>
#include <screen/screen.h>
#include <stdio.h>
#include <stdlib.h>

#include "dialogutil.h"
/*
 * buffer to store messages that we will display in the dialog
 */
#define MSG_SIZE 1024
static char msg[MSG_SIZE];

/**
 * Microseconds between accelerometer reads. This is the rate at which the
 * accelerometer data will be updated from hardware. The hardware update rate
 * is set below using sensor_set_rate.
 */
static const int ACCELEROMETER_RATE = 25000;

/**
 * Print the Accelerometer reading.
 *
 * @param x The g-force along the x-axis.
 * @param y The g-force along the y-axis.
 * @param z The g-force along the z-axis.
 */
void
display_accelerometer_reading(float x, float y, float z)
{
    /*
     * Display the accelerometer values
     */
    snprintf(msg, MSG_SIZE, "x: %f, y: %f, z: %f\n", x, y, z);
    show_dialog_message(msg);
}

/**
 * A sample application demonstrates the BlackBerry Native APIs for
 * accelerometer.  The sample initializes and reads the accelerometer
 * periodically until a NAVIGATOR_EXIT event is received.  The application also
 * listens for window state changes from the navigator so that it can stop
 * reading the accelerometer when the application is no longer visible.
 */
int
main(int argc, char *argv[])
{
    /*
     * The accelerometer forces
     */
    float force_x, force_y, force_z;

    /*
     * Before we can listen for events from the BlackBerry Tablet OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

    if (setup_screen() != EXIT_SUCCESS) {
        printf("Unable to set up the screen. Exiting.");
        return 0;
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry Tablet OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event). This allows
     * us to clean up application resources.
     */
    navigator_request_events(0);
    dialog_request_events(0);

    /*
     * Create and display the dialog.
     */
    create_dialog();

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
        show_dialog_message(msg);

        /*
         * Destroy the dialog, if it exists and cleanup screen resources.
         */
        destroy_dialog();
        cleanup_screen();
        bps_shutdown();
        return EXIT_FAILURE;
    }

    /*
     * Initialize the accelerometer by setting the rates at which the
     * accelerometer values will be updated from hardware
     */
    sensor_set_rate(SENSOR_TYPE_ACCELEROMETER, ACCELEROMETER_RATE);
    sensor_set_skip_duplicates(SENSOR_TYPE_ACCELEROMETER, true);

   /*
    * Process Navigator and accelerometer events until we receive a
    * NAVIGATOR_EXIT event.
    */
    int exit_application = 0;
    int paused = 0;
    while (!exit_application) {

        bps_event_t *event = NULL;
        bps_get_event(&event, -1);

        if (event) {
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                /*
                 * If it is a NAVIGATOR_EXIT event then set the
                 * exit_application flag so the application will stop
                 * processing events, clean up and exit.
                 *
                 * If it is a NAVIGATOR_WINDOW_STATE event, and the window
                 * state is NAVIGATOR_WINDOW_INVISIBLE, set paused to true so
                 * that we stop collecting accelerometer data. Otherwise, if
                 * we're currently paused, then set paused back to false to
                 * resume collecting accelerometer data.
                 */
                unsigned int event_code = bps_event_get_code(event);
                if (NAVIGATOR_EXIT == event_code) {
                    exit_application = 1;
                } else if (NAVIGATOR_WINDOW_STATE == event_code) {
                    if (navigator_event_get_window_state(event) == NAVIGATOR_WINDOW_FULLSCREEN) {
                        /*
                         * The application is now full screen.
                         * Resume reading the accelerometer.
                         */
                        paused = 0;
                        sensor_request_events(SENSOR_TYPE_ACCELEROMETER);
                        snprintf(msg, MSG_SIZE, "Resuming accelerometer reads.\n");
                        show_dialog_message(msg);
                    } else if (!paused){
                        /*
                         * The application has become thumbnailed or invisible.
                         * If it is not already paused, then pause it.
                         * Otherwise, ignore the window state change.
                         */
                        paused = 1;
                        sensor_stop_events(SENSOR_TYPE_ACCELEROMETER);
                        snprintf(msg, MSG_SIZE, "Pausing accelerometer reads.\n");
                        show_dialog_message(msg);
                    }
                }
            } else if (bps_event_get_domain(event) == sensor_get_domain()) {
                if (SENSOR_ACCELEROMETER_READING == bps_event_get_code(event)) {
                    /*
                     * Extract the accelerometer data from the event and
                     * display it.
                     */
                    sensor_event_get_xyz(event, &force_x, &force_y, &force_z);
                    display_accelerometer_reading(force_x, force_y, force_z);
                }
            }
        }
    }

    /*
     * Destroy the dialog, if it exists and cleanup screen resources.
     */
    destroy_dialog();
    cleanup_screen();
    /*
     * Clean up the BPS infrastructure and exit
     */
    bps_shutdown();
    return 0;
}
