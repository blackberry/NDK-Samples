/**
 *
 * The Eventhandler class handles incoming events: how to react on an incoming event
 *
 *
 * Copyright (c) 2012 mappau OG
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
 *
 * feel free to contact us:
 * blackberry@mappau.com
 * http://www.mappau.com
 *
 */

#include "EventHandler.h"

#include <bps/bps.h>
#include <bps/event.h>
#include "bbutil.h"
#include <bps/screen.h>
#include <bps/virtualkeyboard.h>
#include <bps/navigator.h>
#include <gestures/types.h>
#include <gestures/set.h>
#include <input/screen_helpers.h>
#include <sys/keycodes.h>
#include <assert.h>
#include "Render.h"
#include <bps/orientation.h>

#include <bps/netstatus.h>
#include "Touchpoint.h"
#include "Render.h"
#include <list>

#include <bps/dialog.h>

dialog_instance_t alert_dialog = 0;

int move_x = 0;
int move_y = 0;
int down_x = 0;
int down_y = 0;
int min_swipe = 30;
int oriention_side_up = 0;
int closeApplication = 0;



std::list<Touchpoint> points;
std::list<Touchpoint>::iterator p;

void handleEvent() {
	//Request and process BPS next available event
	int rc, domain;

	for (;;) {
		bps_event_t *event = NULL;
		rc = bps_get_event(&event, 0);
		assert(rc == BPS_SUCCESS);

		if (event) {

			int domain = bps_event_get_domain(event);

			if (domain == screen_get_domain()) {
				handleScreenEvent(event);

			} else if(domain == navigator_get_domain()) {
                handleNavigatorEvent(event);

			}
		} else {
			break;
		}
	}

}

void handleScreenEvent(bps_event_t *event) {
	screen_event_t screen_event = screen_event_get_event(event);

	int screen_val;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE,
			&screen_val);

	mtouch_event_t mtouch_event;
	int rc = screen_get_mtouch_event(screen_event, &mtouch_event, 0);
	if (rc) {
		fprintf(stderr, "Error: failed to get mtouch event\n");
	}

	p = points.begin();
	bool found;
	found = false;

	while (p != points.end()) {
		if (p->id == mtouch_event.contact_id) {
			found = true;
			break;
		}
		p++;
	}

	switch (screen_val) {
	case SCREEN_EVENT_MTOUCH_TOUCH:

		if (!found) {

			Touchpoint *tp = new Touchpoint(mtouch_event.x, mtouch_event.y,
					mtouch_event.contact_id);
			if(mtouch_event.contact_id<4){
				tp->setColor(colors[mtouch_event.contact_id][0],colors[mtouch_event.contact_id][1],colors[mtouch_event.contact_id][2]);
			}
			points.push_back(*tp);
			fprintf(stderr,"new touchpoint: %i Orientation: %i \n",mtouch_event.contact_id,tp->startRotation);
		} else {
			p->updatePosition(mtouch_event.x, mtouch_event.y);
		}

		break;
	case SCREEN_EVENT_MTOUCH_MOVE:

		if (found) {
			p->updatePosition(mtouch_event.x, mtouch_event.y);
		} else {
			fprintf(stderr, "ERROR: TOUCH POINT NOT FOUND\n");
		}
		break;
	case SCREEN_EVENT_MTOUCH_RELEASE:
		if (found) {
			p->setInvisible();
		} else {
			fprintf(stderr, "ERROR: TOUCH POINT NOT FOUND\n");
		}
		break;
	}
}

void handleNavigatorEvent(bps_event_t *event) {

    int rc;
    bps_event_t *activation_event = NULL;

    switch (bps_event_get_code(event)) {
    case NAVIGATOR_ORIENTATION_CHECK:
        //Signal navigator that we intend to resize
        navigator_orientation_check_response(event, true);
        break;
    case NAVIGATOR_ORIENTATION:
    	resize(event);
        break;
    case NAVIGATOR_ORIENTATION_DONE:
    	break;
    case NAVIGATOR_EXIT:
		fprintf(stderr, "CLOSE APP \n");
		closeApplication = 1;
        break;
    case NAVIGATOR_WINDOW_INACTIVE:
        //Wait for NAVIGATOR_WINDOW_ACTIVE event
        for (;;) {
            rc = bps_get_event(&activation_event, -1);
            assert(rc == BPS_SUCCESS);

            if (bps_event_get_code(activation_event)
                    == NAVIGATOR_WINDOW_ACTIVE) {
                break;
            }
        }
        break;
    }
}

int resize(bps_event_t *event)
{
	int angle = navigator_event_get_orientation_angle(event);
	fprintf(stderr,"angle: %i\n",angle);

	//Let bbutil rotate current screen surface to this angle
	if (EXIT_FAILURE == bbutil_rotate_screen_surface(angle))
	{
		fprintf(stderr, "Unable to handle orientation change\n");
		return EXIT_FAILURE;
	}

    //Query width and height of the window surface created by utility code
    EGLint surface_width, surface_height;

    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    EGLint err = eglGetError();
    if (err != 0x3000) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }

    // width and height from Render.h
    width = (float) surface_width;
    height = (float) surface_height;

	fprintf(stderr, "angle %i\n",angle);
	switch (angle)
	{
	case 0:
		oriention_side_up = 0;
		break;
	case 90:
		oriention_side_up = 1;
		break;
	case 270:
		oriention_side_up = 2;
		break;
	case 180:
		oriention_side_up = -1;
		break;
	default:
		oriention_side_up = 0;
		break;
	}

	render();

	navigator_done_orientation(event);

	return EXIT_SUCCESS;
}
