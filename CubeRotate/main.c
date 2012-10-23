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

#include <glview/glview.h>

#include <GLES/gl.h>
#include <stdio.h>
#include <math.h>

static const GLfloat vertices[] =
{
      // front
      -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
       0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,

      // right
      0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
      0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f,

      // back
      0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
      -0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

      // left
      -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, 0.5f,
      -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,

      // top
      -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
       0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,

      // bottom
      -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f,
       0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f
};

static const GLfloat colors[] =
{
       /* front  */  0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f,
       /* right  */  0.29296875f,0.66796875f,0.92578125f,1.0f,0.29296875f,0.66796875f,0.92578125f,1.0f, 0.29296875f,0.66796875f,0.92578125f,1.0f, 0.29296875f,0.66796875f,0.92578125f,1.0f, 0.29296875f,0.66796875f,0.92578125f,1.0f, 0.29296875f,0.66796875f,0.92578125f,1.0f,
       /* back   */  0.52734375f,0.76171875f,0.92578125f,1.0f, 0.52734375f,0.76171875f,0.92578125f,1.0f,0.52734375f,0.76171875f,0.92578125f,1.0f, 0.52734375f,0.76171875f,0.92578125f,1.0f, 0.52734375f,0.76171875f,0.92578125f,1.0f, 0.52734375f,0.76171875f,0.92578125f,1.0f,
       /* left   */  0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f,0.0625f,0.57421875f,0.92578125f,1.0f, 0.0625f,0.57421875f,0.92578125f,1.0f,
       /* top    */  0.29296875f,0.66796875f,0.92578125f,1.0f, 0.29296875f,0.66796875f,0.92578125f,1.0f,0.29296875f,0.66796875f,0.92578125f,1.0f,0.29296875f,0.66796875f,0.92578125f,1.0f,0.29296875f,0.66796875f,0.92578125f,1.0f,0.29296875f,0.66796875f,0.92578125f,1.0f,
       /* bottom */  0.52734375f,0.76171875f,0.92578125f,1.0f,0.52734375f,0.76171875f,0.92578125f,1.0f,0.52734375f,0.76171875f,0.92578125f,1.0f,0.52734375f,0.76171875f,0.92578125f,1.0f,0.52734375f,0.76171875f,0.92578125f,1.0f,0.52734375f,0.76171875f,0.92578125f,1.0f
};

static void
init(void *p)
{
    unsigned int surface_height;
    unsigned int surface_width;
    glview_get_size(&surface_width, &surface_height);

    glClearDepthf(1.0f);
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);

    glViewport(0, 0, surface_width, surface_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float nearClip = -2.0f;
    float farClip  = 2.0f;
    float yFOV  = 75.0f;
    float yMax = nearClip * tan(yFOV*M_PI/360.0f);
    float aspect = surface_width/surface_height;
    float xMin = -yMax * aspect;
    float xMax = yMax *aspect;

    glFrustumf(xMin, xMax, -yMax, yMax, nearClip, farClip);

    // bar-descriptor is forcing landscape so we know width > height.
    glScalef((float)surface_height/(float)surface_width, 1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void
display(void *p)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, colors);

    glRotatef(1.0f, 1.0f, 1.0f, 0.0f);

    glDrawArrays(GL_TRIANGLES, 0 , 36);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

int
main(int argc, char **argv)
{
    glview_initialize(GLVIEW_API_OPENGLES_11, &display);
    glview_register_initialize_callback(&init);
    glview_loop();
    return 0;
}
