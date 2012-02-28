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
#include <bps/navigator.h>
#include <bps/netstatus.h>
#include <stdio.h>
#include <stdlib.h>

#include "dialogutil.h"

/**
 * Print the network status.
 *
 * @param is_available boolean indicating the networks status to be displayed
 * with true being network available and false network unavailable.
 */
void
display_net_status(bool is_available)
{
    /*
     * Print the network status
     */
    if(is_available) {
        show_dialog_message("Network: Available");
    } else {
        show_dialog_message("Network: Unavailable");
    }
}

/**
 * A sample application demonstrates the BlackBerry Native APIs for network status.
 * The sample queries for the current network status and then listens for status
 * update events.
 */
int
main(int argc, char *argv[])
{
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
     * once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry Tablet OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so that we can track when
     * the system is terminating the application (NAV_EXIT event) as well as
     * Network Status service events so we can be notified when the network
     * status is updated
     */
    navigator_request_events(0);
    netstatus_request_events(0);
    dialog_request_events(0);

    /*
     * Create and display the dialog.
     */
    create_dialog();

    /*
     * Retrieve and display the current network status using the
     * netstatus_get_availability(...) call
     */
    bool is_available;
    netstatus_get_availability(&is_available);
    display_net_status(is_available);

   /*
    * Process Network Status and Navigator events until we receive a NAVIGATOR_EXIT
    * event.
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
            /*
             * If it is a NETSTATUS_INFO event then display the updated status
             */
            if (bps_event_get_domain(event) == netstatus_get_domain()) {
                if (NETSTATUS_INFO == bps_event_get_code(event)) {
                    is_available = netstatus_event_get_availability(event);
                    display_net_status(is_available);
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
