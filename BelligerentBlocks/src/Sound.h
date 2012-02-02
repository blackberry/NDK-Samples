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
 * @file Sound.h
 *
 */

#ifndef SOUND_H_
#define SOUND_H_

#include <AL/alut.h>

#include <vector>

namespace blocks {

/**
 * Play a sound
 */
class Sound {
public:
    Sound();

    /**
     * If a sound was loaded, destroys the source and frees all the buffers.
     */
    ~Sound();

    /**
     * Load a WAV file.
     *
     * Loads a WAV file and places it into one or more buffers.  These buffers
     * are then attached to a source for playing and/or streaming.
     *
     * @param fileName The WAV file to load.
     * @return True if the file was successfully loaded.  Otherwise returns
     *         false.
     */
    bool load(const char* fileName);

    /**
     * Start playing the sound
     *
     * If this is called it will play a sound until it completes.  To loop the
     * sound the caller has to make frequent calls to tick().
     */
    void play() const;

    /**
     * Pause playing the sound
     *
     * Stops playing the sound, but maintains position in the sound buffer.
     * When play() is called, sound resumes from the point pause() was called.
     */
    void pause() const;

    /**
     * Stream background buffers, if loaded.
     *
     * This method should be called frequently.  It will query the music source
     * to see how many buffers have finished playing.  It will then dequeue
     * each of these finished buffers and then queue them up again.
     *
     * If no sound is loaded this is a no-op.  If pause() was called, this
     * will NOT resume (call play() to accomplish that).
     */
    void tick();

    /**
     * Unload the WAV file.
     *
     * After unloading, subsequent calls to play() and tick() will be no-ops.
     * Any buffers that were allocated will be freed, and the sound source will
     * be destroyed.
     */
    void unload();

    /**
     * Returns whether the sound was properly loaded and ready to play.
     */
    bool isLoaded() const { return (m_source != 0); }

private:
    std::vector<ALuint> m_buffers;
    ALuint m_source;
};

} /* namespace blocks */

#endif /* SOUND_H_ */
