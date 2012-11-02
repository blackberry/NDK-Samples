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
#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/deviceinfo.h>
#include <bps/sensor.h>

#include <glview/glview.h>
#include <GLES/gl.h>
#include <screen/screen.h>

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    float x;
    float y;
    float size;
    GLfloat color;
} box_t;

typedef struct {
    float gravity_x;
    float gravity_y;
    float width;
    float height;

    int num_boxes;
    box_t *boxes;
} app_t;

static const GLfloat vertices[] =
{
    0.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
};

#define MAX_SIZE 60.0f
#define MAX_BOXES 200

static void add_cube(app_t *app, int x, int y) {
    //See if we reached a limit
    int num_boxes = app->num_boxes;
    if (num_boxes == MAX_BOXES - 1) {
        return;
    }

    //Add a cube with a random shade of green and some size variation
    app->boxes[num_boxes].color = ((float) rand()) / RAND_MAX;

    app->boxes[num_boxes].x = (float)x;
    app->boxes[num_boxes].y = app->height - (float)y;
    app->boxes[num_boxes].size = 40.0 + 20.0 * ((float) rand()) / RAND_MAX;

    app->num_boxes++;
}

static void initialize(void *data) {
    app_t *app = (app_t *)data;

    unsigned int width;
    unsigned int height;

    glview_get_size(&width, &height);

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, (float)width / (float)height, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Set world coordinates to coincide with screen pixels
    glScalef(1.0f / (float)height, 1.0f / (float)height, 1.0f);

    app->width = (float)width;
    app->height = (float)height;

    //Set clear color to a shade of green for good looks
    glClearColor(0.0f, 0.25f, 0.0f, 1.0f);

    //Setup Sensors
    app->gravity_x = 0.0f;
    app->gravity_y = -1.0f;

    // Gravity data doesn't change for the simulator
    deviceinfo_details_t *details;
    bool is_simulator = false;
    if (BPS_SUCCESS == deviceinfo_get_details(&details)) {
        is_simulator = deviceinfo_details_is_simulator(details);
        deviceinfo_free_details(&details);
    }

    if (!is_simulator && sensor_is_supported(SENSOR_TYPE_GRAVITY)) {
        //Microseconds between sensor reads. This is the rate at which the
        //sensor data will be updated from hardware. The hardware update
        //rate is set below using sensor_set_rate.
        static const int SENSOR_RATE = 25000;

        //Initialize the sensor by setting the rates at which the
        //sensor values will be updated from hardware
        sensor_set_rate(SENSOR_TYPE_GRAVITY, SENSOR_RATE);

        sensor_set_skip_duplicates(SENSOR_TYPE_GRAVITY, true);
        sensor_request_events(SENSOR_TYPE_GRAVITY);
    }

    //Start with one cube on the screen
    add_cube(app, 200, 100);
}

static void update(app_t *app) {
    //Update position of every cube
    int i;
    float m, b;

    box_t *boxes = app->boxes;
    float gravity_x = app->gravity_x;
    float gravity_y = app->gravity_y;
    float width = app->width;
    float height = app->height;

    for (i = 0; i < app->num_boxes; i++) {
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
            } else if ((boxes[i].x + MAX_SIZE < 0.0) && (gravity_x < 0.0)) {
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
            } else if ((boxes[i].y + MAX_SIZE <= 0.0) && (gravity_y < 0.0)) {
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
            } else if ((boxes[i].y + MAX_SIZE <= 0.0) && (gravity_y < 0.0)) {
                //Bottom edge
                boxes[i].y = height;
            }
        }
    }
}

static void render(const box_t *boxes, int num_boxes) {
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
}

static void frame(void *data) {
    app_t *app = (app_t *)data;
    update(app);
    render(app->boxes, app->num_boxes);
}

static void screen_event_handler(app_t *app, bps_event_t *event) {
    int event_type, buttons;
    int pair[2];

    static bool mouse_pressed = false;

    screen_event_t screen_event = screen_event_get_event(event);

    //Query type of screen event and its location on the screen
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE,
            &event_type);
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_SOURCE_POSITION,
            pair);

    //There is a difference between touch screen events and mouse events
    if (event_type == SCREEN_EVENT_MTOUCH_RELEASE) {
        //This is touch screen event.
        add_cube(app, pair[0], pair[1]);
    } else if (event_type == SCREEN_EVENT_POINTER) {
        //This is a mouse move event, it is applicable to a device with a usb mouse or simulator
        screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS,
                &buttons);

        if ((buttons & SCREEN_LEFT_MOUSE_BUTTON) != 0) {
            //Left mouse button is pressed
            mouse_pressed = true;
        } else if (mouse_pressed) {
            //Left mouse button was released, add a cube
            add_cube(app, pair[0], pair[1]);
            mouse_pressed = false;
        }
    }
}

static void event_handler(bps_event_t *event, int domain, int code, void *data) {
    app_t *app = (app_t *)data;

    if (domain == screen_get_domain()) {
        screen_event_handler(app, event);
    } else if (domain == navigator_get_domain()) {
        if (NAVIGATOR_SWIPE_DOWN == code) {
            app->num_boxes = 0;
        }
    } else if (domain == sensor_get_domain()) {
        if (SENSOR_GRAVITY_READING == code) {
            float z, x, y;
            sensor_event_get_xyz(event, &x, &y, &z);
            app->gravity_x = -x;
            app->gravity_y = -y;
        }
    }
}

static void finalize(void *data) {
    app_t *app = (app_t*)data;
    free(app->boxes);
    free(app);
}

int main(int argc, char **argv) {
    glview_initialize(GLVIEW_API_OPENGLES_11, &frame);
    glview_register_initialize_callback(&initialize);
    glview_register_finalize_callback(&finalize);
    glview_register_event_callback(&event_handler);

    app_t *app = (app_t *)calloc(sizeof(*app), 1);
    if (!app) {
        return EXIT_FAILURE;
    }

    app->boxes = (box_t *)calloc(sizeof(box_t), MAX_BOXES);
    if (!app->boxes) {
        return EXIT_FAILURE;
    }

    glview_set_callback_data(app);

    return glview_loop();
}
