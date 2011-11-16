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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bps/bps.h>
#include <bps/netstatus.h>
#include <curl/curl.h>
#include <screen/screen.h>

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
    static const char *headerfilename = "head.out";
    int final_return_code = EXIT_FAILURE;

    screen_context_t screen_cxt;
    screen_window_t screen_win;
    screen_buffer_t screen_buf;
    int screen_fill_attribs[] = { SCREEN_BLIT_COLOR, COLOR_PURPLE, SCREEN_BLIT_END };
    int screen_dirty[4] = { 0, 0, 1024, 600 }; //start with sane default values

    int idle_mode = SCREEN_IDLE_MODE_KEEP_AWAKE;
    int usage = SCREEN_USAGE_NATIVE;

    /* Step 2: Libscreen Initialization and Usage
     * ==========================================
     */

    if (screen_create_context(&screen_cxt, 0) != 0)
    {
        perror("failed to create context");
        return final_return_code;
    }

    if (screen_create_window(&screen_win, screen_cxt) != 0)
    {
        perror("failed to create window");
        goto fail1;
    }

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) != 0)
    {
        perror("failed to set native usage mode");
        goto fail2;
    }

    if (screen_create_window_buffers(screen_win, 1) != 0)
    {
        perror("failed to set native usage mode");
        goto fail2;
    }

    if(screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf) != 0)
    {
        perror("failed to get screen buffer");
        goto fail2;
    }

    if (screen_fill(screen_cxt, screen_buf, screen_fill_attribs) != 0) {
        perror("failed to fill the screen");
        goto fail3;
    }

    if (screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, screen_dirty+2) != 0) {
        perror("failed to get window size");
        goto fail3;
    }

    if (screen_post_window(screen_win, screen_buf, 1, screen_dirty, 0) != 0) {
        perror("failed to post the window");
        goto fail3;
    }

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_IDLE_MODE, &idle_mode) != 0)
    {
        perror("failed to set idle mode");
        goto fail3;
    }

    /* Step 3: BPS Net Status Initialization and Usage
     * ===============================================
     */


    if (bps_initialize() == BPS_FAILURE) {
        goto fail3;
    }


    if (netstatus_get_proxy_details(&details) == BPS_FAILURE) {
        bps_shutdown();
        goto fail3;
    }

    /* Step 4: Curl Initialization and Usage
     * =====================================
     */

    /* init the curl session */
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        goto fail4;
    }

    /* set URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, "http://example.com");

    /* if proxy is required, then set proxy */
    if (details.http_proxy_host == NULL) {
        fprintf (stderr, "No proxy required!\n");
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
        goto fail4;
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

    fail4:
    /* cleanup netstatus proxy details */
    netstatus_free_proxy_details(&details);

    /* cleanup bps stuff */
    bps_shutdown();

    fail3:
    screen_destroy_buffer(screen_buf);
    fail2:
    screen_destroy_window(screen_win);
    fail1:
    screen_destroy_context(screen_cxt);

    return final_return_code;

}
