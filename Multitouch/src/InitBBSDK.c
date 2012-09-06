/**
 *
 * This class initializes all BB specific stuff (Events, Sensors, ..)
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

#include "bbutil.h"
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/orientation.h>
#include <bps/dialog.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include "InitBBSDK.h"
#include "Render.h"

int dpi;

GLfloat verticesBG[8];
GLfloat tex_coordBG[8];

GLuint background;

int initBBSpecific() {
	//Create a screen context that will be used to create an EGL surface to to receive libscreen events
	screen_create_context(&screen_cxt, 0);

	//Initialize BPS library4
	bps_initialize();

	//Use utility code to initialize EGL for rendering with GL ES 1.1
	if (EXIT_SUCCESS != bbutil_init_egl(screen_cxt, GL_ES_1)) {
		fprintf(stderr, "bbutil_init_egl failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	//Signal BPS library that navigator and screen events will be requested
	if (BPS_SUCCESS != screen_request_events(screen_cxt)) {
		fprintf(stderr, "screen_request_events failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	if (BPS_SUCCESS != navigator_request_events(0)) {
		fprintf(stderr, "navigator_request_events failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	//lock orientation
	if (BPS_SUCCESS != navigator_rotation_lock(false)) {
		fprintf(stderr, "navigator_rotation_lock failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	//get orientation:
	if (BPS_SUCCESS != orientation_request_events(0)) {
		fprintf(stderr, "navigator_rotation_lock failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	//Let bbutil rotate current screen surface to this angle
	if (EXIT_FAILURE == bbutil_rotate_screen_surface(0)) {
		fprintf(stderr, "Unable to handle orientation change\n");
		return EXIT_FAILURE;
	}

	if (EXIT_FAILURE ==  dialog_request_events(0)) {
		fprintf(stderr, "Unable to handle orientation change\n");
		return EXIT_FAILURE;
	}

	return BPS_SUCCESS;
}

