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
#include <bps/screen.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include <bbads/bbads.h>
#include <bbads/event.h>

/**
 *  Banner Properties
 *  Zone ID - Uniquely identifies app to the ad server
 *  Banner Size  - Specify banner size
 *  Banner Position -  Specify the position of the banner on screen
 */
static const int BANNER_TEST_ZONE_ID = 117145;
static const int BANNER_SIZE_X = 468;
static const int BANNER_SIZE_Y = 60;
static const int BANNER_POSITION_X = 180;
static const int BANNER_POSITION_Y = 150;

/**
 *  Instantiate the Banner Object
 */
bbads_banner_t* ad_banner = 0;
int ad_is_visible = 0;

// Screen variables
screen_context_t screen_context = 0;
screen_window_t screen_window = 0;
int screen_size[2] = {50, 50};

// Window group name
static const char *WINDOW_GROUP_NAME = "bbadssamplewindowgroup";

/**
 * 	create URL to the placeholder image
 */
static const char* get_banner_placeholder_image_url()
{
    static char* url = 0;

    if (!url) {
        // Build up the path where our bundled resource is.
        char cwd[PATH_MAX];
        getcwd(cwd,PATH_MAX);

        url = malloc(PATH_MAX);
        snprintf(url, PATH_MAX, "file://%s/app/native/placeholder_468x60.png", cwd);
    }

    return url;
}

/**
 * 	When the user clicks on the ad banner, the browser opens and loads the target of the ad. When this event occurs, the
 *	sample application receives the notification of the BBADS_EVENT_NAVIGATING event type, and the event handler
 *	displays a message indicating that it is handling the event.
 */
void banner_event_handler(bbads_banner_t* banner, bbads_banner_event_type_t type)
{
    if (BBADS_EVENT_NAVIGATING == type)
        fprintf(stderr, "Handling BBADS_EVENT_NAVIGATING event.\n");
}

/**
 *  Function we call to display banner
 */
int load_ad(screen_window_t screen_window, const char* window_group_name)
{
    if (ad_banner) {
        if (BBADS_EOK != bbads_banner_stop_events(ad_banner)) {
            fprintf(stderr, "ERROR: Could not stop events and destroy old ad banner!\n");
            return -1;
        }

        if (BBADS_EOK != bbads_banner_destroy(ad_banner)) {
            fprintf(stderr, "ERROR: Could not stop events and destroy old ad banner!\n");
            return -1;
        }

        ad_banner = 0;
    }

    /**
     *	Invoke bbads_banner_create() to create the ad banner, and pass the top-level screen in the application, the
     *	group name of the top-level screen, and the zone ID in the argument list. The handle of the banner is set in
	 *	ad_banner.
     */
    if (BBADS_EOK != bbads_banner_create(&ad_banner, screen_window, window_group_name, BANNER_TEST_ZONE_ID)) {
        fprintf(stderr, "ERROR: bbads_banner_create failed!\n");
        return -1;
    }

    /**
     * 	Invoke bbads_banner_set_size() to set the size of the banner using the constants specified earlier.
     */

    if (BBADS_EOK != bbads_banner_set_size(ad_banner, BANNER_SIZE_X, BANNER_SIZE_Y))
    	load_ad_failure();

    /**
     * Invoke bbads_banner_set_position() to set the position of the banner using the Screen position coordinates specified earlier.
     */

    if (BBADS_EOK != bbads_banner_set_position(ad_banner, BANNER_POSITION_X, BANNER_POSITION_Y))
    {
    	fprintf(stderr, "set position failed");
    	load_ad_failure();
    }

    /**
     * 	Invoke bbads_banner_set_placeholder_url() to specify the local URL for a placeholder image for the ad
	 *	banner.
     */

    if (BBADS_EOK != bbads_banner_set_placeholder_url(ad_banner, get_banner_placeholder_image_url()))
    {
    	fprintf(stderr, "set placeholder failed");
    	load_ad_failure();
    }

    /**
     *	Invoke bbads_banner_set_refresh_rate() to specify the time in seconds between each ad refresh. The
     *	minimum and default time is 60 seconds.
     */

    if (BBADS_EOK != bbads_banner_set_refresh_rate(ad_banner, 60))
    {
    	fprintf(stderr, "set refresh rate failed");
    	load_ad_failure();
    }

    /**
     * 	Invoke bbads_banner_set_border_width() to set the width of the border when the user selects the ad. The
     *	border width must be an integer value from 0 to 10 pixels.
     */

    if (BBADS_EOK != bbads_banner_set_border_width(ad_banner, 2))
    {
    	fprintf(stderr, "set border width failed");
    	load_ad_failure();
    }

    /**
     * 	The sample app uses bbads_banner_request_events() to request the asynchronous events for the ad
     *	banner through the BlackBerry Platform Services library.
     */

    if (BBADS_EOK != bbads_banner_request_events(ad_banner))
    {
    	fprintf(stderr, "request events failed");
    	load_ad_failure();
    }

    /**
     *	Invoke bbads_banner_load() to display the banner on the screen that bbads_banner_create() creates.
     */

    if (BBADS_EOK != bbads_banner_load(ad_banner))
    {
    	fprintf(stderr, "load failed");
    	load_ad_failure();
    }

    return 0;

}

