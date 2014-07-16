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

/* VideoWindow
 * ===========
 *
 * This is an application that shows OpenGLES2 working while playing a video
 * via mmrenderer.
 *
 * When the application starts, there is a green "Play" symbol that can be
 * pressed.
 *
 * When the button is pressed, the play button turns into a blue stop symbol
 * and video playback begins.  While the video is playing, the user can chose
 * to stop the video by tapping the blue symbol.  The blue stop symbol will
 * then turn into a green play symbol.  When the green symbol is pressed,
 * playback will continue.
 *
 * The user may swipe from the top of the bevel into the video (down-swipe)
 * in order to hide the symbol.  This is achieved by changing the z-order of
 * the mmrender video surface.
 */

#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <bps/bps.h>
#include <bps/navigator.h>
#include <bps/screen.h>

#include <EGL/egl.h>

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <mm/renderer.h>

#include <screen/screen.h>

#include <sys/strm.h>

static const int ctrl_h = 100;
static const int ctrl_w = 100;

static EGLDisplay   g_egl_disp = EGL_NO_DISPLAY;
static EGLSurface   g_egl_surf = EGL_NO_SURFACE;

static EGLConfig    g_egl_conf = ((EGLConfig)0);
static EGLContext   g_egl_ctx = EGL_NO_CONTEXT;

static screen_context_t   g_screen_ctx;
static screen_window_t    g_screen_win;
static screen_display_t   g_screen_disp;

static int app_id = 0;

GLfloat g_triangle_vertices[6];
GLfloat g_square_vertices[10];


void
terminate_egl_window() {

    // Typical EGL cleanup
    if (g_egl_disp != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_surf != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_disp, g_egl_surf);
            g_egl_surf = EGL_NO_SURFACE;
        }
        if (g_egl_ctx != EGL_NO_CONTEXT) {
            eglDestroyContext(g_egl_disp, g_egl_ctx);
            g_egl_ctx = EGL_NO_CONTEXT;
        }
        if (g_screen_win != NULL) {
            screen_destroy_window(g_screen_win);
            g_screen_win = NULL;
        }
        eglTerminate(g_egl_disp);
        g_egl_disp = EGL_NO_DISPLAY;
    }
    eglReleaseThread();
}

static void
egl_perror(const char *msg) {
    static const char *errmsg[] = {
        "function succeeded",
        "EGL is not initialized, or could not be initialized, for the specified display",
        "cannot access a requested resource",
        "failed to allocate resources for the requested operation",
        "an unrecognized attribute or attribute value was passed in an attribute list",
        "an EGLConfig argument does not name a valid EGLConfig",
        "an EGLContext argument does not name a valid EGLContext",
        "the current surface of the calling thread is no longer valid",
        "an EGLDisplay argument does not name a valid EGLDisplay",
        "arguments are inconsistent",
        "an EGLNativePixmapType argument does not refer to a valid native pixmap",
        "an EGLNativeWindowType argument does not refer to a valid native window",
        "one or more argument values are invalid",
        "an EGLSurface argument does not name a valid surface configured for rendering",
        "a power management event has occurred",
    };

    fprintf(stderr, "%s: %s\n", msg, errmsg[eglGetError() - EGL_SUCCESS]);
}

