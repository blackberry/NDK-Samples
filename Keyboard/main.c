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

#include <screen/screen.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <stdlib.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include "bbutil.h"

#include <bps/virtualkeyboard.h>
#include <sys/keycodes.h>
#include <math.h>

static GLfloat vertices[8];
static GLfloat colors[16];

#define ANGLE_INCREMENT 3.0f
#define CIRCLE_DEGREES 360.0f
static float angle = 0.0;

void handleScreenEvent(bps_event_t *event)
{
	screen_event_t screen_event = screen_event_get_event(event);

	int screen_val;
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

	switch (screen_val) {
	case SCREEN_EVENT_KEYBOARD:
		screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_FLAGS, &screen_val);

		if (screen_val & KEY_DOWN) {
			screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM,&screen_val);

			printf("The '%c' key was pressed\n", (char)screen_val);
			fflush(stdout);

			switch (screen_val) {
			case KEYCODE_I:
				// Display the email layout with "Send" enter key
				virtualkeyboard_change_options(VIRTUALKEYBOARD_LAYOUT_EMAIL, VIRTUALKEYBOARD_ENTER_SEND);
				break;
			case KEYCODE_O:
				// Display the phone layout with "Connect" enter key
				virtualkeyboard_change_options(VIRTUALKEYBOARD_LAYOUT_PHONE, VIRTUALKEYBOARD_ENTER_CONNECT);
				break;
			case KEYCODE_P:
				// Display the default layout with default enter key
				virtualkeyboard_change_options(VIRTUALKEYBOARD_LAYOUT_DEFAULT, VIRTUALKEYBOARD_ENTER_DEFAULT);
				break;
			case KEYCODE_H:
				// Hide the keyboard
				virtualkeyboard_hide();
				break;
			case KEYCODE_A:
				// Increment rotation angle
				angle = fmod(angle + ANGLE_INCREMENT, CIRCLE_DEGREES );
				break;
			case KEYCODE_Z:
				// Decrement rotation angle
				angle = fmod(angle - ANGLE_INCREMENT, CIRCLE_DEGREES );
				break;
			default:
				break;
			}
		}
		break;
	}
}

void initialize()
{
	//Initialize vertex and color data
    vertices[0] = -0.25f;
    vertices[1] = -0.25f;

    vertices[2] = 0.25f;
    vertices[3] = -0.25f;

    vertices[4] = -0.25f;
    vertices[5] = 0.25f;

    vertices[6] = 0.25f;
    vertices[7] = 0.25f;

    colors[0] = 1.0f;
    colors[1] = 0.0f;
    colors[2] = 1.0f;
    colors[3] = 1.0f;

    colors[4] = 1.0f;
    colors[5] = 1.0f;
    colors[6] = 0.0f;
    colors[7] = 1.0f;

    colors[8] = 0.0f;
    colors[9] = 1.0f;
    colors[10] = 1.0f;
    colors[11] = 1.0f;

    colors[12] = 0.0f;
    colors[13] = 1.0f;
    colors[14] = 1.0f;
    colors[15] = 1.0f;

    //Query width and height of the window surface created by utility code
    EGLint surface_width, surface_height;

	eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    glTranslatef((float)(surface_width) / (float)(surface_height) / 2, 0.5f, 0.0f);
}

void render()
{
    //Typical render pass
	bbutil_clear();

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, colors);

    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    glDrawArrays(GL_TRIANGLE_STRIP, 0 , 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    //Use utility code to update the screen
    bbutil_swap();
}

int main(int argc, char *argv[])
{
    int exit_application = 0;

    screen_context_t screen_cxt;

    //Create a screen context that will be used to create an EGL surface to to receive libscreen events
    screen_create_context(&screen_cxt,0);

    //Use utility code to initialize EGL for 2D rendering with GL ES 1.1
    if (EXIT_SUCCESS != bbutil_init(screen_cxt, GL_ES_1)) {
    	bbutil_terminate();
        screen_destroy_context(screen_cxt);
        return 0;
    }

    //Initialize app data
    initialize();

    //Initialize BPS library
    bps_initialize();

    //Signal BPS library that navigator, screen, and keyboard events will be requested
    screen_request_events(screen_cxt);
    navigator_request_events(0);
    virtualkeyboard_request_events(0);

    virtualkeyboard_show();

    for (;;) {
    	//Request and process BPS next available event
        bps_event_t *event = NULL;
        if (bps_get_event(&event, 0) != BPS_SUCCESS)
        	return EXIT_FAILURE;

           if (event) {
                int domain = bps_event_get_domain(event);

                if (domain == screen_get_domain()) {
                    handleScreenEvent(event);
                } else if ((domain == navigator_get_domain()) && (NAVIGATOR_EXIT == bps_event_get_code(event)))  {
                	exit_application = 1;
                }
            }
            
            if (exit_application) {
                break;
            }
        render();
    }

    //Stop requesting events from libscreen
    screen_stop_events(screen_cxt);

    //Shut down BPS library for this process
    bps_shutdown();

    //Use utility code to terminate EGL setup
    bbutil_terminate();

    //Destroy libscreen context
    screen_destroy_context(screen_cxt);
    return 0;
}
