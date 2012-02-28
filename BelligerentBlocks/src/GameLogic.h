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
 * @file GameLogic.h
 *
 */

#ifndef GAMELOGIC_H_
#define GAMELOGIC_H_

#include "Platform.h"
#include "Sound.h"
#include "bbutil.h"
#include "Sprite.h"

#include <math.h>
#include "Box2D/Box2D.h"

#include <GLES/gl.h>

#ifndef NDEBUG
#include <assert.h>
#define ASSERT(a) (assert(a))
#else
#define ASSERT(a)
#endif

namespace blocks {

class ContactListener : public b2ContactListener {
public:
    ContactListener(const Sound& sound) : m_sound(sound){};
    virtual ~ContactListener(){};

    virtual void BeginContact(b2Contact* contact) {
        m_sound.play();
    }
    virtual void EndContact(b2Contact* contact) {};
    virtual void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) {};
    virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) {};
private:
    const Sound& m_sound;
};

class GameLogic : public PlatformEventHandler {
public:
    GameLogic(Platform& platform);
    virtual ~GameLogic(){};
    void run();

private:
    Platform& m_platform;
    bool m_shutdown;
    bool m_gamePaused;
    bool m_gameFinished;

    unsigned int m_numBlocks;

    enum GameState { FetchUser, GamePlay, LeaderBoard };
    GameState m_state;

    font_t* m_font;
    font_t* m_scoreFont;
    font_t* m_leaderboardFont;

    float m_sceneWidth, m_sceneHeight;

    float m_velocityTreshold;
    long m_angstCountdown;

    Sprite m_background;
    Sprite m_leaderBoard;
    Sprite m_smallBlockBelligerent;
    Sprite m_smallBlockResting;
    Sprite m_largeBlockBelligerent;
    Sprite m_largeBlockResting;
    Sprite m_buttonPressed;
    Sprite m_buttonRegular;
    Sprite m_ground;

    struct block {
        bool angst;
        time_t countdown;
        b2Body* body;
        const Sprite* belligerent;
        const Sprite* resting;

        block(bool angst, time_t countdown, b2Body* body, const Sprite* belligerent, const Sprite* resting)
            : angst(angst)
            , countdown(countdown)
            , body(body)
            , belligerent(belligerent)
            , resting(resting) {

            ASSERT(belligerent);
            ASSERT(resting);
            ASSERT(belligerent->Width() == resting->Width());
            ASSERT(belligerent->Height() == resting->Height());
        }

        float Width() const { ASSERT(belligerent); return belligerent->Width(); }
        float Height() const { ASSERT(belligerent); return belligerent->Height(); }
    };

    std::vector<block> m_blocks;

    float m_blocksToAddYOffset;

    struct button {
        float posX;
        float posY;
        float sizeX;
        float sizeY;
        bool isPressed;
        Sprite* regular;
        Sprite* pressed;
        float textX;
        float textY;
        font_t* font;
        const char* text;

        void draw() const;

        void setPosition(float x, float y);

        bool isWithin(float clickX, float clickY) const {
            return ((clickX >= posX - sizeX / 2) && (clickX <= posX + sizeX / 2) && (clickY >= posY - sizeX / 2) && (clickY <= posY + sizeY / 2));
        }
    };

    button m_playButton;

    bool m_showClock;
    int m_time;
    int m_scoreTime;
    time_t m_resumeTime;
    int m_score;
    float m_scorePosX, m_scorePosY, m_timerPosX, m_timerPosY;
    const char* m_message;
    float m_messagePosX, m_messagePosY;

    bool m_leaderBoardReady;
    std::vector<Score> m_leaderboard;

    //Box2D things
    float m_b2ScaleFactor;
    float m_timeStep;
    int m_velocityIterations, m_positionIterations;
    b2World m_world;
    b2Body* m_groundBody;

    virtual void onLeftPress(float x, float y);
    virtual void onLeftRelease(float x, float y);
    virtual void onExit();
    virtual void onPause();
    virtual void onResume();
    virtual void onSubmitScore();
    virtual void onLeaderboardReady(const std::vector<Score>& leaderboard);
    virtual void onUserReady(const std::string& userName, bool isAnonymous, const std::string& errorString);
    virtual void onPromptOk(const std::string& input);

    void endGamePlay(bool win);
    void reset();
    void update();
    void renderFetchUser();
    void renderGame();
    void renderLeadBoard();
    void enable2D();
    void addNextShape(float x, float y);

    Sound m_backgroundMusic;
    Sound m_click1;
    Sound m_click2;
    Sound m_clickReverb;
    Sound m_blockFall;
    ContactListener m_contactListener;
};

} /* namespace blocks */
#endif /* GAMELOGIC_H_ */
