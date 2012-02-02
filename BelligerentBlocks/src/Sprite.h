/*
 * Copyright 2011-2012 Research In Motion Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file Sprite.h
 *
 */

#ifndef SPRITE_H_
#define SPRITE_H_

#include "bbutil.h"
#include <GLES/gl.h>

class Sprite {
public:
    Sprite ();
    ~Sprite ();
    bool load(const char* filename);
    void setPosition(float x, float y);
    void draw() const;

    GLfloat Width() const { return m_width; };
    GLfloat Height() const { return m_height; };
    GLfloat PosX() const { return m_posX; };
    GLfloat PosY() const { return m_posY; };
private:
    GLfloat m_vertices[8];
    GLfloat m_textureCoordinates[8];
    GLuint m_textureHandle;
    GLfloat m_width;
    GLfloat m_height;
    GLfloat m_posX;
    GLfloat m_posY;
};

#endif /* SPRITE_H_ */
