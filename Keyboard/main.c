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

#include <bps/virtualkeyboard.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>


#include <glview/glview.h>
#include <GLES/gl.h>

#include <screen/screen.h>
#include <sys/keycodes.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define ANGLE_INCREMENT 3.0f
#define CIRCLE_DEGREES 360.0f

static const GLfloat vertices[] = {
    -0.25f, -0.25f,
     0.25f, -0.25f,
    -0.25f,  0.25f,
     0.25f,  0.25f
};

static const GLfloat colors[] = {
    1.0f, 0.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f
};

static float angle = 0.0;
static bool keyboard_visible = false;

static void
initialize(void *p)
{
    virtualkeyboard_request_events(0);
    virtualkeyboard_show();

    unsigned int surface_width, surface_height;
    glview_get_size(&surface_width, &surface_height);

    glShadeModel(GL_SMOOTH);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glViewport(0, 0, surface_width, surface_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, (float) (surface_width) / (float) (surface_height), 0.0f,
            1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef((float) (surface_width) / (float) (surface_height) / 2, 0.5f,
            0.0f);
}

static void
render(void *p)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, colors);

    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    glDrawArrays(GL_TRIANGLE_STRIP, 0 , 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

static void
event(bps_event_t *event, int domain, int code, void *p)
{
    if (virtualkeyboard_get_domain() == domain) {
        switch (code) {
        case VIRTUALKEYBOARD_EVENT_VISIBLE:
            keyboard_visible = true;
            break;
        case VIRTUALKEYBOARD_EVENT_HIDDEN:
            keyboard_visible = false;
            break;
        }
    } else if (screen_get_domain() == domain) {

        screen_event_t screen_event = screen_event_get_event(event);

        int screen_val;
        screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

        switch (screen_val) {
        case SCREEN_EVENT_MTOUCH_TOUCH:
            if (!keyboard_visible) {
                virtualkeyboard_show();
            }
            break;
        case SCREEN_EVENT_KEYBOARD:
            screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_FLAGS, &screen_val);

            if (screen_val & KEY_DOWN) {
                screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM,&screen_val);

                fprintf(stderr, "The '%c' key was pressed\n", (char)screen_val);

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
}

int
main(int argc, char *argv[])
{
    glview_initialize(GLVIEW_API_OPENGLES_11, &render);
    glview_register_initialize_callback(&initialize);
    glview_register_event_callback(&event);
    return glview_loop();
}
