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

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/keycodes.h>
#include <screen/screen.h>
#include <bps/sensor.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/orientation.h>
#include <math.h>
#include <time.h>
#include <screen/screen.h>
#include <EGL/egl.h>
#include <GLES/gl.h>

#include "bbutil.h"

static bool shutdown;
static int orientation_angle;
static screen_context_t screen_cxt;
static float width, height, max_size;

typedef struct box_t {
    float x;
    float y;
    float size;
    GLfloat color;
} box;

static int num_boxes;
static box* boxes;
static GLfloat vertices[8];

#define MAX_BOXES 200

static float gravity_x, gravity_y;

int init_blocks() {
    EGLint surface_width, surface_height;

    //Initialize common vertex data
    vertices[0] = 0.0f;
    vertices[1] = 0.0f;

    vertices[2] = 1.0f;
    vertices[3] = 0.0f;

    vertices[4] = 0.0f;
    vertices[5] = 1.0f;

    vertices[6] = 1.0f;
    vertices[7] = 1.0f;

    //Initialize app data
    max_size = 60.0;

    //Query width and height of the window surface created by utility code
    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    EGLint err = eglGetError();
    if (err != 0x3000) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }

    width = (float) surface_width;
    height = (float) surface_height;

    //Initialize GL for 2D rendering
    glViewport(0, 0, (int) width, (int) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, width / height, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Set world coordinates to coincide with screen pixels
    glScalef(1.0f / height, 1.0f / height, 1.0f);

    gravity_x = 0.0;
    gravity_y = 0.0;

    boxes = (box*) malloc(sizeof(box) * MAX_BOXES);

    if (!boxes) {
        return EXIT_FAILURE;
    }

    num_boxes = 0;

    //Set clear color to a shade of green for good looks
    glClearColor(0.0f, 0.25f, 0.0f, 1.0f);

    return EXIT_SUCCESS;
}

void set_gravity(float x, float y) {
    gravity_x = x;
    gravity_y = y;
}

void clear_cubes() {
    num_boxes = 0;
}

void add_cube(float x, float y) {
    //See if we reached a limit
    if (num_boxes == MAX_BOXES - 1)
        return;

    //Add a cube with a random shade of green and some size variation
    boxes[num_boxes].color = ((float) rand()) / RAND_MAX;

    boxes[num_boxes].x = (float) x;
    boxes[num_boxes].y = height - y;
    boxes[num_boxes].size = 40.0 + 20.0 * ((float) rand()) / RAND_MAX;

    num_boxes++;
}