int
initialize_egl_window(screen_context_t ctx, char * window_group_name) {
    int usage = SCREEN_USAGE_OPENGL_ES1;
    int format = SCREEN_FORMAT_RGBA8888;
    int sensitivity = SCREEN_SENSITIVITY_ALWAYS;
    int rc, num_configs;

    // must be 2 for RGBA8888
    const int num_window_buffers = 2;

    EGLint attrib_list[]= { EGL_RED_SIZE,        8,
                            EGL_GREEN_SIZE,      8,
                            EGL_BLUE_SIZE,       8,
                            EGL_BLUE_SIZE,       8,
                            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
                            EGL_NONE};

    // Simple egl initialization
    g_screen_ctx = ctx;

    g_egl_disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_disp == EGL_NO_DISPLAY) {
        egl_perror("eglGetDisplay");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = eglInitialize(g_egl_disp, NULL, NULL);
    if (rc != EGL_TRUE) {
        egl_perror("eglInitialize");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = eglBindAPI(EGL_OPENGL_ES_API);
    if (rc != EGL_TRUE) {
        egl_perror("eglBindApi");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    if(!eglChooseConfig(g_egl_disp, attrib_list, &g_egl_conf, 1, &num_configs)) {
        terminate_egl_window();
        fprintf(stderr, "eglChooseConfig failed\n");
        return EXIT_FAILURE;
    }

    g_egl_ctx = eglCreateContext(g_egl_disp, g_egl_conf, EGL_NO_CONTEXT, NULL);
    if (g_egl_ctx == EGL_NO_CONTEXT) {
        egl_perror("eglCreateContext");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = screen_create_window(&g_screen_win, g_screen_ctx);
    if (rc) {
        perror("screen_create_window");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    /* Create the window group for our window. We pass
     * the group name to MMR which uses it to 'parent' its CHILD_WINDOW
     * which contains the video. screen_create_window_group() generates
     * the window group name which we fetch by calling
     * screen_get_window_property_cv().
     */
    if (screen_create_window_group(g_screen_win, NULL) != 0) {
        perror("screen_create_window_group");
        return EXIT_FAILURE;
    }

    screen_get_window_property_cv(g_screen_win, SCREEN_PROPERTY_GROUP,
                                  PATH_MAX, window_group_name);

    rc = screen_set_window_property_iv(g_screen_win, SCREEN_PROPERTY_FORMAT, &format);
    if (rc) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = screen_set_window_property_iv(g_screen_win, SCREEN_PROPERTY_USAGE, &usage);
    if (rc) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = screen_set_window_property_iv(g_screen_win, SCREEN_PROPERTY_SENSITIVITY, &sensitivity);
    if (rc) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_SENSITIVITY)");
        terminate_egl_window();
        return EXIT_FAILURE;
    }


    rc = screen_get_window_property_pv(g_screen_win, SCREEN_PROPERTY_DISPLAY, (void **)&g_screen_disp);
    if (rc) {
        perror("screen_get_window_property_pv");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    const char *env = getenv("WIDTH");
    if (0 == env) {
        perror("failed getenv for WIDTH");
        terminate_egl_window();
        return EXIT_FAILURE;
    }
    int width = atoi(env);

    env = getenv("HEIGHT");
    if (0 == env) {
        perror("failed getenv for HEIGHT");
        terminate_egl_window();
        return EXIT_FAILURE;
    }
    int height = atoi(env);

    int size[2] = { width, height };
    rc = screen_set_window_property_iv(g_screen_win, SCREEN_PROPERTY_BUFFER_SIZE, size);
    if (rc) {
        perror("screen_set_window_property_iv");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = screen_create_window_buffers(g_screen_win, num_window_buffers);
    if (rc) {
        perror("screen_create_window_buffers");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    g_egl_surf = eglCreateWindowSurface(g_egl_disp, g_egl_conf, g_screen_win, NULL);
    if (g_egl_surf == EGL_NO_SURFACE) {
        egl_perror("eglCreateWindowSurface");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    rc = eglMakeCurrent(g_egl_disp, g_egl_surf, g_egl_surf, g_egl_ctx);
    if (rc != EGL_TRUE) {
        egl_perror("eglMakeCurrent");
        terminate_egl_window();
        return EXIT_FAILURE;
    }

    // Set the clear color to be transparent
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    return EXIT_SUCCESS;
}


void render(bool paused)
{
    /* Clear the window to be fully transparent, this is important because by
     * default the MM renderer window will be below our window in Z order, so
     * we will only see the video if our window is transparent.  Note that
     * transparency was set using glClearColor().
     */
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);

    if (!paused)
    {
        // Display the Pause button
        glVertexPointer(2, GL_FLOAT, 0, g_square_vertices);
        glColor4f(0.123f, 0.18f, .9f, 1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0 , 5);
    }
    else {
        // Display the Play button
        glVertexPointer(2, GL_FLOAT, 0, g_triangle_vertices);
        glColor4f(0.123f, 0.78f, 0.0, 1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0 , 3);
    }

    glDisableClientState(GL_VERTEX_ARRAY);

    eglSwapBuffers(g_egl_disp, g_egl_surf);
}


/*
 * we know the video has a resolution of 640x480 so it has an aspect ratio of
 * of approximately 1.33.  Based on these facts, we can show the video as large
 * as possible without changing the aspect ratio.
 */
void set_video_aspect_ratio(int width, int height, screen_window_t video_window) {
    const int image_width = 640;
    const int image_height = 480;
    const float image_aspect = (float)image_width / (float)image_height;
    const float aspect_tolerance = 0.1;

    // Set the default window position and size values for fullscreen
    int win_position[2] = {0, 0};
    int win_size[2] = { width, height };

    float screen_aspect = (float)width/(float)height;
    if (fabs(screen_aspect - image_aspect) < aspect_tolerance) {
        /* if the screen is at almost the same aspect as the video, just
         * do full screen.  Nothing to do here.  Fall through and return
         * full screen.
         */
    } else if (screen_aspect < image_aspect) {
        /* The screen is too tall.  We need to center top to bottom, set the
         * width the same as the screen's while maintaining the same aspect
         * ratio.
         */
        win_position[1] = (height - image_height) / 2;
        win_size[1] = width / image_aspect;
    } else {
        /* The screen is too wide.  We need to center left to right, set the
         * height the same as the screen's while maintaining the same aspect
         * ratio.
         */
        win_position[0] = (width - image_width) / 2;
        win_size[0] = height * image_aspect;
    }

    // Set x, y positions of the window screen coordinates
    if (screen_set_window_property_iv(video_window, SCREEN_PROPERTY_POSITION, win_position) != 0) {
        fprintf(stderr, "Failed to set SCREEN_PROPERTY_POSITION\n");
    }

    // Set the width and height of the window
    if (screen_set_window_property_iv(video_window, SCREEN_PROPERTY_SIZE, win_size) != 0) {
        fprintf(stderr, "Failed to set SCREEN_PROPERTY_SIZE\n");
    }

    return;
}

int main(int argc, char *argv[])
{
    int rc;

    // Renderer variables
    mmr_connection_t*     mmr_connection = 0;
    mmr_context_t*        mmr_context = 0;

    // I/O variables
    int                    video_device_output_id = -1;
    int                    audio_device_output_id = -1;

    // Position of the play and stop button.
    static int ctrl_x = 0;
    static int ctrl_y = 0;

    EGLint surface_width;
    EGLint surface_height;

    srand(time(0));
    app_id = rand();

    // I/O devices
    static char *audio_device_url    = "audio:default";
    static char video_device_url[PATH_MAX];

    // Name of video context...with a random number appended.
    static char video_context_name[PATH_MAX];
    rc = snprintf(video_context_name, PATH_MAX, "samplevideocontextname_%d", app_id);
    if (rc >= PATH_MAX) {
        fprintf(stderr, "Video context name too long\n");
    }

    // Video file bundled with our app
    static const char *video_file_relative_path = "app/native/pb_sample.mp4";

    bps_initialize();

    // Create the Screen context.
    if (screen_create_context(&g_screen_ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        fprintf(stderr, "screen_create_context failed\n");
        return EXIT_FAILURE;
    }

    // Window group name is generated by a call to screen_create_window_group()
    // in initialize_egl_window().
    static char window_group_name[PATH_MAX];

    // Create the window and initialize EGL for GL_ES_1 rendering
    rc = initialize_egl_window(g_screen_ctx, window_group_name);
    if (rc != EXIT_SUCCESS) {
        fprintf(stderr, "initialize_egl_window failed\n");
        return EXIT_FAILURE;
    }

    rc = snprintf(video_device_url, PATH_MAX, "screen:?winid=videosamplewindowgroup_%d&wingrp=%s", app_id, window_group_name);
    if (rc >= PATH_MAX) {
        fprintf(stderr, "URL too long\n");
    }

    // Query width and height of the window surface created by utility code
    eglQuerySurface(g_egl_disp, g_egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(g_egl_disp, g_egl_surf, EGL_HEIGHT, &surface_height);
    EGLint err = eglGetError();
    if (err != EGL_SUCCESS) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }

    // Initialize GL for 2D rendering
    glViewport(0, 0, (int)surface_width, (int) surface_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Set world coordinates to coincide with screen pixels
    glScalef(1.0f / (float)surface_width, 1.0f / (float)surface_height, 1.0f);

    // Calculate the location and vertices of the controls
    ctrl_x = (float)surface_width  / 2 - ctrl_w / 2;
    ctrl_y = (float)surface_height / 2 - ctrl_h / 2;

    // Define a Play button
    g_triangle_vertices[0] = ctrl_x;
    g_triangle_vertices[1] = ctrl_y;

    g_triangle_vertices[2] = ctrl_x;
    g_triangle_vertices[3] = ctrl_y + ctrl_h;

    g_triangle_vertices[4] = ctrl_x + ctrl_w;
    g_triangle_vertices[5] = ctrl_y + ctrl_h / 2;

    // Define a Pause button
    g_square_vertices[0] = ctrl_x;
    g_square_vertices[1] = ctrl_y;
    g_square_vertices[2] = ctrl_x;
    g_square_vertices[3] = ctrl_y + ctrl_h;
    g_square_vertices[4] = ctrl_x + ctrl_w;
    g_square_vertices[5] = ctrl_y + ctrl_h;
    g_square_vertices[6] = ctrl_x + ctrl_w;
    g_square_vertices[7] = ctrl_y;
    g_square_vertices[8] = ctrl_x;
    g_square_vertices[9] = ctrl_y;

    // Configure mm-renderer.
    mmr_connection = mmr_connect(NULL);
    if (mmr_connection == NULL) {
        fprintf(stderr, "mmr_connect failed\n");
        return EXIT_FAILURE;
    }

    mmr_context = mmr_context_create(mmr_connection, video_context_name, 0, S_IRWXU|S_IRWXG|S_IRWXO);
    if (mmr_context == NULL) {
        fprintf(stderr, "mmr_context_create failed\n");
        return EXIT_FAILURE;
    }

    // Configure video and audio output.
    video_device_output_id = mmr_output_attach(mmr_context, video_device_url, "video");
    if (video_device_output_id == -1) {
        fprintf(stderr, "mmr_output_attach(video) failed\n");
        return EXIT_FAILURE;
    }

    audio_device_output_id = mmr_output_attach(mmr_context, audio_device_url, "audio");
    if (audio_device_output_id == -1) {
        fprintf(stderr, "mmr_output_attach(audio) failed\n");
        return EXIT_FAILURE;
    }

    // render 'paused'
    render(true);


    // Build up the path where our bundled resource is.
    char cwd[PATH_MAX];
    char media_file[PATH_MAX];
    getcwd(cwd,PATH_MAX);

    rc = snprintf(media_file, PATH_MAX, "file://%s/%s", cwd, video_file_relative_path);
    if ((rc == -1) || (rc >= PATH_MAX)) {
        fprintf(stderr, "snprintf(media_file) failed\n");
        return EXIT_FAILURE;
    }

    // Attach the input media.
    if (mmr_input_attach(mmr_context, media_file, "track") != 0) {
        fprintf(stderr, "mmr_input_attach(track) failed\n");
        return EXIT_FAILURE;
    }


    int video_speed = 0;

    // Set the speed to 0 to pause the video initially
    if (mmr_speed_set(mmr_context, video_speed) != 0) {
        fprintf(stderr, "mmr_speed_set(0) failed\n");
        return EXIT_FAILURE;
    }

    // Change to the play state, although speed is zero
    if (mmr_play(mmr_context) != 0) {
        fprintf(stderr, "mmr_play() failed\n");
        return EXIT_FAILURE;
    }

    screen_request_events(g_screen_ctx);
    navigator_request_events(0);

    screen_window_t video_window = (screen_window_t)0;
    bool app_window_above = true;
    int screen_val;
    int exit_value = EXIT_SUCCESS;

    // Handle keyboard events and stop playback upon user request.
    for (;;) {
        bps_event_t *event = NULL;
        if (bps_get_event(&event, 0) != BPS_SUCCESS) {
            return EXIT_FAILURE;
        }
        if (event) {
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                if (bps_event_get_code(event) == NAVIGATOR_EXIT) {
                    break;
                } else if(NAVIGATOR_SWIPE_DOWN == bps_event_get_code(event)) {
                    if ((screen_window_t)0 != video_window) {

                        app_window_above = !app_window_above;
                        if (app_window_above) {
                            screen_val = 1;     // Show the control
                        } else {
                            screen_val = -1;    // Hide the control
                        }
                        if (screen_set_window_property_iv(video_window, SCREEN_PROPERTY_ZORDER, &screen_val) != 0) {
                            fprintf(stderr, "screen_set_window_property(ZORDER) failed\n");
                            exit_value = EXIT_FAILURE;
                            break;
                        }

                        screen_val = 1;
                        if (screen_set_window_property_iv(video_window, SCREEN_PROPERTY_VISIBLE, &screen_val) != 0) {
                            fprintf(stderr, "screen_set_window_property(VISIBLE) failed\n");
                            exit_value = EXIT_FAILURE;
                            break;
                        }

                        rc = screen_flush_context(g_screen_ctx, SCREEN_WAIT_IDLE);
                        if (rc != 0) {
                            fprintf (stderr, "Warning: Failed to flush\n");
                        }
                    }
                }
            } else if (bps_event_get_domain(event) == screen_get_domain()) {
                screen_event_t screen_event = screen_event_get_event(event);
                int event_type;
                screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &event_type);

                if (event_type == SCREEN_EVENT_CREATE && (video_window == (screen_window_t)0)) {
                    char id[256];

                    rc = screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_WINDOW, (void**)&video_window);
                    if (rc != 0) {
                        fprintf(stderr, "screen_get_event_property(WINDOW) failed\n");
                        exit_value = EXIT_FAILURE;
                        break;
                    }

                    rc = screen_get_window_property_cv(g_screen_win, SCREEN_PROPERTY_GROUP,
                                                  PATH_MAX, id);
                    if (rc != 0) {
                        fprintf(stderr, "screen_get_window_property(ID) failed\n");
                        exit_value = EXIT_FAILURE;
                        break;
                    }

                    /* Set the aspect video of the video. */
                    set_video_aspect_ratio(surface_width, surface_height, video_window);

                    if (strncmp(id, window_group_name, strlen(window_group_name)) != 0) {
                        fprintf(stderr, "window ID mismatch\n");
                        exit_value = EXIT_FAILURE;
                        break;
                    }
                } else if(event_type == SCREEN_EVENT_MTOUCH_TOUCH) {
                    if (video_speed == 0) {
                        video_speed = 1000;
                        render(false);
                    } else {
                        video_speed = 0;
                        render(true);
                    }

                    if (mmr_speed_set(mmr_context, video_speed) != 0) {
                        fprintf(stderr, "mmr_speed_set(%d) failed\n", video_speed);
                        exit_value = EXIT_FAILURE;
                        break;
                    }
                }
            }
        }
    }

    screen_stop_events(g_screen_ctx);

    if (mmr_stop(mmr_context) != 0) {
        fprintf(stderr, "mmr_stop failed\n");
        exit_value = EXIT_FAILURE;
    }

    if (mmr_output_detach(mmr_context, audio_device_output_id) != 0) {
        fprintf(stderr, "mmr_output_detach(audio) failed\n");
        exit_value = EXIT_FAILURE;
    }

    if (mmr_output_detach(mmr_context, video_device_output_id) != 0) {
        fprintf(stderr, "mmr_output_detach(video) failed\n");
        exit_value = EXIT_FAILURE;
    }

    if (mmr_context_destroy(mmr_context) != 0) {
        fprintf(stderr, "mmr_context_destroy failed\n");
        exit_value = EXIT_FAILURE;
    }

    mmr_context = 0;
    video_device_output_id = -1;
    audio_device_output_id = -1;

    mmr_disconnect(mmr_connection);
    mmr_connection = 0;

    bps_shutdown();

    if (screen_destroy_window(g_screen_win) != 0) {
        fprintf(stderr, "screen_destroy_window failed\n");
        exit_value = EXIT_FAILURE;
    }

    if (screen_destroy_context(g_screen_ctx) != 0) {
        fprintf(stderr, "screen_destroy_context failed\n");
        exit_value = EXIT_FAILURE;
    }

    g_screen_ctx = 0;
    g_screen_win = 0;

    return exit_value;
}
