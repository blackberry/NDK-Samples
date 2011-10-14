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

#ifndef _UTILITY_H_INCLUDED
#define _UTILITY_H_INCLUDED

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <screen/screen.h>
#include <sys/platform.h>

extern EGLDisplay egl_disp;
extern EGLSurface egl_surf;

typedef struct{
    GLuint texture_handle[128];
    GLuint font_texture;
    float pt;
    float advance[128];
    float width[128];
    float height[128];
    float tex_x1[128];
    float tex_x2[128];
    float tex_y1[128];
    float tex_y2[128];
    float offset_x[128];
    float offset_y[128];
    bool initialized;
} font_t;

enum RENDERING_API {GL_ES_1 = EGL_OPENGL_ES_BIT, GL_ES_2 = EGL_OPENGL_ES2_BIT, VG = EGL_OPENVG_BIT};

/**
 * Initializes EGL, GL and loads a default font
 *
 * \param libscreen context that will be used for EGL setup
 * \return EXIT_SUCCESS if initialization succeeded otherwise EXIT_FAILURE
 */
int bbutil_init(screen_context_t ctx, enum RENDERING_API api);

/**
 * Initializes EGL
 *
 * \param libscreen context that will be used for EGL setup
 * \return EXIT_SUCCESS if initialization succeeded otherwise EXIT_FAILURE
 */
int bbutil_init_egl(screen_context_t ctx, enum RENDERING_API api);

/**
 * Initializes GL 1.1 for simple 2D rendering. GL2 initialization will be added at a later point.
 *
 * \return EXIT_SUCCESS if initialization succeeded otherwise EXIT_FAILURE
 */
int bbutil_init_gl2d();

/**
 * Terminates EGL
 */
void bbutil_terminate();

/**
 * Swaps default bbutil window surface to the screen
 */
void bbutil_swap();

/**
 * Clears the screen of any existing text.
 * NOTE: must be called after a successful return from bbutil_init() or bbutil_init_egl() call
 */
void bbutil_clear();

/**
 * Loads the font from the specified font file.
 * NOTE: should be called after a successful return from bbutil_init() or bbutil_init_egl() call
 * \param font_file string indicating the absolute path of the font file
 * \param point_size used for glyph generation
 * \param dpi used for glyph generation
 * \return EXIT_SUCCESS if initialization succeeded otherwise EXIT_FAILURE
 */
int bbutil_load_font(const char* font_file, int point_size, int dpi);

/**
 * Renders the specified message using current font starting from the specified
 * bottom left coordinates.
 * NOTE: must be called after a successful return from bbutil_init() or bbutil_init_egl() call

 *
 * \param msg the message to display
 * \param x, y position of the bottom-left corner of text string in world coordinate space
 * \param scale allows to change size of text
 */
void bbutil_render_text(char* msg, float x, float y, float scale);

/**
 * Returns the non-scaled width and height of a string
 * NOTE: must be called after a successful return from bbutil_init() or bbutil_init_egl() call

 *
 * \param msg the message to get the size of
 * \param scale intended size of text
 * \param return pointer for width of a string
 * \param return pointer for height of a string
 */
void bbutil_measure_text(char* msg, float scale, float* width, float *height);

/**
 * Creates and loads a texture from a png file
 * NOTE: must be called after a successful return from bbutil_init() or bbutil_init_egl() call

 *
 * \param filename path to texture png
 * \param return width of texture
 * \param return height of texture
 * \param return gl texture handle
 * \return EXIT_SUCCESS if texture loading succeeded otherwise EXIT_FAILURE
 */

int bbutil_load_texture(const char* filename, int *width, int *height, float* tex_x, float* tex_y, GLuint *tex);

/**
 * Returns dpi for a given screen

 *
 * \param ctx path libscreen context that corresponds to display of interest
 * \return dpi for a given screen
 */

int bbutil_calculate_dpi(screen_context_t ctx);

#endif
