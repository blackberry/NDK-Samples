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

#include <fcntl.h>
#include <math.h>
#include <stdlib.h>

#include <bps/bps.h>
#include <bps/navigator.h>
#include <bps/screen.h>

#include <mm/renderer.h>

// I/O devices
static const char *video_device_url    = "screen:?winid=videosamplewindowgroup&wingrp=videosamplewindowgroup";
static const char *audio_device_url    = "audio:default";

// Name of video context
static const char *video_context_name = "samplevideocontextname";

// Window group name
static const char *window_group_name = "videosamplewindowgroup";

/* 
 * we know the video has a resolution of 640x480 so it has an aspect ratio of
 * of approximately 1.33.  Based on these facts, we can show the video as large
 * as possible without changing the aspect ratio. 
 */ 
strm_dict_t* calculate_rect(int width, int height) {
    const int image_width = 640;
    const int image_height = 480;
    const float image_aspect = (float)image_width / (float)image_height;
    const float aspect_tolerance = 0.1;

    char buffer[16];
    strm_dict_t *dict = strm_dict_new();

    if (NULL == dict) {
        return NULL;
    }

    //fullscreen is the default.
    dict = strm_dict_set(dict, "video_dest_x", "0");
    if (NULL == dict)
        goto fail;
    dict = strm_dict_set(dict, "video_dest_y", "0");
    if (NULL == dict)
        goto fail;
    dict = strm_dict_set(dict, "video_dest_w", itoa(width, buffer, 10));
    if (NULL == dict)
        goto fail; 
    dict = strm_dict_set(dict, "video_dest_h", itoa(height, buffer, 10));
    if (NULL == dict)
        goto fail;

    float screen_aspect = (float)width/(float)height;
    if (fabs(screen_aspect - image_aspect) < aspect_tolerance) {
        //if the screen is at almost the same aspect as the video, just
        //do full screen.  Nothing to do here.  Fall through and return 
        //full screen.
    } else if (screen_aspect < image_aspect) {
        /* The screen is too tall.  We need to centre top to bottom, set the 
         * width the same as the screen's while maintaining the same aspect 
         * ratio.
         */ 
        dict = strm_dict_set(dict, "video_dest_y", itoa((height - image_height) / 2, buffer, 10));
        if (NULL == dict)
            goto fail;

        height = width / image_aspect;

        dict = strm_dict_set(dict, "video_dest_h", itoa(height, buffer, 10));
        if (NULL == dict)
            goto fail;
    } else {
        /* The screen is too wide.  We need to centre left to right, set the 
         * height the same as the screen's while maintaining the same aspect 
         * ratio.
         */
        dict = strm_dict_set(dict, "video_dest_x", itoa((width - image_width) / 2, buffer, 10));
        if (NULL == dict)
            goto fail;

        width = height * image_aspect;

        dict = strm_dict_set(dict, "video_dest_w", itoa(width, buffer, 10));
        if (NULL == dict)
            goto fail;
    }

    return dict;

fail:
    strm_dict_destroy(dict);
    return NULL;
}

