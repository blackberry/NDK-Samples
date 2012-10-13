/*
 * Copyright (c) 2012 by BGmot <ey@tm-k.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <screen/screen.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

const char *hg_id_string = "hourglass";

const char *bar_id_string = "bar";

const char *gles_id_string = "gles"; // OpenGL window

const int barwidth = 32;

screen_window_t screen_bg_win = NULL;

screen_window_t screen_hg_win = NULL;

screen_window_t screen_bar_win = NULL;

screen_window_t screen_gles_win = NULL;

screen_context_t screen_ctx;

screen_window_t create_bg_window(const char *group, int dims[2])
{
	/* Start by creating the application window and window group. */
	screen_window_t screen_win;
	screen_create_window(&screen_win, screen_ctx);
	screen_create_window_group(screen_win, group);

	int vis = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_VISIBLE, &vis);

	int color = 0xffffff00;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_COLOR, &color);

	int zorder = 0;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &zorder);

	int rect[4] = { 0, 0, 1, 1 };
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect+2);
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_SIZE, dims);
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SIZE, dims);

	int pos[2] = { -dims[0], -dims[1] };
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_POSITION, pos);

	screen_buffer_t screen_buf;
	screen_create_window_buffers(screen_win, 1);
	screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);
	screen_post_window(screen_win, screen_buf, 1, rect, 0);

	return screen_win;
}


screen_window_t create_bar_window(const char *group, const char *id, int dims[2])
{
	screen_window_t screen_win;
	screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW);
	screen_join_window_group(screen_win, group);
	screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, strlen(id), id);

	int vis = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_VISIBLE, &vis);

	int zorder = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &zorder);

	int color = 0xff0000ff;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_COLOR, &color);

	int rect[4] = { 0, 0, 1, 1 };
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect+2);

	int pos[2] = { -rect[2], -rect[3] };
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SOURCE_POSITION, pos);
	pos[0] = pos[1] = 0;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_POSITION, pos);
	int size[2] = {barwidth,dims[1]};
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SIZE, size);


	screen_buffer_t screen_buf;
	screen_create_window_buffers(screen_win, 1);
	screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);
	screen_post_window(screen_win, screen_buf, 1, rect, 0);

	return screen_win;
}


screen_window_t create_hg_window(const char *group, const char *id, int dims[2])
{
	int i, j;

	screen_window_t screen_win;
	screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW);
	screen_join_window_group(screen_win, group);
	screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, strlen(id), id);

	int flag = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_STATIC, &flag);

	int vis = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_VISIBLE, &vis);

	int zorder = 2;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &zorder);

	int format = SCREEN_FORMAT_RGBA8888;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format);

	int usage = SCREEN_USAGE_WRITE;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);

	int transparency = SCREEN_TRANSPARENCY_SOURCE_OVER;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_TRANSPARENCY, &transparency);

	int rect[4] = { 0, 0, 100, 100 };
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, rect+2);
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SIZE, &rect[2]);

	screen_buffer_t screen_buf;
	screen_create_window_buffers(screen_win, 1);
	screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);

	char *ptr = NULL;
	screen_get_buffer_property_pv(screen_buf, SCREEN_PROPERTY_POINTER, (void **)&ptr);

	int stride = 0;
	screen_get_buffer_property_iv(screen_buf, SCREEN_PROPERTY_STRIDE, &stride);

	for (i = 0; i < rect[3]; i++, ptr += stride) {
		for (j = 0; j < rect[2]; j++) {
			ptr[j*4] = 0xa0;
			ptr[j*4+1] = 0xa0;
			ptr[j*4+2] = 0xa0;
			ptr[j*4+3] = ((j >= i && j <= rect[3]-i) || (j <= i && j >= rect[3]-i)) ? 0xff : 0;
		}
	}

	screen_post_window(screen_win, screen_buf, 1, rect, 0);

	return screen_win;
}

// Variables for EGL
EGLDisplay egl_disp;
EGLSurface egl_surf;
EGLContext egl_ctx;


static void egl_perror(const char *msg) {
	// Borrowed from bbutil.c
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
        "unknown error code"
    };

    int message_index = eglGetError() - EGL_SUCCESS;
    if (message_index < 0 || message_index > 14)
        message_index = 15;
    fprintf(stderr, "%s: %s\n", msg, errmsg[message_index]);
}

void gl_terminate(screen_window_t *screen_win) {
	// Borrowed from bbutil.c
    // EGL cleanup
    if (egl_disp != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_surf != EGL_NO_SURFACE) {
            eglDestroySurface(egl_disp, egl_surf);
            egl_surf = EGL_NO_SURFACE;
        }
        if (egl_ctx != EGL_NO_CONTEXT) {
            eglDestroyContext(egl_disp, egl_ctx);
            egl_ctx = EGL_NO_CONTEXT;
        }
        if (screen_win != NULL) {
            screen_destroy_window(*screen_win);
            screen_win = NULL;
        }
        eglTerminate(egl_disp);
        egl_disp = EGL_NO_DISPLAY;
    }
    eglReleaseThread();
}

