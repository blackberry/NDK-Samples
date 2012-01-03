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

#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/keycodes.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#include "bbutil.h"

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "png.h"

EGLDisplay egl_disp;
EGLSurface egl_surf;

static EGLConfig egl_conf;
static EGLContext egl_ctx;

static screen_context_t screen_ctx;
static screen_window_t screen_win;

static font_t current_font;
static const char *BBUTIL_DEFAULT_FONT = "/usr/fonts/font_repository/monotype/arial.ttf";

static int initialized = 0;

static void
bbutil_egl_perror(const char *msg) {
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
EGLConfig bbutil_choose_config(EGLDisplay egl_disp, enum RENDERING_API api) {
    EGLConfig egl_conf = (EGLConfig)0;
    EGLConfig *egl_configs;
    EGLint egl_num_configs;
    EGLint val;
    EGLBoolean rc;
    EGLint i;

    rc = eglGetConfigs(egl_disp, NULL, 0, &egl_num_configs);
    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglGetConfigs");
        return egl_conf;
    }
    if (egl_num_configs == 0) {
        fprintf(stderr, "eglGetConfigs: could not find a configuration\n");
        return egl_conf;
    }

    egl_configs = malloc(egl_num_configs * sizeof(*egl_configs));
    if (egl_configs == NULL) {
        fprintf(stderr, "could not allocate memory for %d EGL configs\n", egl_num_configs);
        return egl_conf;
    }

    rc = eglGetConfigs(egl_disp, egl_configs,
        egl_num_configs, &egl_num_configs);
    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglGetConfigs");
        free(egl_configs);
        return egl_conf;
    }

    for (i = 0; i < egl_num_configs; i++) {
        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_SURFACE_TYPE, &val);
        if (!(val & EGL_WINDOW_BIT)) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_RENDERABLE_TYPE, &val);
        if (!(val & api)) {
        	continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_DEPTH_SIZE, &val);
        if ((api & (GL_ES_1|GL_ES_2)) && (val == 0)) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_RED_SIZE, &val);
        if (val != 8) {
            continue;
        }
        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_GREEN_SIZE, &val);
        if (val != 8) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_BLUE_SIZE, &val);
        if (val != 8) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_BUFFER_SIZE, &val);
        if (val != 32) {
            continue;
        }

        egl_conf = egl_configs[i];
        break;
    }

    free(egl_configs);

    if (egl_conf == (EGLConfig)0) {
        fprintf(stderr, "bbutil_choose_config: could not find a matching configuration\n");
    }

    return egl_conf;
}