int main(int argc, char *argv[])
{
    int rc;
    int exit_application = 0;

    // Screen variables
    screen_context_t    screen_context = 0;
    screen_window_t     screen_window = 0;

    int screen_size[2] = {0,0};

    // Renderer variables
    mmr_connection_t*     mmr_connection = 0;
    mmr_context_t*        mmr_context = 0;
    strm_dict_t*          dict = NULL;

    // I/O variables
    int                    video_device_output_id = -1;
    int                    audio_device_output_id = -1;

    bps_initialize();

    /*
     * Create the window used for video output.
     */
    if (screen_create_context(&screen_context, SCREEN_APPLICATION_CONTEXT) != 0) {
        return EXIT_FAILURE;
    }

    if (screen_create_window(&screen_window, screen_context) != 0) {
        screen_destroy_context(screen_context);
        return EXIT_FAILURE;
    }

    if (screen_create_window_group(screen_window, window_group_name) != 0) {
        return EXIT_FAILURE;
    }

    int format = SCREEN_FORMAT_RGBA8888;
    if (screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_FORMAT, &format) != 0) {
        return EXIT_FAILURE;
    }

    int usage = SCREEN_USAGE_NATIVE;
    if (screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_USAGE, &usage) != 0) {
        return EXIT_FAILURE;
    }


    if (screen_create_window_buffers(screen_window, 1) != 0) {
        return EXIT_FAILURE;
    }

    /*
     * Configure mm-renderer.
     */
    mmr_connection = mmr_connect(NULL);
    if (mmr_connection == NULL) {
        return EXIT_FAILURE;
    }

    mmr_context = mmr_context_create(mmr_connection, video_context_name, 0, S_IRWXU|S_IRWXG|S_IRWXO);
    if (mmr_context == NULL) {
        return EXIT_FAILURE;
    }

    /*
     * Configure video and audio output.
     */
    video_device_output_id = mmr_output_attach(mmr_context, video_device_url, "video");
    if (video_device_output_id == -1) {
        return EXIT_FAILURE;
    }

    audio_device_output_id = mmr_output_attach(mmr_context, audio_device_url, "audio");
    if (audio_device_output_id == -1) {
        return EXIT_FAILURE;
    }

    // Get the render buffer
    screen_buffer_t temp_buffer[1];
    if (screen_get_window_property_pv( screen_window, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)temp_buffer) != 0) {
        return EXIT_FAILURE;
    }

    // Fill the buffer with a solid color (black)
    int fill_attributes[3] = {SCREEN_BLIT_COLOR, 0x0, SCREEN_BLIT_END};
    if (screen_fill(screen_context, temp_buffer[0], fill_attributes) != 0) {
        return EXIT_FAILURE;
    }

    // Make the window visible
    if (screen_get_window_property_iv(screen_window, SCREEN_PROPERTY_SIZE, screen_size) != 0) {
        return EXIT_FAILURE;
    }

    int temp_rectangle[4] = {0, 0, screen_size[0], screen_size[1]};
    if (screen_post_window(screen_window, temp_buffer[0], 1, temp_rectangle, 0) != 0) {
        return EXIT_FAILURE;
    }

    // Prevent the backlight from going off
    int idle_mode = SCREEN_IDLE_MODE_KEEP_AWAKE;
    if (screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_IDLE_MODE, &idle_mode) != 0) {
        return EXIT_FAILURE;
    }

    // Build up the path where our bundled resource is.
    char cwd[PATH_MAX];
    char media_file[PATH_MAX];
    getcwd(cwd,PATH_MAX);

    rc = snprintf(media_file, PATH_MAX, "file://%s/app/native/pb_sample.mp4", cwd);
    if ((rc == -1) || (rc >= PATH_MAX)) {
        return EXIT_FAILURE;
    }

    /*
     * Start the playback.
     */
    if (mmr_input_attach(mmr_context, media_file, "track") != 0) {
        return EXIT_FAILURE;
    }

    if (mmr_play(mmr_context) != 0) {
        return EXIT_FAILURE;
    }

    /* Do some work to make the aspect ratio correct.
     */
    dict = calculate_rect(screen_size[0], screen_size[1]);
    if (NULL == dict) {
        return EXIT_FAILURE;
    }

    if (mmr_output_parameters(mmr_context, video_device_output_id, dict) != 0) {
        return EXIT_FAILURE;
    }

     /* Note that we allocated memory for the dictionary, but the call to 
      * mmr_output_parameters() deallocates that memory even on failure.
      */  
    dict = NULL;

    screen_request_events(screen_context);
    navigator_request_events(0);

    /*
     * Handle keyboard events and stop playback upon user request.
     */
    for (;;) {
        bps_event_t *event = NULL;
        if (bps_get_event(&event, -1) != BPS_SUCCESS) {
            return EXIT_FAILURE;
        }

        if (event) {

            if (bps_event_get_domain(event) == navigator_get_domain() &&
                bps_event_get_code(event) == NAVIGATOR_EXIT) {

                exit_application = 1;
            }

            if (exit_application) {
                break;
            }
        }
    }

    screen_stop_events(screen_context);

    if (mmr_stop(mmr_context) != 0) {
        return EXIT_FAILURE;
    }

    if (mmr_output_detach(mmr_context, audio_device_output_id) != 0) {
        return EXIT_FAILURE;
    }

    if (mmr_output_detach(mmr_context, video_device_output_id) != 0) {
        return EXIT_FAILURE;
    }

    if (mmr_context_destroy(mmr_context) != 0) {
        return EXIT_FAILURE;
    }

    mmr_context = 0;
    video_device_output_id = -1;
    audio_device_output_id = -1;

    mmr_disconnect(mmr_connection);
    mmr_connection = 0;

    bps_shutdown();

    if (screen_destroy_window(screen_window) != 0) {
        return EXIT_FAILURE;
    }

    if (screen_destroy_context(screen_context) != 0) {
        return EXIT_FAILURE;
    }

    screen_context = 0;
    screen_window = 0;

    return EXIT_SUCCESS;
}
