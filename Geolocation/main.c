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


#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/geolocation.h>
#include <bps/navigator.h>
#include <screen/screen.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "dialogutil.h"

#define MSG_SIZE 1024
static char msg[MSG_SIZE];

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
 * Convert a heading in degrees to a bearing.
 */
static const char *
get_bearing(double heading)
{
    static const char * cardinals[] = {
        "N", "NE",
        "E", "SE",
        "S", "SW",
        "W", "NW",
    };
    static const int num_cardinals = 8;

    double adjusted_heading = heading + (360.0/num_cardinals/2);
    int index = (int)(adjusted_heading / 360.0 * num_cardinals) % num_cardinals;

    return cardinals[index];
}

/**
 * Formats double data into a buffer.
 */
static char *
format_double_data(char * buf, size_t buf_size, const char * desc, const char * units, double data, bool valid)
{
    if (valid) {
        snprintf(buf, buf_size, "%s: % 7.3f %s", desc, data, units);
    } else {
        snprintf(buf, buf_size, "%s: N/A", desc);
    }

    return buf;
}

/**
 * Formats int data into a buffer.
 */
static char *
format_int_data(char * buf, size_t buf_size, const char * desc, const char * units, int data, bool valid)
{
    if (valid) {
        snprintf(buf, buf_size, "%s: % d %s", desc, data, units);
    } else {
        snprintf(buf, buf_size, "%s: N/A", desc);
    }

    return buf;
}

/**
 * Displays geolocation data in the main dialog.
 */
static void
display_geolocation_data(int count,
                         double latitude, double longitude, double accuracy,
                         double altitude, bool altitude_valid,
                         double altitude_accuracy, bool altitude_accuracy_valid,
                         double heading, bool heading_valid,
                         double speed, bool speed_valid,
                         int num_satellites_used, bool num_satellites_valid)
{

    char altitude_buf[128];
    char altitude_accuracy_buf[128];
    char heading_buf[128];
    char speed_buf[128];
    char num_satellites_used_buf[128];

    snprintf(msg, MSG_SIZE,
             "Geolocation report #%d\n"
             "\tlatitude: % 13.8f degrees\n"
             "\tlongitude: % 13.8f degrees\n"
             "\taccuracy: % 7.3f m\n"
             "\t%s\n"
             "\t%s\n"
             "\t%s\n"
             "\t%s\n"
             "\t%s\n",
             count,
             latitude, longitude, accuracy,
             format_double_data(altitude_buf, sizeof altitude_buf, "altitude", "m",
                                altitude, altitude_valid),
             format_double_data(altitude_accuracy_buf, sizeof altitude_accuracy_buf, "altitude accuracy", "m",
                                altitude_accuracy, altitude_accuracy_valid),
             format_double_data(heading_buf, sizeof heading_buf, "heading", get_bearing(heading),
                                heading, heading_valid),
             /* Speed is reported in m/s.  To convert to km/h, use the formula:
              *   1 m/s = 60*60/1000 km/h = 3.6 km/h
              */
             format_double_data(speed_buf, sizeof speed_buf, "speed", "km/h",
                                speed*3.6, speed_valid),
             format_int_data(num_satellites_used_buf, sizeof num_satellites_used_buf, "number of satellites used", "",
                             num_satellites_used, num_satellites_valid)
        );

    show_dialog_message(msg);
}

/**
 * Handle a geolocation response.
 */
static void
handle_geolocation_response(bps_event_t *event)
{
    static int count = 0;

    /* Double check that the event is valid */
    if (event == NULL || bps_event_get_code(event) != GEOLOCATION_INFO) {
        return;
    }

    double latitude = geolocation_event_get_latitude(event);
    double longitude = geolocation_event_get_longitude(event);
    double accuracy = geolocation_event_get_accuracy(event);
    double altitude = geolocation_event_get_altitude(event);
    bool altitude_valid = geolocation_event_is_altitude_valid(event);
    double altitude_accuracy = geolocation_event_get_altitude_accuracy(event);
    bool altitude_accuracy_valid = geolocation_event_is_altitude_accuracy_valid(event);
    double heading = geolocation_event_get_heading(event);
    bool heading_valid = geolocation_event_is_heading_valid(event);
    double speed = geolocation_event_get_speed(event);
    bool speed_valid = geolocation_event_is_speed_valid(event);
    double num_satellites_used = geolocation_event_get_num_satellites_used(event);
    bool num_satellites_valid = geolocation_event_is_num_satellites_valid(event);

    display_geolocation_data(count++,
                             latitude, longitude, accuracy,
                             altitude, altitude_valid, altitude_accuracy, altitude_accuracy_valid,
                             heading, heading_valid,
                             speed, speed_valid,
                             num_satellites_used, num_satellites_valid);
}


/**
 * A sample application that demonstrates the BlackBerry(R) 10 Native SDK APIs
 * for geolocation.
 */
int
main(int argc, char *argv[])
{
    bool exit_application = false;

    /*
     * Before we can listen for events from the BlackBerry(R) 10 OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

    /*
     * Initialize the screen so that the window group Id is properly set, to allow
     * the dialogs to be displayed.
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
     * For this sample, we request Navigator events so that we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event).
     *
     * We request dialog events so we can be notified when the dialog service
     * responds to our requests/queries.
     *
     * We request geolocation events so that we can be notified of our current
     * geolocation.
     */
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

    /*
     * Get geolocation events once a second, which is the most often that they
     * are capable of being reported.
     */
    geolocation_set_period(1);

    /*
     * Create and display the dialog that will show the geolocation data.
     */
    create_dialog();
    show_dialog_message("Geolocation getting first fix");

    /*
     * Process Geolocation, Dialog and Navigator events until we receive a
     * NAVIGATOR_EXIT event.
     */
    while (!exit_application) {
        /*
         * Using a negative timeout (-1) in the call to bps_get_event(...)
         * ensures that we don't busy wait by blocking until an event is
         * available.
         */
        bps_event_t *event = NULL;
        bps_get_event(&event, -1);

        if (event) {
            /*
             * If it is a geolocation event, determine the response code and
             * handle the event accordingly.
             */
            if (bps_event_get_domain(event) == geolocation_get_domain()) {
                handle_geolocation_response(event);
            }

            /*
             * If it is a dialog event, determine the response code and handle
             * the event accordingly.
             */
            else if (bps_event_get_domain(event) == dialog_get_domain()) {
                /* We have no buttons so we don't need to do anything. */
                ;
            }

            /*
             * If it is a NAVIGATOR_EXIT event then set the exit_application
             * flag so the application will stop processing events, clean up and
             * exit.
             */
            else if (bps_event_get_domain(event) == navigator_get_domain()) {
                exit_application = handle_navigator_event(event);
            }
        }
    }

    /*
     * Stop geolocation events.
     */
    geolocation_stop_events(0);

    /*
     * Destroy the dialog, if it exists.
     */
    destroy_dialog();

    /*
     * Clean up the bps infrastructure and exit
     */
    bps_shutdown();
    cleanup_screen();
    return 0;
}
