/**
 * MULTITOUCH
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

#include <assert.h>
#include <screen/screen.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <math.h>
#include <time.h>

#include <sys/keycodes.h>
#include <bps/virtualkeyboard.h>
#include <gestures/types.h>
#include <gestures/set.h>
#include <input/screen_helpers.h>

#include "bbutil.h"
#include "Render.h"
#include "EventHandler.h"
#include "InitBBSDK.h"

int main(int argc, char *argv[]) {

	int rc;
	int exit_application = 0;

	initBBSpecific();

	initOpenGL(screen_cxt);

	while (closeApplication == 0) {
		handleEvent();
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

