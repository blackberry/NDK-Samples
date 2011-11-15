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

#include <bps/accelerometer.h>
#include <bps/bps.h>
#include <bps/navigator.h>
#include <screen/screen.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Milliseconds between accelerometer reads. This is the same
 * rate at which the accelerometer data will be updated from
 * hardware. The hardware update rate is set below using
 * accelerometer_set_update_frequency(FREQ_40_HZ).
 */
static const int ACCELEROMETER_MAX_POLL_INTERVAL = 25;

/**
 * Print the Accelerometer reading.
 *
 * @param pitch the pitch value in degrees.
 * @param roll the roll value in degrees.
 */
void
display_accelerometer_reading(double pitch, double roll)
{
    /*
     * Display the accelerometer values
     */
    printf("Pitch: %f deg  Roll: %f deg\n", pitch, roll);
}

static screen_context_t screen_ctx;
static screen_window_t screen_win;

/**
 * Set up a basic screen, so that the navigator will
 * send window state events when the window state changes.
 *
 * @return @c EXIT_SUCCESS or @c EXIT_FAILURE
 */
int
setup_screen()
{
    if (screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        return EXIT_FAILURE;
    }

    if (screen_create_window(&screen_win, screen_ctx) != 0) {
        screen_destroy_context(screen_ctx);
        return EXIT_FAILURE;
    }

    int usage = SCREEN_USAGE_NATIVE;
    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) != 0) goto fail;

    if (screen_create_window_buffers(screen_win, 1) != 0) goto fail;

    screen_buffer_t buff;
    if (screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void*)&buff) != 0) goto fail;

    int buffer_size[2];
    if (screen_get_buffer_property_iv(buff, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size) != 0) goto fail;

    int attribs[1] = {SCREEN_BLIT_END};
    if (screen_fill(screen_ctx, buff, attribs) != 0) goto fail;

    int dirty_rects[4] = {0, 0, buffer_size[0], buffer_size[1]};
    if (screen_post_window(screen_win, buff, 1, (const int*)dirty_rects, 0) != 0) goto fail;

    return EXIT_SUCCESS;

fail:
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    return EXIT_FAILURE;
}


/**
 * A sample application demonstrates the BlackBerry Native APIs for accelerometer.
 * The sample initializes and reads the accelerometer periodically until a
 * NAVIGATOR_EXIT event is received.
 * The application also listens for window state changes from the navigator so that
 * it can stop reading the accelerometer when the application is no longer visible.
 */
int
main(int argc, char *argv[])
{
    if (setup_screen() != EXIT_SUCCESS) {
        printf("Unable to set up the screen. Exiting.");
        return 0;
    }

    /*
     * The accelerometer forces
     */
    double force_x, force_y, force_z;

    /*
     * Before we can listen for events from the BlackBerry Tablet OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

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

    /*
     * Before initializing the accelerometer service we must ensure the device
     * supports it
     */
    if (!accelerometer_is_supported()) {
        /*
         * If the device does not support accelerometer then notify the user,
         * clean up and exit
         */
        printf("Accelerometer not supported by device!");
        bps_shutdown();
        return EXIT_FAILURE;
    }

    /*
     * Initialize the accelerometer by setting the rates at which the
     * accelerometer values will be updated from hardware
     */
    accelerometer_set_update_frequency(FREQ_40_HZ);

   /*
    * Process Navigator events and take accelerometer readings periodically
    * until we receive a NAVIGATOR_EXIT event.
    */
    int exit_application = 0;
    int paused = 0;
    while (!exit_application) {
        /*
         * By setting the bps_get_event timeout to ACCELEROMETER_MAX_POLL_INTERVAL,
         * we assign the maximum time (in millis) that we will wait before
         * unblocking so we can take an accelerometer reading.
         */
        bps_event_t *event = NULL;
        bps_get_event(&event, ACCELEROMETER_MAX_POLL_INTERVAL);

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
                        printf("Resuming accelerometer reads.\n");
                    } else if (!paused){
                        /*
                         * The application has become thumbnailed or invisible.
                         * If it is not already paused, then pause it.
                         * Otherwise, ignore the window state change.
                         */
                        paused = 1;
                        printf("Resuming accelerometer reads.\n");
                    }
                }
            }
            /*
             * Remember to destroy any delivered events as we need to trigger
             * the release of any event resources when we are done processing
             * it.
             */
            bps_event_destroy(event);
        } else {
            /*
             * We've woken up. See if we are in the paused state. If not,
             * take an accelerometer reading
             */
            if (!paused) {
                accelerometer_read_forces(&force_x, &force_y, &force_z);
                display_accelerometer_reading(
                        ACCELEROMETER_CALCULATE_PITCH(force_x, force_y, force_z),
                        ACCELEROMETER_CALCULATE_ROLL(force_x, force_y, force_z));
            }
        }
    }

    /*
     * Clean up screen resources
     */
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);

    /*
     * Clean up the BPS infrastructure and exit
     */
    bps_shutdown();
    return 0;
}
