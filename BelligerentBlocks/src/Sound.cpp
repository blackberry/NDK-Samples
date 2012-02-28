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

#include "Sound.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef NDEBUG
#include <assert.h>
#define ASSERT(a) (assert(a))
#else
#define ASSERT(a)
#endif

namespace blocks {

static const int BufferSize = 1 * 1024 * 1024;

Sound::Sound()
    : m_source(0) {
}

Sound::~Sound() {
    if (isLoaded()) {
        unload();
    }
}

bool Sound::load(const char* fileName) {
    ASSERT(!isLoaded());

    // Clear any old ALUT error
    alutGetError();

    ALenum format;
    ALsizei size;
    ALfloat frequency;
    unsigned char* rawData = static_cast<unsigned char*>(alutLoadMemoryFromFile(fileName, &format, &size, &frequency));

    ALenum error = alutGetError();
    if (error != ALUT_ERROR_NO_ERROR) {
        fprintf(stderr, "Cannot load background file into memory: %d, %s\n", error, alutGetErrorString(error));
        free(rawData);
        return false;
    }

    m_buffers.resize(size / BufferSize + 1);

    // Clear old AL error
    alGetError();
    alGenBuffers(m_buffers.size(), &m_buffers[0]);
    error = alGetError();
    if (error != AL_NO_ERROR) {
        fprintf(stderr, "Cannot generate background buffers: %d\n", error);
        free(rawData);
        return false;
    }

    alGenSources(1, &m_source);
    error = alGetError();
    if (error != AL_NO_ERROR) {
        fprintf(stderr, "Cannot generate background source: %d\n", error);
        free(rawData);
        alDeleteBuffers(m_buffers.size(), &m_buffers[0]);
        m_buffers.clear();
        return false;
    }

    for (unsigned int i = 0; i < m_buffers.size(); i++) {
        int writeSize = size - (i * BufferSize);
        if (writeSize > BufferSize) {
            writeSize = BufferSize;
        }

        alBufferData(m_buffers[i], format, &(rawData[i * BufferSize]), writeSize, frequency);
        error = alGetError();
        if (error != AL_NO_ERROR) {
            fprintf(stderr, "Cannot copy raw background music into buffer: %d\n", error);
            free(rawData);
            alDeleteBuffers(m_buffers.size(), &m_buffers[0]);
            m_buffers.clear();
            alDeleteSources(1, &m_source);
            m_source = 0;
            return false;
        }

        alSourceQueueBuffers(m_source, 1, &m_buffers[i]);
        error = alGetError();
        if (error != AL_NO_ERROR) {
            fprintf(stderr, "Cannot queue background buffer: %d\n", error);
            free(rawData);
            alDeleteBuffers(m_buffers.size(), &m_buffers[0]);
            m_buffers.clear();
            alDeleteSources(1, &m_source);
            m_source = 0;
            return false;
        }
    }

    free(rawData);
    return true;
}

void Sound::play() const {
    if (isLoaded()) {
        alSourcePlay(m_source);
    }
}

void Sound::pause() const {
    if (isLoaded()) {
        alSourcePause(m_source);
    }
}

void Sound::tick() {
    ALenum error;

    // Clear any previous error
    alGetError();

    if (isLoaded()) {
        ALint processedBuffers;
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processedBuffers);
        if ((error = alGetError()) != AL_NO_ERROR) {
            fprintf(stderr, "Cannot query processed buffers: %d\n", error);
            return;
        }

        while (processedBuffers > 0) {
            ALuint buffer;
            alSourceUnqueueBuffers(m_source, 1, &buffer);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR) {
                fprintf(stderr, "Cannot dequeue a processed buffer: %d\n", error);
                return;
            }

            alSourceQueueBuffers(m_source, 1, &buffer);
            error = alGetError();
            if (error != AL_NO_ERROR) {
                fprintf(stderr, "Cannot re-queue a processed buffer: %d\n", error);
                return;
            }

            processedBuffers--;
        }

        // Did we run out of audio?  If so, restart playing.
        ALint state;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        error = alGetError();
        if (error != AL_NO_ERROR) {
            fprintf(stderr, "Cannot query background source state: %d\n", error);
            return;
        }

        if (state != AL_PLAYING && state != AL_PAUSED) {
            fprintf(stderr, "Background music has stalled\n");

            ALint queuedBuffers = 0;
            alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queuedBuffers);
            error = alGetError();
            if (error != AL_NO_ERROR) {
                fprintf(stderr, "Cannot query background source queued buffers: %d\n", error);
                return;
            }

            if (queuedBuffers > 0) {
                alSourcePlay(m_source);
                error = alGetError();
                if (error != AL_NO_ERROR) {
                    fprintf(stderr, "Cannot restart background music: %d\n", error);
                    return;
                }
            }
        }
    }
}

void Sound::unload() {
    // Stop the background music and clear the queue.
    ASSERT(isLoaded());
    if (isLoaded()) {
        alSourceStop(m_source);
        alSourcei(m_source, AL_BUFFER, 0);
        alDeleteSources(1, &m_source);
        m_source = 0;
        alDeleteBuffers(m_buffers.size(), &m_buffers[0]);
        m_buffers.clear();
    }
}

} /* namespace blocks */
