/*
 * Copyright 2011 Research In Motion Limited
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
 * @file Sprite.cpp
 *
 */

#include "Sprite.h"
#include <stdio.h>
#include <stdlib.h>

Sprite::Sprite() {
    for (int i = 0; i < 8; i++) {
        m_vertices[i] = 0.0f;
        m_textureCoordinates[i] = 0.0f;
    }
    m_textureHandle = 0;
    m_width = 0.0f;
    m_height = 0.0f;
    m_posX = 0.0f;
    m_posY = 0.0f;
}

Sprite::~Sprite() {
    glDeleteTextures(1, &m_textureHandle);
}

bool Sprite::load(const char* filename){
    float texX = 1.0f, texY = 1.0f;
    int sizeX, sizeY;

    if (EXIT_SUCCESS != bbutil_load_texture(filename, &sizeX, &sizeY, &texX, &texY, (unsigned int*)&m_textureHandle)) {
        fprintf(stderr, "Unable to load sprite texture\n");
        return false;
    }

    m_width = static_cast<float>(sizeX);
    m_height = static_cast<float>(sizeY);

    m_vertices[0] = -m_width / 2;
    m_vertices[1] = -m_height / 2;
    m_vertices[2] =  m_width / 2;
    m_vertices[3] = -m_height / 2;
    m_vertices[4] = -m_width / 2;
    m_vertices[5] =  m_height / 2;
    m_vertices[6] =  m_width / 2;
    m_vertices[7] =  m_height / 2;

    m_textureCoordinates[0] = 0.0f;
    m_textureCoordinates[1] = 0.0f;
    m_textureCoordinates[2] = texX;
    m_textureCoordinates[3] = 0.0f;
    m_textureCoordinates[4] = 0.0f;
    m_textureCoordinates[5] = texY;
    m_textureCoordinates[6] = texX;
    m_textureCoordinates[7] = texY;

    m_posX = -m_width / 2;
    m_posY = -m_height / 2;

    return true;
}

void Sprite::setPosition(float x, float y){
    if (!glIsTexture(m_textureHandle) || ((m_width == 0) && (m_height == 0))) {
        fprintf(stderr, "Sprite has not been loaded\n");
        return;
    }

    m_posX = x;
    m_posY = y;

    m_vertices[0] = x + m_vertices[0];
    m_vertices[1] = y + m_vertices[1];
    m_vertices[2] = x + m_vertices[2];
    m_vertices[3] = y + m_vertices[3];
    m_vertices[4] = x + m_vertices[4];
    m_vertices[5] = y + m_vertices[5];
    m_vertices[6] = x + m_vertices[6];
    m_vertices[7] = y + m_vertices[7];
}

void Sprite::draw() const {
    if (!glIsTexture(m_textureHandle) || ((m_width == 0) && (m_height == 0))) {
        fprintf(stderr, "Sprite has not been loaded\n");
        return;
    }

    glVertexPointer(2, GL_FLOAT, 0, m_vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, m_textureCoordinates);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);
    glDrawArrays(GL_TRIANGLE_STRIP, 0 , 4);
}
