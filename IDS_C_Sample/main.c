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

#include "bbutil.h"

#include <pthread.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>

#include <screen/screen.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "identity.h"

static char titleText[ 256 ] = { "Welcome" };
static char statusText[ 256 ] = { 0 };
static char countText[ 256 ] = { 0 };
static pthread_mutex_t textMux = PTHREAD_MUTEX_INITIALIZER;

static GLfloat button_vertices[8];
static GLfloat button_tex_coord[8];
static GLuint button;
static screen_context_t screen_cxt;
static screen_window_t screen_wnd;
static font_t* font;
static float width, height;
static int shutdown;
static float button_size_x, button_size_y;
static float pos_x, pos_y;

static int resize();
static void render();

void setTitleText( const char* text ) {
    if( !text ) {
        return;
    }
    pthread_mutex_lock( &textMux );
    strncpy( titleText, text, sizeof( titleText ) );
    pthread_mutex_unlock( &textMux );
    render();
}

void setStatusText( const char* text ) {
    if( !text ) {
        return;
    }
    pthread_mutex_lock( &textMux );
    strncpy( statusText, text, sizeof( statusText ) );
    pthread_mutex_unlock( &textMux );
    render();
}

void setCountText( const char* text ) {
    if( !text ) {
        return;
    }
    pthread_mutex_lock( &textMux );
    strncpy( countText, text, sizeof( countText ) );
    pthread_mutex_unlock( &textMux );
    render();
}

static void handleClick(int x, int y) {
    if( ( y > ( ( height / 2 ) - ( button_size_y / 2 ) ) )
     && ( y < ( ( height / 2 ) + ( button_size_y / 2 ) ) )
     &&  ( x > ( ( width / 2 ) - ( button_size_x / 2 ) ) )
     && ( x < ( ( width / 2 ) + ( button_size_x / 2 ) ) ) ) {
        identity_trigger_set_data();
    }
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
        //Handle touch screen event
        handleClick(pair[0], pair[1]);

    } else if (screen_val == SCREEN_EVENT_POINTER) {
        //This is a mouse move event, it is applicable to a device with a usb mouse or simulator
        screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS,
                &buttons);

        if (buttons == SCREEN_LEFT_MOUSE_BUTTON) {
            //Left mouse button is pressed
            mouse_pressed = true;
        } else {
            if (mouse_pressed) {
                //Left mouse button was released, handle left click
                handleClick(pair[0], pair[1]);
                mouse_pressed = false;
            }
        }
    }
}

static void handleNavigatorEvent(bps_event_t *event) {
    switch (bps_event_get_code(event)) {
    case NAVIGATOR_ORIENTATION_CHECK:
        //Signal navigator that we intend to resize
        navigator_orientation_check_response(event, true);
        break;
    case NAVIGATOR_ORIENTATION:
        if (EXIT_FAILURE == resize(event)) {
            shutdown = true;
        }
        break;
    case NAVIGATOR_EXIT:
        shutdown = true;
        break;
    case NAVIGATOR_WINDOW_INACTIVE:
        //Wait for NAVIGATOR_WINDOW_ACTIVE event
        for (;;) {
            if (BPS_SUCCESS != bps_get_event(&event, -1)) {
                fprintf(stderr, "bps_get_event failed\n");
                break;
            }

            if (event && (bps_event_get_domain(event) == navigator_get_domain())) {
                int code = bps_event_get_code(event);
                if (code == NAVIGATOR_EXIT) {
                    shutdown = true;
                    break;
                } else if (code == NAVIGATOR_WINDOW_ACTIVE) {
                    break;
                }
            }
        }
        break;
    }
}

static void handle_events() {
    //Request and process available BPS events
    for(;;) {
        bps_event_t *event = NULL;
        if (BPS_SUCCESS != bps_get_event(&event, 0)) {
            fprintf(stderr, "bps_get_event failed\n");
            break;
        }

        if (event) {
            int domain = bps_event_get_domain(event);

            if (domain == screen_get_domain()) {
                handleScreenEvent(event);
            } else if (domain == navigator_get_domain()) {
                handleNavigatorEvent(event);
            }
        } else {
            break;
        }
    }
}

static int resize(bps_event_t *event) {
    //Query width and height of the window surface created by utility code
    EGLint surface_width, surface_height;

    if (event) {
        int angle = navigator_event_get_orientation_angle(event);

        //Let bbutil rotate current screen surface to this angle
        if (EXIT_FAILURE == bbutil_rotate_screen_surface(angle)) {
            fprintf(stderr, "Unable to handle orientation change\n");
            return EXIT_FAILURE;
        }
    }

    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    EGLint err = eglGetError();
    if (err != 0x3000) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }

    width = (float) surface_width;
    height = (float) surface_height;

    if (event) {
        render();

        navigator_done_orientation(event);
    }

    return EXIT_SUCCESS;
}