static void update() {
    //Update position of every cube
    int i;
    float m, b;

    for (i = 0; i < num_boxes; i++) {
        boxes[i].x += gravity_x * 5;
        boxes[i].y += gravity_y * 5;

        //y = mx + b
        if ((gravity_x > 0.05) || (gravity_x < -0.05)) {
            //General case, boxes are not falling vertically
            m = gravity_y / gravity_x;
            b = boxes[i].y - m * boxes[i].x;

            if ((boxes[i].x > width) && (gravity_x > 0.0)) {
                //Right edge
                if ((b >= 0) && (b <= height)) {
                    //Intersection with x = 0
                    boxes[i].x = 0;
                    boxes[i].y = b;
                } else if ((-b / m >= 0) && (-b / m <= width)) {
                    //Intersection with y = 0
                    boxes[i].x = -b / m;
                    boxes[i].y = 0;
                } else if (((height - b) / m >= 0)
                        && ((height - b) / m <= width)) {
                    //Intersection with y = APP_HEIGHT
                    boxes[i].x = (height - b) / m;
                    boxes[i].y = height;
                } else {
                    //Corner case
                    boxes[i].x = 0;
                    boxes[i].y = 0;
                }
            } else if ((boxes[i].x + max_size < 0.0) && (gravity_x < 0.0)) {
                //Left edge
                if ((m * width + b >= 0) && (m * width + b <= height)) {
                    //Intersection with x = APP_WIDTH
                    boxes[i].x = width;
                    boxes[i].y = m * width + b;
                } else if ((-b / m >= 0) && (-b / m <= width)) {
                    //Intersection with y = 0
                    boxes[i].x = -b / m;
                    boxes[i].y = 0;
                } else if (((height - b) / m >= 0)
                        && ((height - b) / m <= width)) {
                    //Intersection with y = APP_HEIGHT
                    boxes[i].x = (height - b) / m;
                    boxes[i].y = height;
                } else {
                    //Corner case
                    boxes[i].x = 0;
                    boxes[i].y = 0;
                }
            } else if ((boxes[i].y > height) && (gravity_y > 0.0)) {
                //Top edge
                if ((b >= 0) && (b <= height)) {
                    //Intersection with x = 0
                    boxes[i].x = 0;
                    boxes[i].y = b;
                } else if ((-b / m >= 0) && (-b / m <= width)) {
                    //Intersection with y = 0
                    boxes[i].x = -b / m;
                    boxes[i].y = 0;
                } else if ((m * width + b >= 0) && (m * width + b <= height)) {
                    //Intersection with x = APP_WIDTH
                    boxes[i].x = width;
                    boxes[i].y = m * width + b;
                } else {
                    //Corner case
                    boxes[i].x = 0;
                    boxes[i].y = 0;
                }
            } else if ((boxes[i].y + max_size <= 0.0) && (gravity_y < 0.0)) {
                //Bottom edge
                if ((m * width + b >= 0) && (m * width + b <= height)) {
                    //Intersection with x = APP_WIDTH
                    boxes[i].x = width;
                    boxes[i].y = m * width + b;
                } else if ((b >= 0) && (b <= height)) {
                    //Intersection with x = 0
                    boxes[i].x = 0;
                    boxes[i].y = b;
                } else if (((height - b) / m >= 0)
                        && ((height - b) / m <= width)) {
                    //Intersection with y = APP_HEIGHT
                    boxes[i].x = (height - b) / m;
                    boxes[i].y = height;
                } else {
                    //Corner case
                    boxes[i].x = 0;
                    boxes[i].y = 0;
                }
            }
        } else {
            //Special case, boxes are falling (almost) vertically so we can't describe m = -+ infinity effectively
            if ((boxes[i].y > height) && (gravity_y > 0.0)) {
                //Top edge
                boxes[i].y = 0;
            } else if ((boxes[i].y + max_size <= 0.0) && (gravity_y < 0.0)) {
                //Bottom edge
                boxes[i].y = height;
            }
        }
    }
}

void render() {
    int i;

    //Typical rendering pass
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);

    for (i = 0; i < num_boxes; i++) {
        glPushMatrix();

        glColor4f(boxes[i].color, 0.78f, 0, 1.0f);
        glTranslatef(boxes[i].x, boxes[i].y, 0.0f);
        glScalef(boxes[i].size, boxes[i].size, 1.0f);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glPopMatrix();
    }

    glDisableClientState(GL_VERTEX_ARRAY);

    //Use utility code to update the screen
    bbutil_swap();
}

static void handleScreenEvent(bps_event_t *event) {
    int screen_val, buttons;
    int pair[2];

    static bool mouse_pressed = false;

    screen_event_t screen_event = screen_event_get_event(event);

    //Query type of screen event and its location on the screen
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE,
            &screen_val);
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_SOURCE_POSITION,
            pair);

    //There is a difference between touch screen events and mouse events
    if (screen_val == SCREEN_EVENT_MTOUCH_RELEASE) {
        //This is touch screen event.
        add_cube((float) pair[0], (float) pair[1]);
    } else if (screen_val == SCREEN_EVENT_POINTER) {
        //This is a mouse move event, it is applicable to a device with a usb mouse or simulator
        screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS,
                &buttons);

        if (buttons == SCREEN_LEFT_MOUSE_BUTTON) {
            //Left mouse button is pressed
            mouse_pressed = true;
        } else {
            if (mouse_pressed) {
                //Left mouse button was released, add a cube
                add_cube((float) pair[0], (float) pair[1]);
                mouse_pressed = false;
            }
        }
    }
}