int load_ad_failure()
{
	fprintf(stderr, "ERROR: Could not load ad banner!\n");
	bbads_banner_destroy(ad_banner);
	return -1;
}

int setup_screen()
{
    /*
     * Create the window.
     */
    if (screen_create_context(&screen_context, SCREEN_APPLICATION_CONTEXT) != 0)
        return EXIT_FAILURE;

    if (screen_create_window(&screen_window, screen_context) != 0) {
        screen_destroy_context(screen_context);
        return EXIT_FAILURE;
    }

    if (screen_create_window_group(screen_window, WINDOW_GROUP_NAME) != 0)
        return EXIT_FAILURE;

    int format = SCREEN_FORMAT_RGBA8888;
    if (screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_FORMAT, &format) != 0)
        return EXIT_FAILURE;

    int usage = SCREEN_USAGE_NATIVE;
    if (screen_set_window_property_iv(screen_window, SCREEN_PROPERTY_USAGE, &usage) != 0)
        return EXIT_FAILURE;

    if (screen_create_window_buffers(screen_window, 1) != 0)
        return EXIT_FAILURE;

    // Get the render buffer
    screen_buffer_t temp_buffer[1];
    if (screen_get_window_property_pv( screen_window, SCREEN_PROPERTY_RENDER_BUFFERS, (void**)temp_buffer) != 0)
        return EXIT_FAILURE;

    // Fill the buffer with a solid color (green)
    int fill_attributes[3] = {SCREEN_BLIT_COLOR, 0x00C000, SCREEN_BLIT_END};
    if (screen_fill(screen_context, temp_buffer[0], fill_attributes) != 0)
        return EXIT_FAILURE;

    // Make the window visible
    if (screen_get_window_property_iv(screen_window, SCREEN_PROPERTY_SIZE, screen_size) != 0)
        return EXIT_FAILURE;

    int temp_rectangle[4] = {0,0,screen_size[0],screen_size[1]};
    if (screen_post_window(screen_window, temp_buffer[0], 1, temp_rectangle, 0) != 0)
        return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
    int rc;
    int exit_application = 0;

    bps_initialize();

    // Create and set up the app window.
    if (EXIT_FAILURE == setup_screen())
        return EXIT_FAILURE;

    bbads_set_log_verbosity(2);

    // Display Banner on screen
    load_ad(screen_window, WINDOW_GROUP_NAME);

    screen_request_events(screen_context);
    navigator_request_events(0);

    /**
     * Handle keyboard events and stop playback upon user request.
     */
    for (;;) {
        bps_event_t *event = 0;
        if (BPS_SUCCESS != bps_get_event(&event, -1)) {
            fprintf(stderr, "ERROR: Could not get BPS event!\n");
            return EXIT_FAILURE;
        }

        if (event) {
            if (navigator_get_domain() == bps_event_get_domain(event)) {
                int code = bps_event_get_code(event);

                if (NAVIGATOR_EXIT == code) {
                	if (BBADS_EOK != bbads_banner_destroy(ad_banner))
                	{
                	    fprintf(stderr, "cannot destroy banner");
                	}
                	else
                	{
                	   fprintf(stderr, "banner destroy");
                	}

                    exit_application = 1;
                }

            }
            else if (bbads_get_domain() == bps_event_get_domain(event)) {
                bbads_banner_t* event_banner;

                bbads_event_get_banner(event, &event_banner);
                bbads_banner_event_type_t event_type = (bbads_banner_event_type_t)bps_event_get_code(event);

                banner_event_handler(event_banner, event_type);
            }
            else if (screen_get_domain() == bps_event_get_domain(event)) {
            	int screen_val;
				char buf[1024];
				int pos[2];

				screen_event_t screen_event = screen_event_get_event(event);
				screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);
				screen_get_event_property_cv(screen_event, SCREEN_PROPERTY_ID_STRING, sizeof(buf), buf);
				screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_POSITION, pos);

				switch (screen_val) {
				case SCREEN_EVENT_CREATE:
					fprintf(stderr, "create\n");
					if (BBADS_EOK != bbads_banner_is_visible(ad_banner, &ad_is_visible))
					{
						fprintf(stderr, "cannot get banner visibility");
					}
					if (!ad_is_visible)
					{
						if (BBADS_EOK != bbads_banner_set_window_visible(ad_banner))
							fprintf(stderr, "bbads_banner_set_window_visible has errors\n");

						if (BBADS_EOK != bbads_banner_display(ad_banner, screen_context, event))
							fprintf(stderr, "create cannot display banner\n");

					}
					break;
				}

            if (exit_application)
                break;
            }
        }
    }

    screen_stop_events(screen_context);

    bps_shutdown();

    if (screen_destroy_window(screen_window) != 0)
        return EXIT_FAILURE;

    if (screen_destroy_context(screen_context) != 0)
        return EXIT_FAILURE;

    screen_context = 0;
    screen_window = 0;

    return EXIT_SUCCESS;
}