static int initialize() {
    EGLint surface_width, surface_height;

    //Load button texture
    float tex_x = 1.0f, tex_y = 1.0f;

    //Load texture for button
    int size_x = 544, size_y = 207;

    if (EXIT_SUCCESS
            != bbutil_load_texture("app/native/button.png",
                    NULL, NULL, &tex_x, &tex_y, &button)) {
        fprintf(stderr, "Unable to load button texture\n");
    }

    button_vertices[0] = 0.0f;
    button_vertices[1] = 0.0f;
    button_vertices[2] = size_x;
    button_vertices[3] = 0.0f;
    button_vertices[4] = 0.0f;
    button_vertices[5] = size_y;
    button_vertices[6] = size_x;
    button_vertices[7] = size_y;

    button_tex_coord[0] = 0.0f;
    button_tex_coord[1] = 0.0f;
    button_tex_coord[2] = tex_x;
    button_tex_coord[3] = 0.0f;
    button_tex_coord[4] = 0.0f;
    button_tex_coord[5] = tex_y;
    button_tex_coord[6] = tex_x;
    button_tex_coord[7] = tex_y;

    button_size_x = (float) size_x;
    button_size_y = (float) size_y;

    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    EGLint err = eglGetError();
    if (err != 0x3000) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }

    width = (float) surface_width;
    height = (float) surface_height;

    pos_x = 0.0f;
    pos_y = 0.0f;

    //Load a typical arial font to use for our color menu
    int dpi = bbutil_calculate_dpi(screen_cxt);

    if (dpi == EXIT_FAILURE) {
        fprintf(stderr, "Unable to calculate dpi\n");
        return EXIT_FAILURE;
    }

    //As bbutil renders text using device-specifc dpi, we need to compute a point size
    //for the font, so that the text string fits into the bubble. Note that Playbook is used
    //as a reference point in this equation as we know that at dpi of 170, font with point size of
    //15 fits into the bubble texture.

    int point_size = (int)(15.0f / ((float)dpi / 170.0f ));

    font = bbutil_load_font("/usr/fonts/font_repository/monotype/arial.ttf", point_size, dpi);

    if (!font) {
       return EXIT_FAILURE;
    }

    //Initialize positions of graphics assets on the screen, but don't resize the surface
    if (EXIT_FAILURE == resize(NULL)) {
        fprintf(stderr, "Initialize surface\n");
        return EXIT_FAILURE;
    }

    //Common gl setup
    glShadeModel(GL_SMOOTH);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_CULL_FACE);

    return EXIT_SUCCESS;
}

static void render() {
    //Typical render pass
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, (int) width, (int) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, width / height, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1.0f / height, 1.0f / height, 1.0f);

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    pos_x = ( width / 2 ) - ( button_size_x / 2 );
    pos_y = ( height / 2 ) - ( button_size_y / 2 );
    glTranslatef(pos_x, pos_y, 0.0f);

    glVertexPointer(2, GL_FLOAT, 0, button_vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, button_tex_coord);
    glBindTexture(GL_TEXTURE_2D, button);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glTranslatef(-pos_x, -pos_y, 0.0f);

    pthread_mutex_lock( &textMux );
    bbutil_render_text(font, titleText, 10.0f, height - 100, 0.35f, 0.35f, 0.35f, 1.0f);
    bbutil_render_text(font, statusText, 10.0f, 10.0f, 0.35f, 0.35f, 0.35f, 1.0f);
    bbutil_render_text(font, countText, 10.0f, pos_y - 30, 0.35f, 0.35f, 0.35f, 1.0f);
    pthread_mutex_unlock( &textMux );

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);

    //Use utility code to update the screen
    bbutil_swap();
}

int main(int argc, char *argv[]) {
    char windowGroupId[64] = {0};
    //Create a screen context that will be used to create an EGL surface to to receive libscreen events
    screen_create_context(&screen_cxt, 0);

    // Get the application's window group id so that it can be passed the identity library in cases
    // where a user may need to be interacted with on the application's behalf.
    screen_create_window(&screen_wnd, screen_cxt);
    screen_create_window_group( screen_wnd, NULL );
    screen_get_window_property_cv( screen_wnd, SCREEN_PROPERTY_GROUP, sizeof( windowGroupId ), windowGroupId );

    //Initialize BPS library
    bps_initialize();

    //Use utility code to initialize EGL for rendering with GL ES 1.1
    if (EXIT_SUCCESS != bbutil_init_egl(screen_cxt)) {
        fprintf(stderr, "bbutil_init_egl failed\n");
        bbutil_terminate();
        screen_destroy_window(screen_wnd);
        screen_destroy_context(screen_cxt);
        return 0;
    }

    //Initialize application logic
    if (EXIT_SUCCESS != initialize()) {
        fprintf(stderr, "initialize failed\n");
        bbutil_terminate();
        screen_destroy_window(screen_wnd);
        screen_destroy_context(screen_cxt);
        return 0;
    }

    //Signal BPS library that navigator and screen events will be requested
    if (BPS_SUCCESS != screen_request_events(screen_cxt)) {
        fprintf(stderr, "screen_request_events failed\n");
        bbutil_terminate();
        screen_destroy_window(screen_wnd);
        screen_destroy_context(screen_cxt);
        return 0;
    }

    if (BPS_SUCCESS != navigator_request_events(0)) {
        fprintf(stderr, "navigator_request_events failed\n");
        bbutil_terminate();
        screen_destroy_window(screen_wnd);
        screen_destroy_context(screen_cxt);
        return 0;
    }



    if( EXIT_SUCCESS != identity_initialize( windowGroupId ) ) {
        fprintf(stderr, "Initialization of IDS Library failed\n");
        bbutil_terminate();
        screen_destroy_window(screen_wnd);
        screen_destroy_context(screen_cxt);
        return 0;
    }

    identity_trigger_get_username();

    // ENTER APPLICATION EVENT LOOP
    while (!shutdown) {
        // Handle bps events, includes input and fd changes
        handle_events();
    }

    identity_shutdown();

    //Stop requesting events from libscreen
    screen_stop_events(screen_cxt);

    //Use utility code to terminate EGL setup
    bbutil_terminate();

    //Shut down BPS library for this process
    bps_shutdown();

    screen_destroy_window(screen_wnd);
    //Destroy libscreen context
    screen_destroy_context(screen_cxt);
    return 0;
}