static void handleNavigatorEvent(bps_event_t *event) {
    switch (bps_event_get_code(event)) {
    case NAVIGATOR_SWIPE_DOWN:
        clear_cubes();
        break;
    case NAVIGATOR_EXIT:
        shutdown = true;
        break;
    }
}

static void handleSensorEvent(bps_event_t *event) {
    if (SENSOR_GRAVITY_READING == bps_event_get_code(event)) {
        float x, y, z;

        sensor_event_get_xyz(event, &x, &y, &z);
        set_gravity(-x, -y);
    }

}

static void handle_events() {
    int screen_domain = screen_get_domain();
    int navigator_domain = navigator_get_domain();
    int sensor_domain = sensor_get_domain();

    //Request and process available BPS events
    for(;;) {
        bps_event_t *event = NULL;
        if (BPS_SUCCESS != bps_get_event(&event, 0)) {
            fprintf(stderr, "bps_get_event failed\n");
            return;
        }

        if (event) {
            int domain = bps_event_get_domain(event);

            if (domain == screen_domain) {
                handleScreenEvent(event);
            } else if (domain == navigator_domain) {
                handleNavigatorEvent(event);
            } else if (domain == sensor_domain) {
                handleSensorEvent(event);
            }
        } else {
            //No more events in the queue
            break;
        }
    }
}

int main(int argc, char **argv) {
    shutdown = false;

    //Create a screen context that will be used to create an EGL surface to to receive libscreen events
    screen_create_context(&screen_cxt, 0);

    //Initialize BPS library
    bps_initialize();

    //Determine initial orientation angle
    orientation_direction_t direction;
    orientation_get(&direction, &orientation_angle);

    //Use utility code to initialize EGL for rendering with GL ES 1.1
    if (EXIT_SUCCESS != bbutil_init_egl(screen_cxt)) {
        fprintf(stderr, "bbutil_init_egl failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        return 0;
    }

    //Initialize application logic
    if (EXIT_SUCCESS != init_blocks()) {
        fprintf(stderr, "initialize failed\n");
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

    //Signal BPS library that navigator orientation is not to be locked
    if (BPS_SUCCESS != navigator_rotation_lock(false)) {
        fprintf(stderr, "navigator_rotation_lock failed\n");
        bbutil_terminate();
        screen_destroy_context(screen_cxt);
        return 0;
    }

    //Setup Sensors
    if (sensor_is_supported(SENSOR_TYPE_GRAVITY)) {
        //Microseconds between sensor reads. This is the rate at which the
        //sensor data will be updated from hardware. The hardware update
        //rate is set below using sensor_set_rate.
        static const int SENSOR_RATE = 25000;

        //Initialize the sensor by setting the rates at which the
        //sensor values will be updated from hardware
        sensor_set_rate(SENSOR_TYPE_GRAVITY, SENSOR_RATE);

        sensor_set_skip_duplicates(SENSOR_TYPE_GRAVITY, true);
        sensor_request_events(SENSOR_TYPE_GRAVITY);
    } else {
        set_gravity(0.0f, -1.0f);
    }

    //Start with one cube on the screen
    add_cube(200, 100);

    while (!shutdown) {
        // Handle user input and sensors
        handle_events();

        //Update cube positions
        update();

        // Draw Scene
        render();
    }

    //Stop requesting events from libscreen
    screen_stop_events(screen_cxt);

    //Shut down BPS library for this process
    bps_shutdown();

    //Free app data
    free(boxes);

    //Use utility code to terminate EGL setup
    bbutil_terminate();

    //Destroy libscreen context
    screen_destroy_context(screen_cxt);
    return 0;
}
