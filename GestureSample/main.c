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

#include <assert.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <fcntl.h>
#include <img/img.h>
#include <math.h>
#include <screen/screen.h>
#include <string.h>

#include "input/screen_helpers.h"
#include "gestures/double_tap.h"
#include "gestures/pinch.h"
#include "gestures/set.h"
#include "gestures/swipe.h"
#include "gestures/tap.h"
#include "gestures/two_finger_pan.h"





#define MIN_VIEWPORT_SIZE 128
#define MAX_VIEWPORT_SIZE 4096

static bool shutdown;
struct gestures_set * set;
const char* img_path = "app/native/wallpaper.jpg"; /* Relative path to image asset */
int viewport_pos[2] = { 0, 0 };
int viewport_size[2] = { 0, 0 };
int last_touch[2] = { 0, 0 };

/**
 * The callback invoked when a gesture is recognized or updated.
 */
void
gesture_callback(gesture_base_t* gesture, mtouch_event_t* event, void* param, int async)
{
    if (async) {
        fprintf(stderr,"[async] ");
    }
    switch (gesture->type) {
        case GESTURE_TWO_FINGER_PAN: {
            gesture_tfpan_t* tfpan = (gesture_tfpan_t*)gesture;
            fprintf(stderr,"Two finger pan: %d, %d", (tfpan->last_centroid.x - tfpan->centroid.x), (tfpan->last_centroid.y - tfpan->centroid.y));
            if (tfpan->last_centroid.x && tfpan->last_centroid.y) {
                viewport_pos[0] += (tfpan->last_centroid.x - tfpan->centroid.x) >> 1;
                viewport_pos[1] += (tfpan->last_centroid.y - tfpan->centroid.y) >> 1;
            }
            break;
        }
        case GESTURE_PINCH: {
            gesture_pinch_t* pinch = (gesture_pinch_t*)gesture;
            fprintf(stderr,"Pinch %d, %d", (pinch->last_distance.x - pinch->distance.x), (pinch->last_distance.y - pinch->distance.y));

            int dist_x = pinch->distance.x;
            int dist_y = pinch->distance.y;
            int last_dist_x = pinch->last_distance.x;
            int last_dist_y = pinch->last_distance.y;

            int reldist = sqrt((dist_x)*(dist_x) + (dist_y)*(dist_y));
            int last_reldist = sqrt((last_dist_x)*(last_dist_x) + (last_dist_y)*(last_dist_y));

            if (reldist && last_reldist) {
                viewport_size[0] += (last_reldist - reldist) >> 1;
                viewport_size[1] += (last_reldist - reldist) >> 1;

                /* Size restrictions */
                if (viewport_size[0] < MIN_VIEWPORT_SIZE) {
                    viewport_size[0] = MIN_VIEWPORT_SIZE;
                } else if (viewport_size[0] > MAX_VIEWPORT_SIZE) {
                    viewport_size[0] = MAX_VIEWPORT_SIZE;
                }
                if (viewport_size[1] < MIN_VIEWPORT_SIZE) {
                    viewport_size[1] = MIN_VIEWPORT_SIZE;
                } else if (viewport_size[1] > MAX_VIEWPORT_SIZE) {
                    viewport_size[1] = MAX_VIEWPORT_SIZE;
                }

                /* Zoom into center of image */
                if (viewport_size[0] > MIN_VIEWPORT_SIZE && viewport_size[1] > MIN_VIEWPORT_SIZE &&
                        viewport_size[0] < MAX_VIEWPORT_SIZE && viewport_size[1] < MAX_VIEWPORT_SIZE) {
                    viewport_pos[0] -= (last_reldist - reldist) >> 2;
                    viewport_pos[1] -= (last_reldist - reldist) >> 2;
                }
            }
            break;
        }
        case GESTURE_TAP: {
            gesture_tap_t* tap = (gesture_tap_t*)gesture;
            fprintf(stderr,"Tap x:%d y:%d",tap->touch_coords.x,tap->touch_coords.y);
            break;
        }
        case GESTURE_DOUBLE_TAP: {
            gesture_tap_t* d_tap = (gesture_tap_t*)gesture;
            fprintf(stderr,"Double tap x:%d y:%d", d_tap->touch_coords.x, d_tap->touch_coords.y);
            break;
        }
        default: {
            fprintf(stderr,"Unknown Gesture");
            break;
        }
    }
    fprintf(stderr,"\n");
}

/**
 * Initialize the gestures sets
 */
static void
init_gestures()
{
    gesture_tap_t* tap;
    gesture_double_tap_t* double_tap;
    set = gestures_set_alloc();
    if (NULL != set) {
        tap = tap_gesture_alloc(NULL, gesture_callback, set);
        double_tap = double_tap_gesture_alloc(NULL, gesture_callback, set);
        tfpan_gesture_alloc(NULL, gesture_callback, set);
        pinch_gesture_alloc(NULL, gesture_callback, set);
    } else {
        fprintf(stderr, "Failed to allocate gestures set\n");
    }
}

static void
gestures_cleanup()
{
    if (NULL != set) {
        gestures_set_free(set);
        set = NULL;
    }
}