int
bbutil_init_egl(screen_context_t ctx, enum RENDERING_API api) {
    int usage;
    int format = SCREEN_FORMAT_RGBX8888;
    int nbuffers = 2;
    EGLint interval = 1;
    int rc;
    EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    if (api == GL_ES_1) {
    	usage = SCREEN_USAGE_OPENGL_ES1;
    } else if (api == GL_ES_2) {
    	usage = SCREEN_USAGE_OPENGL_ES2;
    } else if (api == VG) {
    	usage = SCREEN_USAGE_OPENVG;
    } else {
    	fprintf(stderr, "invalid api setting\n");
    	return EXIT_FAILURE;
    }

    //Simple egl initialization
    screen_ctx = ctx;

    egl_disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_disp == EGL_NO_DISPLAY) {
        bbutil_egl_perror("eglGetDisplay");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = eglInitialize(egl_disp, NULL, NULL);
    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglInitialize");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    if ((api == GL_ES_1) || (api == GL_ES_2)) {
    	rc = eglBindAPI(EGL_OPENGL_ES_API);
    } else if (api == VG) {
    	rc = eglBindAPI(EGL_OPENVG_API);
    }

    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglBindApi");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    egl_conf = bbutil_choose_config(egl_disp, api);
    if (egl_conf == (EGLConfig)0) {
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    if (api == GL_ES_2) {
    	egl_ctx = eglCreateContext(egl_disp, egl_conf, EGL_NO_CONTEXT, attributes);
    } else {
    	egl_ctx = eglCreateContext(egl_disp, egl_conf, EGL_NO_CONTEXT, NULL);
    }

    if (egl_ctx == EGL_NO_CONTEXT) {
        bbutil_egl_perror("eglCreateContext");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = screen_create_window(&screen_win, screen_ctx);
    if (rc) {
        perror("screen_create_window");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format);
    if (rc) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);
    if (rc) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = screen_create_window_buffers(screen_win, nbuffers);
    if (rc) {
        perror("screen_create_window_buffers");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    egl_surf = eglCreateWindowSurface(egl_disp, egl_conf, screen_win, NULL);
    if (egl_surf == EGL_NO_SURFACE) {
        bbutil_egl_perror("eglCreateWindowSurface");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = eglMakeCurrent(egl_disp, egl_surf, egl_surf, egl_ctx);
    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglMakeCurrent");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    rc = eglSwapInterval(egl_disp, interval);
    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglSwapInterval");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    initialized = true;

    return EXIT_SUCCESS;
}

int
bbutil_init_gl2d() {
    EGLint surface_width, surface_height;

    if ((egl_disp == EGL_NO_DISPLAY) || (egl_surf == EGL_NO_SURFACE) ){
    	return EXIT_FAILURE;
    }

	eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    glShadeModel(GL_SMOOTH);

    //bbutil sets clear color to white
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glViewport(0, 0, surface_width, surface_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, (float)(surface_width) / (float)(surface_height), 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    return EXIT_SUCCESS;
}

int
bbutil_init(screen_context_t ctx, enum RENDERING_API api) {
	if (EXIT_SUCCESS != bbutil_init_egl(ctx, api)) {
		return EXIT_FAILURE;
	}

	if ((GL_ES_1 == api) && (EXIT_SUCCESS != bbutil_init_gl2d())) {
		return EXIT_FAILURE;
	}

	int dpi = bbutil_calculate_dpi(ctx);

	if ((GL_ES_1 == api) && (EXIT_SUCCESS != bbutil_load_font(BBUTIL_DEFAULT_FONT, 10, dpi))) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void
bbutil_terminate() {
    //Typical EGL cleanup
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
	        screen_destroy_window(screen_win);
	        screen_win = NULL;
	    }
	    eglTerminate(egl_disp);
	    egl_disp = EGL_NO_DISPLAY;
	}
	eglReleaseThread();

	initialized = false;
}

void
bbutil_swap() {
    int rc = eglSwapBuffers(egl_disp, egl_surf);
    if (rc != EGL_TRUE) {
        bbutil_egl_perror("eglSwapBuffers");
    }
}

void
bbutil_clear() {
    glClear(GL_COLOR_BUFFER_BIT);
}

/* Finds the next power of 2 */
static inline int
nextp2(int x)
{
    int val = 1;
    while(val < x) val <<= 1;
    return val;
}

int bbutil_load_font(const char* path, int point_size, int dpi) {
    FT_Library library;
    FT_Face face;
    int c;
    int i, j;

    if (!initialized) {
    	fprintf(stderr, "EGL has not been initialized\n");
    	return EXIT_FAILURE;
    }

    if (!path){
    	fprintf(stderr, "Invalid path to font file\n");
		return EXIT_FAILURE;
	}

    if(FT_Init_FreeType(&library)) {
    	fprintf(stderr, "Error loading Freetype library\n");
    	return EXIT_FAILURE;
    }
    if (FT_New_Face(library, path,0,&face)) {
		fprintf(stderr, "Error loading font %s\n", path);
		return EXIT_FAILURE;
    }

    if(FT_Set_Char_Size ( face, point_size * 64, point_size * 64, dpi, dpi)) {
		fprintf(stderr, "Error initializing character parameters\n");
		return EXIT_FAILURE;
    }

    glGenTextures(1, &current_font.font_texture);

    //Let each glyph reside in 32x32 section of the font texture
    int segment_size = 32;
    int num_segments_x = 16;
    int num_segments_y = 8;
    int font_tex_width = num_segments_x * segment_size;
    int font_tex_height = num_segments_y * segment_size;

    int glyph_width, glyph_height;
    int bitmap_offset_x = 0, bitmap_offset_y = 0;

    FT_GlyphSlot slot;
    FT_Bitmap bmp;

    GLubyte* font_texture_data = (GLubyte*) malloc(sizeof(GLubyte) * 2 * font_tex_width * font_tex_height);

    if (!font_texture_data) {
		fprintf(stderr, "Failed to allocate memory for font texture\n");
		return EXIT_FAILURE;
	}

    // Fill font texture bitmap with individual bmp data and record appropriate size, texture coordinates and offsets for every glyph
    for(c = 0; c < 128; c++) {
		if(FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			fprintf(stderr, "FT_Load_Char failed\n");
			return EXIT_FAILURE;
		}

		slot = face->glyph;
		bmp = slot->bitmap;

		glyph_width = nextp2(bmp.width);
		glyph_height = nextp2(bmp.rows);

		div_t temp = div(c, num_segments_x);

		bitmap_offset_x = segment_size * temp.rem;
		bitmap_offset_y = segment_size * temp.quot;

        for (j = 0; j < glyph_height; j++) {
        	for (i = 0; i < glyph_width; i++) {
        		font_texture_data[2 * ((bitmap_offset_x + i) + (j + bitmap_offset_y) * font_tex_width) + 0] =
        		font_texture_data[2 * ((bitmap_offset_x + i) + (j + bitmap_offset_y) * font_tex_width) + 1] =
					(i >= bmp.width || j >= bmp.rows)? 0 : bmp.buffer[i + bmp.width * j];
            }
        }

        current_font.advance[c] = (float)(slot->advance.x >> 6);
        current_font.tex_x1[c] = (float)bitmap_offset_x / (float) font_tex_width;
        current_font.tex_x2[c] = (float)(bitmap_offset_x + bmp.width) / (float)font_tex_width;
        current_font.tex_y1[c] = (float)bitmap_offset_y / (float) font_tex_height;
        current_font.tex_y2[c] = (float)(bitmap_offset_y + bmp.rows) / (float)font_tex_height;
        current_font.width[c] = bmp.width;
        current_font.height[c] = bmp.rows;
        current_font.offset_x[c] = (float)slot->bitmap_left;
        current_font.offset_y[c] =  (float)((slot->metrics.horiBearingY-face->glyph->metrics.height) >> 6);
    }

    glBindTexture(GL_TEXTURE_2D, current_font.font_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, font_tex_width, font_tex_height, 0, GL_LUMINANCE_ALPHA , GL_UNSIGNED_BYTE, font_texture_data);

    glGetError();

    free(font_texture_data);

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    current_font.initialized = true;

    return EXIT_SUCCESS;
}

void bbutil_render_text(char* msg, float x, float y, float scale) {
    int i, c;
    GLfloat *vertices;
    GLfloat *texture_coords;
    GLshort* indices;

    float pen_x = 0.0f;

    if (!current_font.initialized) {
    	fprintf(stderr, "Font has not been loaded\n");
    	return;
    }

    if (!msg) {
    	return;
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    vertices = (GLfloat*) malloc(sizeof(GLfloat) * 8 * strlen(msg));
    texture_coords = (GLfloat*) malloc(sizeof(GLfloat) * 8 * strlen(msg));

    indices = (GLshort*) malloc(sizeof(GLfloat) * 5 * strlen(msg));

    for(i = 0; i < strlen(msg); ++i) {
        c = msg[i];

        vertices[8 * i + 0] = x + pen_x + current_font.offset_x[c] * scale;
        vertices[8 * i + 1] = y + current_font.offset_y[c] * scale;
        vertices[8 * i + 2] = vertices[8 * i + 0] + current_font.width[c] * scale;
        vertices[8 * i + 3] = vertices[8 * i + 1];
        vertices[8 * i + 4] = vertices[8 * i + 0];
        vertices[8 * i + 5] = vertices[8 * i + 1] + current_font.height[c] * scale;
        vertices[8 * i + 6] = vertices[8 * i + 2];
        vertices[8 * i + 7] = vertices[8 * i + 5];

        texture_coords[8 * i + 0] = current_font.tex_x1[c];
		texture_coords[8 * i + 1] = current_font.tex_y2[c];
		texture_coords[8 * i + 2] = current_font.tex_x2[c];
		texture_coords[8 * i + 3] = current_font.tex_y2[c];
		texture_coords[8 * i + 4] = current_font.tex_x1[c];
        texture_coords[8 * i + 5] = current_font.tex_y1[c];
        texture_coords[8 * i + 6] = current_font.tex_x2[c];
        texture_coords[8 * i + 7] = current_font.tex_y1[c];

        indices[i * 6 + 0] = 4 * i + 0;
        indices[i * 6 + 1] = 4 * i + 1;
        indices[i * 6 + 2] = 4 * i + 2;
        indices[i * 6 + 3] = 4 * i + 2;
        indices[i * 6 + 4] = 4 * i + 1;
        indices[i * 6 + 5] = 4 * i + 3;

        //Assume we are only working with typewriter fonts
        pen_x += current_font.advance[c] * scale;
    }

    glVertexPointer(2, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texture_coords);
	glBindTexture(GL_TEXTURE_2D, current_font.font_texture);

	glDrawElements(GL_TRIANGLES, 6 * strlen(msg), GL_UNSIGNED_SHORT, indices);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    free(vertices);
    free(texture_coords);
    free(indices);
}

void bbutil_measure_text(char* msg, float scale, float *width, float* height) {
	int i, c;

	if (!msg) {
		return;
	}

	if (width) {
		//Width of a text rectangle is a sum advances for every glyph in a string
		*width = 0.0f;

		for(i = 0; i < strlen(msg); ++i) {
			c = msg[i];
			*width += current_font.advance[c];
		}

		*width = (int)((*width) * scale);

	}

	if (height) {
		//Height of a text rectangle is a high of a tallest glyph in a string
		*height = 0.0f;

		for(i = 0; i < strlen(msg); ++i) {
			c = msg[i];

			if (*height < current_font.height[c]) {
				*height = current_font.height[c];
			}
		}

		*height = (int)((*height) * scale);
	}
}

int bbutil_load_texture(const char* filename, int *width, int *height, float* tex_x, float* tex_y, GLuint *tex) {
	int i, j;
	//header for testing if it is a png
	png_byte header[8];

	if (!tex) {
		return EXIT_FAILURE;
	}

	//open file as binary
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return EXIT_FAILURE;
	}

	//read the header
	fread(header, 1, 8, fp);

	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png) {
		fclose(fp);
		return EXIT_FAILURE;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fp);
		return EXIT_FAILURE;
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		fclose(fp);
		return EXIT_FAILURE;
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		fclose(fp);
		return EXIT_FAILURE;
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		return EXIT_FAILURE;
	}

	//init png reading
	png_init_io(png_ptr, fp);

	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);

	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);

	//variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 image_width, image_height;

	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &image_width, &image_height, &bit_depth, &color_type, NULL, NULL, NULL);

	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);

	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// Allocate the image_data as a big block, to be given to opengl
	png_byte *image_data = (png_byte*) malloc(sizeof(png_byte) * rowbytes * image_height);

	if (!image_data) {
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		return EXIT_FAILURE;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * image_height);
	if (!row_pointers) {
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		free(image_data);
		fclose(fp);
		return EXIT_FAILURE;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	for (i = 0; i < image_height; i++) {
		row_pointers[image_height - 1 - i] = image_data + i * rowbytes;
	}

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	int tex_width, tex_height;

	tex_width = nextp2(image_width);
	tex_height = nextp2(image_height);

	//DEBUG ONLY
	 GLubyte* texture_data = (GLubyte*) malloc(sizeof(GLubyte) * 4 * tex_width * tex_height);

	 for (j = 0; j < tex_height; j++) {
    	for (i = 0; i < tex_width; i++) {
    		texture_data[4 * (i + j * tex_width) + 0] = (i >= image_width || j >= image_height)? 0 : image_data[3 * (i + j * image_width) + 0];
    		texture_data[4 * (i + j * tex_width) + 1] = (i >= image_width || j >= image_height)? 0 : image_data[3 * (i + j * image_width) + 1];
    	    texture_data[4 * (i + j * tex_width) + 2] = (i >= image_width || j >= image_height)? 0 : image_data[3 * (i + j * image_width) + 2];
    	    texture_data[4 * (i + j * tex_width) + 3] = 1.0f;
        }
    }

	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, (*tex));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)texture_data);

	GLint err = glGetError();

	//clean up memory and close stuff
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	free(texture_data);
	free(image_data);
	free(row_pointers);
	fclose(fp);

	if (err == 0) {
		//Return physical with and height of texture if pointers are not null
		if(width) {
			*width = tex_width;
		}
		if (height) {
			*height = tex_height;
		}
		//Return modified texture coordinates if pointers are not null
		if(tex_x) {
			*tex_x = (float) image_width / (float)tex_width;
		}
		if(tex_y) {
			*tex_y = (float) image_height / (float)tex_height;
		}
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "GL error %i \n", err);
		return EXIT_FAILURE;
	}
}

