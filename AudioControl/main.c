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
#include <bps/audiomixer.h>
#include <bps/dialog.h>
#include <bps/navigator.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialogutil.h"

/*
 * buffer to store messages that we will display in the dialog
 */
#define MSG_SIZE 1024
static char msg[MSG_SIZE];

/**
 * Converts the specified boolean value to the corresponding English string
 * "True" or "False"
 *
 * @param b the boolean value to convert to string
 *
 * @return string representing the English encoding of the boolean value
 */
static const char*
bool_str(bool b) {
    return b ? "True" : "False";
}

/**
 * Retrieves the output channel name for the specified channel ID
 *
 * @param output the output channel ID
 *
 * @return string representing the output channel name or "Unknown" if no
 * corresponding channel ID is found
 */
static const char*
get_channel_name(audiomixer_output_t output) {
    if (output == AUDIOMIXER_OUTPUT_SPEAKER)
        return "Speaker";
    if (output == AUDIOMIXER_OUTPUT_HEADPHONE)
        return "Headphones";

    return "Unknown";
}

/**
 * Output the updated audio mixer status indicated in the event
 *
 * @param event the AUDIOMIXER_INFO bps_event_t event to containing the updated audio
 * mixer status
 */
static void
print_audio_event(bps_event_t *event)
{
    char tmp[MSG_SIZE];
    snprintf(msg, MSG_SIZE, "Audio Mixer Event\n");

    snprintf(tmp, MSG_SIZE, "Headphone Volume: %f\n", audiomixer_event_get_output_level(event, AUDIOMIXER_OUTPUT_HEADPHONE));
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Speaker Volume: %f\n", audiomixer_event_get_output_level(event, AUDIOMIXER_OUTPUT_SPEAKER));
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Input Gain: %f\n", audiomixer_event_get_input_level(event, AUDIOMIXER_INPUT));
    strlcat(msg, tmp, MSG_SIZE);

    snprintf(tmp, MSG_SIZE, "Headphone Mute: %s\n", bool_str(audiomixer_event_get_output_mute(event, AUDIOMIXER_OUTPUT_HEADPHONE)));
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Speaker Mute: %s\n", bool_str(audiomixer_event_get_output_mute(event, AUDIOMIXER_OUTPUT_SPEAKER)));
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Input Mute: %s\n", bool_str(audiomixer_event_get_input_mute(event, AUDIOMIXER_INPUT)));
    strlcat(msg, tmp, MSG_SIZE);

    snprintf(tmp, MSG_SIZE, "Available Output Channel: %s\n\n", get_channel_name(audiomixer_event_get_available(event)));
    strlcat(msg, tmp, MSG_SIZE);
    show_dialog_message(msg);
}


/**
 * A sample application that demonstrates the BlackBerry(R) 10 Native SDK APIs
 * for managing audio control. The sample queries for the current speaker output
 * level, sets the speaker output level and then waits for updates to any audio
 * levels.
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
        fprintf(stderr, "Unable to set up the screen. Exiting.");
        bps_shutdown();
        return 0;
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry(R) 10 OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so that we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event) as well
     * as Dialog events so we can show a dialog and Audio Mixer events so we can
     * be notified when audio values are updated.
     */
    if (BPS_FAILURE == navigator_request_events(0)) {
        fprintf(stderr, "Unable to request navigator events. Exiting.");
        cleanup_screen();
        bps_shutdown();
        return 0;
    }
    if (BPS_FAILURE == dialog_request_events(0)) {
        fprintf(stderr, "Unable to request dialog events. Exiting.");
        cleanup_screen();
        bps_shutdown();
        return 0;
    }
    /*
     * Create and display the dialog.
     */
    create_dialog();

    if (BPS_FAILURE == audiomixer_request_events(0)) {
        show_dialog_message("Unable to request audio mixer events.");
    } else {
        /*
         * Retrieve and display the current audio mixer status
         */
        float headphone_volume;
        audiomixer_get_output_level(AUDIOMIXER_OUTPUT_HEADPHONE, &headphone_volume);
        float speaker_volume;
        audiomixer_get_output_level(AUDIOMIXER_OUTPUT_SPEAKER, &speaker_volume);
        float input_gain;
        audiomixer_get_input_level(AUDIOMIXER_INPUT, &input_gain);

        bool is_headphone_muted;
        audiomixer_get_output_mute(AUDIOMIXER_OUTPUT_HEADPHONE, &is_headphone_muted);
        bool is_speaker_muted;
        audiomixer_get_output_mute(AUDIOMIXER_OUTPUT_SPEAKER, &is_speaker_muted);
        bool is_input_muted;
        audiomixer_get_input_mute(AUDIOMIXER_INPUT, &is_input_muted);

        char tmp[MSG_SIZE];
        snprintf(msg, MSG_SIZE, "Audio Mixer Status\n");
        snprintf(tmp, MSG_SIZE, "Headphone Volume: %f\n", headphone_volume);
        strlcat(msg, tmp, MSG_SIZE);
        snprintf(tmp, MSG_SIZE, "Speaker Volume: %f\n", speaker_volume);
        strlcat(msg, tmp, MSG_SIZE);
        snprintf(tmp, MSG_SIZE, "Input Gain: %f\n", input_gain);
        strlcat(msg, tmp, MSG_SIZE);
        snprintf(tmp, MSG_SIZE, "Headphone Muted: %s\n", bool_str(is_headphone_muted));
        strlcat(msg, tmp, MSG_SIZE);
        snprintf(tmp, MSG_SIZE, "Speaker Muted: %s\n", bool_str(is_speaker_muted));
        strlcat(msg, tmp, MSG_SIZE);
        snprintf(tmp, MSG_SIZE, "Input Muted: %s\n\n", bool_str(is_input_muted));
        strlcat(msg, tmp, MSG_SIZE);
        show_dialog_message(msg);
    }

    /*
     * Process Audio Mixer and Navigator events until we receive a NAVIGATOR_EXIT
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
            int domain = bps_event_get_domain(event);

            /*
             * If it is an AUDIOMIXER_INFO event then display the updated audio mixer
             * information
             */
            if (audiomixer_get_domain() == domain) {
                if (AUDIOMIXER_INFO == bps_event_get_code(event)) {
                    print_audio_event(event);
                }
            }
            /*
             * If it is a NAVIGATOR_EXIT event then set the exit_application
             * flag so the application will stop processing events, clean up
             * and exit
             */
            if (navigator_get_domain() == domain) {
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
     * Clean up BPS infrastructure and exit
     */
    bps_shutdown();
    return 0;
}
