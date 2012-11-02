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
#include <screen/screen.h>
#include <stdio.h>
#include <stdlib.h>

static screen_context_t screen_ctx;
static screen_window_t screen_win;
dialog_instance_t alert_dialog = 0;


/**
 * Use the PID to set the window group id.
 */
char *
get_window_group_id()
{
    static char s_window_group_id[16] = "";

    if (s_window_group_id[0] == '\0') {
        snprintf(s_window_group_id, sizeof(s_window_group_id), "%d", getpid());
    }

    return s_window_group_id;
}

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

    if (screen_create_window_group(screen_win, get_window_group_id()) != 0) goto fail;

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
 * Show an alert dialog that has two buttons: a "Cancel" button, and a "OK" button.
 */
static void
show_alert()
{
    if (alert_dialog) {
        return;
    }

    if (dialog_create_alert(&alert_dialog) != BPS_SUCCESS) {
        fprintf(stderr, "Failed to create alert dialog.");
        return;
    }

    if (dialog_set_alert_message_text(alert_dialog, "Hello World!") != BPS_SUCCESS) {
        fprintf(stderr, "Failed to set alert dialog message text.");
        dialog_destroy(alert_dialog);
        alert_dialog = 0;
        return;
    }

    /*
     * Create a context to attach to the cancel button
     */
    char* cancel_button_context = "Cancelled";

    /*
     * Use a button label defined in bps/dialog.h. Attach a context to the button.
     */
    if (dialog_add_button(alert_dialog, DIALOG_CANCEL_LABEL, true, cancel_button_context, true) != BPS_SUCCESS) {
        fprintf(stderr, "Failed to add button to alert dialog.");
        dialog_destroy(alert_dialog);
        alert_dialog = 0;
        return;
    }

    /*
     * Use a button label of our own. Don't attach a context to the button.
     */
    if (dialog_add_button(alert_dialog, DIALOG_OK_LABEL, true, 0, true) != BPS_SUCCESS) {
        fprintf(stderr, "Failed to add button to alert dialog.");
        dialog_destroy(alert_dialog);
        alert_dialog = 0;
        return;
    }

    if (dialog_show(alert_dialog) != BPS_SUCCESS) {
        fprintf(stderr, "Failed to show alert dialog.");
        dialog_destroy(alert_dialog);
        alert_dialog = 0;
        return;
    }
}

/**
 * Handle a dialog response.
 */
static void
handle_dialog_response(bps_event_t *event)
{
    /*
     * Double check that the event is valid
     */
    if (event == NULL) {
        return;
    }

    int selectedIndex = dialog_event_get_selected_index(event);
    const char* label = dialog_event_get_selected_label(event);
    const char* context = dialog_event_get_selected_context(event);

    char output[1024];
    snprintf(output, 1024, "Selected Index: %d, Label: %s, Context: %s\n",
            selectedIndex, label?label:"n/a", context?(char*)context:"n/a");
    fprintf(stderr, output);

    dialog_destroy(alert_dialog);
    alert_dialog = 0;
}


/**
 * A sample application that demonstrates the BlackBerry(R) 10 Native SDK APIs
 * for displaying dialog windows.
 */
int
main(int argc, char *argv[])
{
    int exit_application = 0;

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
        exit(0);
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry(R) 10 OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so that we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event), and as a
     * convenient way to trigger the display of a dialog (NAVIGATOR_SWIPE_DOWN).
     * We request dialog events so we can be notified when the dialog service
     * responds to our requests/queries.
     */
    navigator_request_events(0);
    dialog_request_events(0);

    /*
     * start the application with a dialog.
     */
    show_alert();

    /*
     * Process Dialog and Navigator events until we receive a NAVIGATOR_EXIT event.
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
             * If it is a dialog event, determine the response code
             * and handle the event accordingly.
             */
            if (bps_event_get_domain(event) == dialog_get_domain()) {
                handle_dialog_response(event);
            }

            /*
             * If it is a NAVIGATOR_EXIT event then set the exit_application
             * flag so the application will stop processing events, clean up
             * and exit.
             *
             * If it is a NAVIGATOR_SWIPE_DOWN event, display an alert dialog.
             */
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                if (NAVIGATOR_EXIT == bps_event_get_code(event)) {
                    exit_application = 1;
                } else if (NAVIGATOR_SWIPE_DOWN == bps_event_get_code(event)) {
                    show_alert();
                }
            }
        }
    }

    /*
     * Destroy the dialog, if it still exists.
     */
    if (alert_dialog) {
        dialog_destroy(alert_dialog);
    }

    /*
     * Clean up the BPS infrastructure and exit
     */
    bps_shutdown();
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    return 0;
}