int bbutil_calculate_dpi(screen_context_t ctx) {
	int rc;
	screen_display_t* disp;
	int num_displays;

	rc = screen_get_context_property_iv(ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &num_displays);
    if (rc) {
        perror("screen_get_context_property_iv");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

	disp = (screen_display_t*)malloc(sizeof(screen_display_t) * num_displays);

	if (!disp) {
		fprintf(stderr, "Failed to allocate memory for display handles\n");
		return EXIT_FAILURE;
	}

	rc = screen_get_context_property_pv(ctx, SCREEN_PROPERTY_DISPLAYS, (void*)disp);
    if (rc) {
        perror("screen_get_context_property_pv");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

	int screen_phys_size[2];

	rc = screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_PHYSICAL_SIZE, screen_phys_size);
    if (rc) {
        perror("screen_get_display_property_iv");
        bbutil_terminate();
        return EXIT_FAILURE;
    }

    //Simulator will return 0,0 for physical size of the screen, so use 170 as default dpi
    if ((screen_phys_size[0] == 0) && (screen_phys_size[1] == 0)) {
    	return 170;
    } else {
		int screen_resolution[2];
		rc = screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_SIZE, screen_resolution);
		if (rc) {
			perror("screen_get_display_property_iv");
			bbutil_terminate();
			return EXIT_FAILURE;
		}

		int diagonal_pixels = sqrt(screen_resolution[0] * screen_resolution[0] + screen_resolution[1] * screen_resolution[1]);
		int diagontal_inches = 0.0393700787 * sqrt(screen_phys_size[0] * screen_phys_size[0] + screen_phys_size[1] * screen_phys_size[1]);
        return (int)(diagonal_pixels / diagontal_inches);
    }
}
