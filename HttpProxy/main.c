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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/navigator.h>
#include <bps/netstatus.h>
#include <curl/curl.h>
#include <screen/screen.h>

#include "dialogutil.h"



static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

const unsigned int COLOR_PURPLE = 0xffff00ff;

int main(void)
{

    /* Step 1: Variable Declarations and Initializations
     * =================================================
     */
    netstatus_proxy_details_t details;
    memset(&details, 0, sizeof(details));
    CURL *curl_handle = NULL;
    FILE *headerfile = NULL;
    static const char *headerfilename = "app/native/head.out";
    int final_return_code = EXIT_FAILURE;
    int exit_application = 0;
    bool proxyWasNotRequired = false;

    /* Step 2: BPS Net Status Initialization and Usage
     * ===============================================
     */
    if (bps_initialize() == BPS_FAILURE) {
        return 0;
    }

    if (netstatus_get_proxy_details(&details) == BPS_FAILURE) {
        bps_shutdown();
        return 0;
    }

    /* Step 3: Libscreen Initialization and Usage
     * ==========================================
     */
    if (setup_screen() != EXIT_SUCCESS) {
        printf("Unable to set up the screen. Exiting.");
        return 0;
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry(R) 10 OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     * For this sample, we request Navigator events so we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event). This allows
     * us to clean up application resources.
     */
    navigator_request_events(0);
    dialog_request_events(0);

    /*
     * Create and display the dialog on the screen.
     */
    create_dialog();


    /* Step 4: Curl Initialization and Usage
     * =====================================
     */

    /* init the curl session */
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        goto fail;
    }

    /* set URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, "http://example.com");

    /* if proxy is required, then set proxy */
    if (details.http_proxy_host == NULL) {
        proxyWasNotRequired = true;
        show_dialog_message("No proxy required!\n");
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_PROXY, details.http_proxy_host);
        /* if we have a valid port number then let libcurl know what it is */
        if (details.http_proxy_port != 0)
            curl_easy_setopt(curl_handle, CURLOPT_PROXYPORT, (long)details.http_proxy_port);
    }

    /* if proxy user name is available, then set the user name */
    if (details.http_proxy_login_user != NULL)
        curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERNAME, details.http_proxy_login_user);
    /* if proxy password is available, then set the password */
    if (details.http_proxy_login_password != NULL)
        curl_easy_setopt(curl_handle, CURLOPT_PROXYPASSWORD, details.http_proxy_login_password);

    /* no progress meter, please */
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

    /* open the files */
    headerfile = fopen(headerfilename,"w");
    if (headerfile == NULL) {
        curl_easy_cleanup(curl_handle);
        goto fail;
    }

    /* we want the headers written to this file handle to confirm that
     * it went through a proxy
     */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, headerfile);

    /*
     * Notice here that if you want the actual data sent anywhere else but
     * stdout, you should consider using the CURLOPT_WRITEDATA option.
     */

    /* get it! */
    curl_easy_perform(curl_handle);

    /* Step 5: Tear Down and Clean Up
     * ==============================
     */

    /* close the header file */
    fclose(headerfile);

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    final_return_code = EXIT_SUCCESS;

    fail:
    if (proxyWasNotRequired) {
        if (final_return_code == EXIT_SUCCESS) {
            show_dialog_message("No proxy required!\nSUCCESS!!");
        } else {
            show_dialog_message("No proxy required!\nFAILED!!");
        }
    } else {
        if (final_return_code == EXIT_SUCCESS) {
            show_dialog_message("SUCCESS!!");
        } else {
            show_dialog_message("FAILED!!");
        }
    }

    /* cleanup netstatus proxy details */
    netstatus_free_proxy_details(&details);

    /*
     * Process Navigator events until we receive a NAVIGATOR_EXIT event.
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

    /* cleanup bps stuff */
    bps_shutdown();

    return final_return_code;

}
