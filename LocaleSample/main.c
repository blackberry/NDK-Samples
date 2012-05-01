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
#include <bps/locale.h>
#include <bps/navigator.h>
#include <stdio.h>
#include <stdlib.h>

#include "dialogutil.h"
/*
 * buffer to store messages that we will display in the dialog
 */
#define MSG_SIZE 1024
static char msg[MSG_SIZE];


/**
 * Print the Locale data.
 *
 * @param language the locale language value.
 * @param country the locale country value
 */
void
display_locale(const char *language, const char *country)
{
    /*
     * Print the locale values
     */
    if(language == NULL) {
       language = "None";
    }

    if(country == NULL) {
       country = "None";
    }

    snprintf(msg, MSG_SIZE, "Language: %s\n Country: %s\n", language, country);
    show_dialog_message(msg);
}

/**
 * A sample application that demonstrates the BlackBerry(R) 10 Native SDK APIs
 * for managing locale. The sample queries for the current locale and then
 * listens for locale update events.
 */
int
main(int argc, char *argv[])
{
    /*
     * Before we can listen for events from the BlackBerry(R) 10 OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

    if (setup_screen() != EXIT_SUCCESS) {
        printf("Unable to set up the screen. Exiting.");
        return 0;
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry(R) 10 OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so that we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event) as well
     * as Locale events so we can be notified when the locale is updated
     */
    navigator_request_events(0);
    locale_request_events(0);
    dialog_request_events(0);

    /*
     * Create and display the dialog.
     */
    create_dialog();

    /*
     * Retrieve and display the current Locale using the locale_get(...) API
     */
    char* country = NULL;
    char* language = NULL;
    locale_get(&language, &country);
    display_locale(language, country);
    bps_free((char*)language);
    bps_free((char*)country);

   /*
    * Process Locale and Navigator events until we receive a NAVIGATOR_EXIT event.
    */
    int exit_application = 0;
    while (!exit_application) {
        /*
         * Using a negative timeout (-1) in the call to bps_get_event(...)
         * ensures that we don't busy wait by blocking until an event is
         * available.
         */
        bps_event_t *event = NULL;
        bps_get_event(&event, -1);

        if (event) {
            if (bps_event_get_domain(event) == locale_get_domain()) {
                /*
                 * If it is a LOCALE_INFO event then display the updated locale
                 * information
                 */
                if (LOCALE_INFO == bps_event_get_code(event)) {
                    /*
                     * The locale_event_get_language and locale_event_get_country
                     * calls return pointers to data within the event. When
                     * the event is destroyed below via the call to
                     * bps_event_destroy(event), the returned pointers would
                     * reference deallocated memory.
                     *
                     * To avoid potentially having pointers to dereferenced
                     * memory, we'll use local variables to store the pointers
                     * into the event data. This way, the pointers will go
                     * out of scope and thus cannot be used after the event
                     * is freed.
                     */
                    const char* language = locale_event_get_language(event);
                    const char* country = locale_event_get_country(event);
                    display_locale(language, country);
                }
            }

            /*
             * If it is a NAVIGATOR_EXIT event then set the exit_application
             * flag so the application will stop processing events, clean up
             * and exit
             */
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                if (NAVIGATOR_EXIT == bps_event_get_code(event)) {
                    exit_application = 1;
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
