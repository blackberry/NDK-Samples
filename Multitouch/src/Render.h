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

#ifndef RENDER_H_
#define RENDER_H_


#include <EGL/egl.h>
#include <GLES/gl.h>
#include <stdbool.h>
#include <time.h>
#include <bps/screen.h>

extern GLfloat vertices[8];
extern GLfloat verticesH[8];
extern GLfloat verticesTouchpoint[8];

extern GLfloat tex_coord[8];
extern GLfloat tex_coordH[8];
extern GLfloat tex_coord_touchpoint[8];

extern GLuint background;
extern GLuint backgroundH;
extern GLuint touchpoint;

extern float width, height;

extern float colors[4][3];

void enable_2d();
int initOpenGL(screen_context_t);
void render();

#endif /* RENDER_H_ */
