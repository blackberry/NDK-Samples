/**
 *
 * The Main Renderer
 * This class includes all the rendering stuff
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

#include "Render.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bbutil.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "Touchpoint.h"
#include "EventHandler.h"

GLfloat vertices[8];
GLfloat verticesH[8];
GLfloat verticesTouchpoint[8];
GLfloat tex_coord[8];
GLfloat tex_coordH[8];
GLfloat tex_coord_touchpoint[8];
GLuint background;
GLuint backgroundH;
GLuint touchpoint;

int dpi;
int counter = 0;
float width, height;

float colors[4][3] = {{1,0.35,0.35},{0,0.64,0},{0,0.64,0.64},{0.64,0,0}};

font_t* font;

std::list<Touchpoint>::iterator pp;

int initOpenGL(screen_context_t screen_cxt) {
	fprintf(stderr, "init openGL\n");

	//Initialize vertex and color data
	vertices[0] = 0.0f;
	vertices[1] = 0.0f;

	vertices[2] = 1024.0f;
	vertices[3] = 0.0f;

	vertices[4] = 0.0f;
	vertices[5] = 600.0f;

	vertices[6] = 1024.0f;
	vertices[7] = 600.0f;

	verticesH[0] = 0.0f;
	verticesH[1] = 0.0f;

	verticesH[2] = 600.0f;
	verticesH[3] = 0.0f;

	verticesH[4] = 0.0f;
	verticesH[5] = 1024.0f;

	verticesH[6] = 600.0f;
	verticesH[7] = 1024.0f;

	verticesTouchpoint[0] = 0.0f;
	verticesTouchpoint[1] = 0.0f;

	verticesTouchpoint[2] = 120.0f;
	verticesTouchpoint[3] = 0.0f;

	verticesTouchpoint[4] = 0.0f;
	verticesTouchpoint[5] = 120.0f;

	verticesTouchpoint[6] = 120.0f;
	verticesTouchpoint[7] = 120.0f;

	//Query width and height of the window surface created by utility code
	EGLint surface_width, surface_height;

	eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
	eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    width = (float) surface_width;
    height = (float) surface_height;

    if (width < height)
    {
    	oriention_side_up = 1;
    }

	//On initialize bbutil loads arial as a default font. We are going to load MyriadPro-Bold as it looks a little better and scale it
	//to fit out bubble nicely.
	dpi = bbutil_calculate_dpi(screen_cxt);

	font = bbutil_load_font(
			"/usr/fonts/font_repository/adobe/MyriadPro-Bold.otf", 9, dpi);
	if (!font) {
		return EXIT_FAILURE;
	}

	EGLint err = eglGetError();
	if (err != 0x3000) {
		fprintf(stderr, "Unable to query egl surface dimensions\n");
		return EXIT_FAILURE;
	}

	//Load background texture
	float tex_x, tex_y;
	if (EXIT_SUCCESS
			!= bbutil_load_texture("app/native/bg.png", NULL, NULL, &tex_x,
					&tex_y, &background)) {
		fprintf(stderr, "Unable to load background texture\n");
	}
	//Load background portrait texture
	float texH_x, texH_y;
	if (EXIT_SUCCESS
			!= bbutil_load_texture("app/native/bg_hochformat.png", NULL, NULL, &texH_x,
					&texH_y, &backgroundH)) {
		fprintf(stderr, "Unable to load background portrait texture\n");
	}
	//Load background texture
	float tex_x_touch, tex_y_touch;
	if (EXIT_SUCCESS
			!= bbutil_load_texture("app/native/zeiger.png", NULL, NULL,
					&tex_x_touch, &tex_y_touch, &touchpoint)) {
		fprintf(stderr, "Unable to load zeiger texture\n");
	}

	tex_coord[0] = 0.0f;
	tex_coord[1] = 0.0f;

	tex_coord[2] = tex_x;
	tex_coord[3] = 0.0f;

	tex_coord[4] = 0.0f;
	tex_coord[5] = tex_y;

	tex_coord[6] = tex_x;
	tex_coord[7] = tex_y;

	tex_coordH[0] = 0.0f;
	tex_coordH[1] = 0.0f;

	tex_coordH[2] = texH_x;
	tex_coordH[3] = 0.0f;

	tex_coordH[4] = 0.0f;
	tex_coordH[5] = texH_y;

	tex_coordH[6] = texH_x;
	tex_coordH[7] = texH_y;

	tex_coord_touchpoint[0] = 0.0f;
	tex_coord_touchpoint[1] = 0.0f;

	tex_coord_touchpoint[2] = tex_x_touch;
	tex_coord_touchpoint[3] = 0.0f;

	tex_coord_touchpoint[4] = 0.0f;
	tex_coord_touchpoint[5] = tex_y_touch;

	tex_coord_touchpoint[6] = tex_x_touch;
	tex_coord_touchpoint[7] = tex_y_touch;

	glShadeModel(GL_SMOOTH);

	//set clear color to white
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	glViewport(0, 0, surface_width, surface_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrthof(0.0f, (float) (surface_width) / (float) (surface_height), 0.0f,
			1.0f, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glScalef(1.0f / surface_height, 1.0f / surface_height, 1.0f);

	//init texture
	glEnable(GL_TEXTURE_2D);

	return EXIT_SUCCESS;
}


void enable_2d()
{
    glViewport(0, 0, (int) width, (int) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, width / height, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1.0f / height, 1.0f / height, 1.0f);
}

/**
 * main render function
 */
void render() {
	counter++;
	if(counter >= 36000){
		counter = 0;
	}
	//Typical render pass
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// update dimensions
	enable_2d();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glPushMatrix();

		// if in portrait mode
		if (oriention_side_up > 0)
		{
			glVertexPointer(2, GL_FLOAT, 0, verticesH);
			glTexCoordPointer(2, GL_FLOAT, 0, tex_coordH);
		}
		// if in landscape mode
		else
		{
			glVertexPointer(2, GL_FLOAT, 0, vertices);
			glTexCoordPointer(2, GL_FLOAT, 0, tex_coord);
		}

		//portrait or landscape?
		glBindTexture(GL_TEXTURE_2D, oriention_side_up>0?backgroundH:background);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glPopMatrix();

	pp = points.begin();

	while (pp != points.end()) {

		if (pp->visible) {
			// draw touchpoint
			glPushMatrix();
				glTranslatef(pp->getX(), (oriention_side_up>0?1024:600) - pp->getY(), 0);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glVertexPointer(2, GL_FLOAT, 0, verticesTouchpoint);
				glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_touchpoint);
				glBindTexture(GL_TEXTURE_2D, touchpoint);
				glPushMatrix();
					glRotatef((float) ((float)(pp->startRotation) + (float) counter / 0.25f), 0, 0, 1);
					glPushMatrix();
						glTranslatef(-60, -60, 0);
						glColor4f(pp->r,pp->g,pp->b,1);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glPopMatrix();
				glPopMatrix();
			glPopMatrix();

			// draw touchpoint number
			glPushMatrix();
				glColor4f(0.3f,0.3f,0.3f,1.0f);
				char buffer [33];
				itoa (pp->id+1,buffer,10);
				glTranslatef(pp->getX()+50,(oriention_side_up>0?1024:600)-pp->getY()+50,0);
				glPushMatrix();
					bbutil_render_text(font,buffer,0,0);
				glPopMatrix();
			glPopMatrix();

		}
		pp++;
	}

	glColor4f(1.0f,1.0f,1.0f,1.0f);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	//Use utility code to update the screen
	bbutil_swap();
}