static void
handle_screen_event(bps_event_t *event)
{
    int screen_val, rc;

    screen_event_t screen_event = screen_event_get_event(event);
    mtouch_event_t mtouch_event;
    rc = screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);
    if(screen_val == SCREEN_EVENT_MTOUCH_TOUCH || screen_val == SCREEN_EVENT_MTOUCH_MOVE || screen_val == SCREEN_EVENT_MTOUCH_RELEASE) {
        rc = screen_get_mtouch_event(screen_event, &mtouch_event, 0);
        if (rc) {
            fprintf(stderr, "Error: failed to get mtouch event\n");
        }
        rc = gestures_set_process_event(set, &mtouch_event, NULL);

        /* No gesture detected, treat as pan. */
        if (!rc) {
            if (mtouch_event.contact_id == 0) {
                if(last_touch[0] && last_touch[1]) {
                    fprintf(stderr,"Pan %d %d\n",(last_touch[0] - mtouch_event.x),(last_touch[1] - mtouch_event.y));
                    viewport_pos[0] += (last_touch[0] - mtouch_event.x) >> 1;
                    viewport_pos[1] += (last_touch[1] - mtouch_event.y) >> 1;
                }
                last_touch[0] = mtouch_event.x;
                last_touch[1] = mtouch_event.y;
            }
        }
        if (screen_val == SCREEN_EVENT_MTOUCH_RELEASE) {
            last_touch[0] = 0;
            last_touch[1] = 0;
        }
    }
}

static void
handle_navigator_event(bps_event_t *event) {
    switch (bps_event_get_code(event)) {
    case NAVIGATOR_EXIT:
        shutdown = true;
        break;
    }
}

static void
handle_events()
{
    int rc, domain;
    bool has_events = true;

    while(has_events) {
        bps_event_t *event = NULL;
        rc = bps_get_event(&event, 50);
        assert(rc == BPS_SUCCESS);
        if (event) {
            domain = bps_event_get_domain(event);
            if (domain == navigator_get_domain()) {
                handle_navigator_event(event);
            } else if (domain == screen_get_domain()) {
                handle_screen_event(event);
            }
        } else {
            has_events = false;
        }
    }
}

static int decode_setup(uintptr_t data, img_t *img, unsigned flags)
{
    screen_window_t screen_win = (screen_window_t)data;
    screen_buffer_t screen_buf;
    int size[2];

    size[0] = img->w;
    size[1] = img->h;
    screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, size);
    screen_create_window_buffers(screen_win, 1);

    screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);
    screen_get_buffer_property_pv(screen_buf, SCREEN_PROPERTY_POINTER, (void **)&img->access.direct.data);
    screen_get_buffer_property_iv(screen_buf, SCREEN_PROPERTY_STRIDE, (int *)&img->access.direct.stride);

    img->flags |= IMG_DIRECT;
    return IMG_ERR_OK;
}

static void decode_abort(uintptr_t data, img_t *img)
{
    screen_window_t screen_win = (screen_window_t)data;
    screen_destroy_window_buffers(screen_win);
}

int
load_image(screen_window_t screen_win, const char *path)
{
    img_decode_callouts_t callouts;
    img_lib_t ilib = NULL;
    img_t img;
    int rc;

    rc = img_lib_attach(&ilib);
    if (rc != IMG_ERR_OK) {
        return -1;
    }

    memset(&img, 0, sizeof(img));
    img.flags |= IMG_FORMAT;
    img.format = IMG_FMT_PKLE_XRGB8888;

    memset(&callouts, 0, sizeof(callouts));
    callouts.setup_f = decode_setup;
    callouts.abort_f = decode_abort;
    callouts.data = (uintptr_t)screen_win;

    rc = img_load_file(ilib, path, &callouts, &img);
    img_lib_detach(ilib);

    return rc == IMG_ERR_OK ? 0 : -1;
}


int
main(int argc, char **argv)
{
    const int usage = SCREEN_USAGE_WRITE;

    screen_context_t screen_ctx;
    screen_window_t screen_win;
    screen_buffer_t screen_buf = NULL;
    int rect[4] = { 0, 0, 0, 0 };

    /* Setup the window */
    screen_create_context(&screen_ctx, 0);
    screen_create_window(&screen_win, screen_ctx);
    screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);

    screen_create_window_buffers(screen_win, 1);

    load_image(screen_win, img_path);

    screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);
    screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect+2);
    viewport_size[0] = rect[2];
    viewport_size[1] = rect[3];
    screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_SIZE , viewport_size);

    screen_post_window(screen_win, screen_buf, 1, rect, 0);

    init_gestures();

    /* Signal bps library that navigator and screen events will be requested */
    bps_initialize();
    screen_request_events(screen_ctx);
    navigator_request_events(0);

    while (!shutdown) {
        /* Handle user input */
        handle_events();
        screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_POSITION , viewport_pos);
        screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_SIZE , viewport_size);
        screen_flush_context(screen_ctx,0);
    }

    /* Clean up */
    gestures_cleanup();
    screen_stop_events(screen_ctx);
    bps_shutdown();
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    return 0;
}