int create_gles_window(const char *group, const char *id, int dims[2])
{
	int i, j;

	screen_window_t screen_win;
	screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW);
	screen_join_window_group(screen_win, group);
	screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, strlen(id), id);

	int flag = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_STATIC, &flag);

	int vis = 1;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_VISIBLE, &vis);

	int zorder = 3;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &zorder);

	int format = SCREEN_FORMAT_RGBA8888;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format);

    int usage = SCREEN_USAGE_OPENGL_ES2;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);

	int transparency = SCREEN_TRANSPARENCY_SOURCE_OVER;
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_TRANSPARENCY, &transparency);

	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, dims);
	screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_SIZE, dims);

	screen_buffer_t screen_buf;
	screen_create_window_buffers(screen_win, 1);
	screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);

	int rect[4] = { 0, 0, dims[0], dims[1] }; // OpenGL window covers all display
	screen_post_window(screen_win, screen_buf, 1, rect, 0);

	// EGL initialization stuff starts here
	EGLint surface_width, surface_height;
	GLuint rendering_program;
	EGLConfig egl_conf;

    EGLint interval = 1;
    int rc, num_configs;

    EGLint attrib_list[]= { EGL_RED_SIZE,        8,
                            EGL_GREEN_SIZE,      8,
                            EGL_BLUE_SIZE,       8,
                            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                            EGL_NONE};
    EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    //EGL initialization
    egl_disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_disp == EGL_NO_DISPLAY) {
        egl_perror("bbutil_init_egl: eglGetDisplay");
        gl_terminate(&screen_win);
        return EXIT_FAILURE;
    }

    rc = eglInitialize(egl_disp, NULL, NULL);
    if (rc != EGL_TRUE) {
        egl_perror("bbutil_init_egl: eglInitialize");
        gl_terminate(&screen_win);
        return EXIT_FAILURE;
    }

    rc = eglBindAPI(EGL_OPENGL_ES_API);
    if (rc != EGL_TRUE) {
        egl_perror("bbutil_init_egl: eglBindApi");
        gl_terminate(&screen_win);
        return EXIT_FAILURE;
    }

    if(!eglChooseConfig(egl_disp, attrib_list, &egl_conf, 1, &num_configs)) {
        gl_terminate(&screen_win);
        return EXIT_FAILURE;
    }

    egl_ctx = eglCreateContext(egl_disp, egl_conf, EGL_NO_CONTEXT, attributes);
    if (egl_ctx == EGL_NO_CONTEXT) {
        egl_perror("bbutil_init_egl: eglCreateContext");
        gl_terminate(&screen_win);
        return EXIT_FAILURE;
    }

    // Bound EGL serface to our Native Window
    egl_surf = eglCreateWindowSurface(egl_disp, egl_conf, screen_win, NULL);
    if (egl_surf == EGL_NO_SURFACE) {
        egl_perror("eglCreateWindowSurface");
        gl_terminate(&screen_win);
        return 1;
    }

    rc = eglMakeCurrent(egl_disp, egl_surf, egl_surf, egl_ctx);
    if (rc != EGL_TRUE) {
        egl_perror("eglMakeCurrent");
        gl_terminate(&screen_win);
        return 1;
    }

    rc = eglSwapInterval(egl_disp, interval);
    if (rc != EGL_TRUE) {
        egl_perror("eglSwapInterval");
        gl_terminate(&screen_win);
        return 1;
    }

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Last byte is 'transparency', it's important to have it 0.0f
    glEnable(GL_CULL_FACE);

    // Enable 2d
    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    EGLint err = eglGetError();
    if (err != 0x3000) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }
    glViewport(0, 0, surface_width, surface_height);

    // Create shaders
    const char *v_source =
    		"attribute vec4 vPosition;\n"
    		"void main()\n"
    		"{\n"
			"gl_Position = vPosition;\n"
			"}";
    const char *f_source =
    		"precision mediump float; \n"
    		"void main()\n"
    		"{\n"
    		"gl_FragColor = vec4(1.0, 0.0, 0.0, 0.7); \n" // 0.7 means we make it 'a bit' transparent
    		"}	\n";

    // Compile the vertex shader
    GLint status;
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (!vs) {
        fprintf(stderr, "Failed to create vertex shader: %d\n", glGetError());
        return EXIT_FAILURE;
    } else {
        glShaderSource(vs, 1, &v_source, 0);
        glCompileShader(vs);
        glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
        if (GL_FALSE == status) {
            GLchar log[256];
            glGetShaderInfoLog(vs, 256, NULL, log);

            fprintf(stderr, "Failed to compile vertex shader: %s\n", log);

            glDeleteShader(vs);
            return EXIT_FAILURE;
        }
    }

    // Compile the fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        fprintf(stderr, "Failed to create fragment shader: %d\n", glGetError());
        return EXIT_FAILURE;
    } else {
        glShaderSource(fs, 1, &f_source, 0);
        glCompileShader(fs);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
        if (GL_FALSE == status) {
            GLchar log[256];
            glGetShaderInfoLog(fs, 256, NULL, log);

            fprintf(stderr, "Failed to compile fragment shader: %s\n", log);

            glDeleteShader(vs);
            glDeleteShader(fs);

            return EXIT_FAILURE;
        }
    }

    // Create and link the program
    rendering_program = glCreateProgram();
    if (rendering_program)
    {
        glAttachShader(rendering_program, vs);
        glAttachShader(rendering_program, fs);
        glLinkProgram(rendering_program);

        glGetProgramiv(rendering_program, GL_LINK_STATUS, &status);
        if (status == GL_FALSE){
            GLchar log[256];
            glGetProgramInfoLog(fs, 256, NULL, log);

            fprintf(stderr, "Failed to link text rendering shader program: %s\n", log);

            glDeleteProgram(rendering_program);
            rendering_program = 0;

            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Failed to create a shader program\n");

        glDeleteShader(vs);
        glDeleteShader(fs);
        return EXIT_FAILURE;
    }
    // We don't need the shaders anymore - the program is enough
    glDeleteShader(fs);
    glDeleteShader(vs);

    glUseProgram(rendering_program); // In this example we use only one rendering program, so we can call this function once

    screen_gles_win = screen_win;

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int pos[2] = {0, 0};
	int size[2];
	int vis = 0;
	int type;

	screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT);

	int count = 0;
	screen_get_context_property_iv(screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &count);
	screen_display_t *screen_disps = calloc(count, sizeof(screen_display_t));
	screen_get_context_property_pv(screen_ctx, SCREEN_PROPERTY_DISPLAYS, (void **)screen_disps);

	screen_display_t screen_disp = screen_disps[0];
	free(screen_disps);

	int dims[2] = { 0, 0 };
	screen_get_display_property_iv(screen_disp, SCREEN_PROPERTY_SIZE, dims);

	char str[16];
	snprintf(str, sizeof(str), "%d", getpid());
	screen_bg_win = create_bg_window(str, dims);

	screen_bar_win = create_bar_window(str, bar_id_string, dims);
	screen_hg_win = create_hg_window(str, hg_id_string, dims);
	if ( create_gles_window(str, gles_id_string, dims) != EXIT_SUCCESS){
		fprintf(stderr, "Could not initialize OpenGL window. Exiting...\n");
		screen_destroy_context(screen_ctx);

		return EXIT_FAILURE;
	}

	screen_event_t screen_ev;
	screen_create_event(&screen_ev);

	// Now draw our OpenGL stuff, it does not change so we need to do it only once
	GLfloat vVertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	int rc = eglSwapBuffers(egl_disp, egl_surf);
    if (rc != EGL_TRUE) {
        egl_perror("eglSwapBuffers");
    }

    while (1) {
		do {
			screen_get_event(screen_ctx, screen_ev, vis ? 0 : ~0);
			screen_get_event_property_iv(screen_ev, SCREEN_PROPERTY_TYPE, &type);

			if (type == SCREEN_EVENT_CLOSE) {

				screen_window_t screen_win;
				screen_get_event_property_pv(screen_ev, SCREEN_PROPERTY_WINDOW, (void **)&screen_win);


				if (screen_win == screen_bar_win) {
					screen_bar_win = NULL;
				} else if (screen_win == screen_hg_win) {
					screen_hg_win = NULL;
				} else if (screen_win == screen_gles_win) {
					screen_gles_win = NULL;
				}

				screen_destroy_window(screen_win);

				if (!screen_bar_win || !screen_hg_win || !screen_gles_win) {
					vis = 0;
				}
			}
		} while (type != SCREEN_EVENT_NONE);

		if (vis) {
			if (++pos[0] > dims[0] - barwidth) {
				pos[0] = 0;
			}
			screen_set_window_property_iv(screen_bar_win, SCREEN_PROPERTY_POSITION, pos);
			screen_flush_context(screen_ctx, SCREEN_WAIT_IDLE);
		}
	}


	screen_destroy_event(screen_ev);
	screen_destroy_context(screen_ctx);

	return EXIT_SUCCESS;
}
