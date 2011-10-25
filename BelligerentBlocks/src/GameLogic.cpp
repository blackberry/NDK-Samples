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
 * @file GameLogic.cpp
 *
 */

#include "GameLogic.h"
#include "Platform.h"
#include "Sound.h"

#include <AL/alut.h>

#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <unistd.h>

#define SCORE_OFFSET_X 50.0f
#define SCORE_OFFSET_Y 50.0f
#define TIMER_OFFSET_X 50.0f
#define TIMER_OFFSET_Y 10.0f
#define MESSAGE_OFFSET_Y 35.0f
#define LEADERBOARD_LINE_OFFSET_X 30.0f
#define LEADERBOARD_LINE_OFFSET_Y 30.0f

namespace blocks {

GameLogic::GameLogic(Platform &platform)
    : PlatformEventHandler()
    , m_platform(platform)
    , m_shutdown(false)
    , m_gamePaused(true)
    , m_gameFinished(false)
    , m_numBlocks(0)
    , m_state(FetchUser)
    , m_velocityTreshold(0.001f)
    , m_angstCountdown(2l)
    , m_showClock(false)
    , m_scoreTime(0)
    , m_resumeTime(0)
    , m_score(0)
    , m_leaderBoardReady(false)
    , m_b2ScaleFactor(0.01f)
    , m_timeStep(1.0f / 60.0f)
    , m_velocityIterations(6)
    , m_positionIterations(2)
    , m_world(b2Vec2(0.0f, -10.0f))
    , m_contactListener(m_clickReverb)
{

    m_backgroundMusic.load("app/native/background.wav");
    m_click1.load("app/native/click1.wav");
    m_click2.load("app/native/click2.wav");
    m_clickReverb.load("app/native/clickreverb.wav");
    m_blockFall.load("app/native/blockfall.wav");

    m_platform.setEventHandler(this);
    m_platform.getSize(m_sceneWidth, m_sceneHeight);

    int dpi = m_platform.getDPI();

    m_font = bbutil_load_font("/usr/fonts/font_repository/adobe/MyriadPro-Bold.otf", 15, dpi);
    if (!m_font) {
        fprintf(stderr, "Unable to load font\n");
    }

    m_scoreFont = bbutil_load_font("/usr/fonts/font_repository/adobe/MyriadPro-Bold.otf", 30, dpi);
    if (!m_scoreFont) {
        fprintf(stderr, "Unable to load font\n");
    }

    m_leaderboardFont = bbutil_load_font("/usr/fonts/font_repository/adobe/MyriadPro-Bold.otf", 10, dpi);
    if (!m_scoreFont) {
        fprintf(stderr, "Unable to load font\n");
    }

    //Initialize game sprites
    m_smallBlockBelligerent.load("app/native/belligerent_small.png");
    m_smallBlockResting.load("app/native/resting_small.png");
    m_largeBlockBelligerent.load("app/native/belligerent.png");
    m_largeBlockResting.load("app/native/resting.png");

    m_background.load("app/native/Background.png");
    m_background.setPosition(m_sceneWidth / 2, m_sceneHeight / 2);

    m_leaderBoard.load("app/native/leaderboard.png");
    m_leaderBoard.setPosition(m_sceneWidth / 2, m_sceneHeight / 2);

    m_buttonRegular.load("app/native/button_regular.png");
    m_buttonPressed.load("app/native/button_pressed.png");

    m_ground.load("app/native/ground.png");
    m_ground.setPosition(m_sceneWidth / 2, m_ground.Height() / 2);

    //Initialize message
    float textSizeX, textSizeY;
    m_message = "Loading...";
    bbutil_measure_text(m_font, m_message, &textSizeX, &textSizeY);
    m_messagePosX = (m_sceneWidth - textSizeX) / 2;
    m_messagePosY = (m_sceneHeight - textSizeY) / 2;

    //Initialize score and timer positions
    bbutil_measure_text(m_scoreFont, "123", &textSizeX, &textSizeY);
    m_scorePosX = SCORE_OFFSET_X;
    m_scorePosY = m_sceneHeight - textSizeY - SCORE_OFFSET_Y;

    bbutil_measure_text(m_font, "123", &textSizeX, &textSizeY);
    m_timerPosX = TIMER_OFFSET_X;
    m_timerPosY = m_scorePosY - textSizeY - TIMER_OFFSET_Y;

    //Initialize start button
    m_playButton.sizeX = m_buttonRegular.Width();
    m_playButton.sizeY = m_buttonRegular.Height();
    m_playButton.isPressed = false;
    m_playButton.regular = &m_buttonRegular;
    m_playButton.pressed = &m_buttonPressed;
    m_playButton.font = m_font;
    m_playButton.text = "Play Again";
    bbutil_measure_text(m_font, m_playButton.text, &textSizeX, &textSizeY);
    m_playButton.textX = - textSizeX / 2;
    m_playButton.textY = - textSizeY / 2;
    m_playButton.setPosition(m_sceneWidth / 2, m_leaderBoard.PosY() - m_leaderBoard.Height() / 2);

    //Box2D initialization and scene setup
    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(m_sceneWidth / 2 * m_b2ScaleFactor, m_ground.Height() / 2 * m_b2ScaleFactor);
    m_groundBody = m_world.CreateBody(&groundBodyDef);
    b2PolygonShape groundBox;
    groundBox.SetAsBox(m_ground.Width() / 2 * m_b2ScaleFactor, m_ground.Height() / 2 * m_b2ScaleFactor - 0.02f);

    m_groundBody->CreateFixture(&groundBox, 0.0f);

    m_world.SetContactListener(&m_contactListener);

    //Initialize shape list
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_largeBlockBelligerent, &m_largeBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_largeBlockBelligerent, &m_largeBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_smallBlockBelligerent, &m_smallBlockResting));
    m_blocks.push_back(block(false, 0, NULL, &m_largeBlockBelligerent, &m_largeBlockResting));

    m_blocksToAddYOffset = 0.55 * m_largeBlockBelligerent.Height() / 2 + 10.0f;
}

void GameLogic::enable2D() {
    //Initialize GL for 2D rendering
    glViewport(0, 0, static_cast<int>(m_sceneWidth), static_cast<int>(m_sceneHeight));

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, m_sceneWidth / m_sceneHeight, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //Set world coordinates to coincide with screen pixels
    glScalef(1.0f / m_sceneHeight, 1.0f / m_sceneHeight, 1.0f);
    glClearColor(0.775f, 0.775f, 0.775f, 1.0f);

    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GameLogic::run() {
    enable2D();

    m_backgroundMusic.play();

    m_platform.fetchUser();

    while (!m_shutdown) {
        // platform handle input
        m_platform.processEvents();

        m_backgroundMusic.tick();

        switch (m_state) {
        case FetchUser:
            renderFetchUser();
            break;
        case GamePlay:
            update();
            renderGame();
            break;
        case LeaderBoard:
            renderLeadBoard();
            break;
        }
    }
}

void GameLogic::update() {
    if (m_gamePaused || m_gameFinished) {
        return;
    }

    m_world.Step(m_timeStep, m_velocityIterations, m_positionIterations);

    time_t now = m_platform.getCurrentTime();
    m_scoreTime += (now - m_resumeTime);
    m_resumeTime = now;

    if (!m_showClock) {
        m_score = 1000 / static_cast<int>(m_scoreTime + 1);
    }

    if (m_numBlocks == m_blocks.size()) {
        //Start/Update the clock
        if (!m_showClock) {
            m_showClock = true;
            m_time = m_scoreTime + 6L;
        } else if (m_showClock && m_time <= m_scoreTime + 1L ) {
            endGamePlay(true);
            return;
        }
    }

    //Go through all the blocks that are in game and see if any have fallen off the screen
    //Also update block anger state of each block
    for (unsigned int i = 0; i < m_numBlocks; i++) {
        b2Vec2 position = m_blocks[i].body->GetPosition();

        if (position.y < 0) {
            endGamePlay(false);
            // Game is done, don't bother updating the other blocks
            return;
        }

        b2Vec2 velocity = m_blocks[i].body->GetLinearVelocity();

        if ((velocity.x > m_velocityTreshold) || (velocity.x <-m_velocityTreshold) ||
                (velocity.y > m_velocityTreshold) || (velocity.y <-m_velocityTreshold)) {
            m_blocks[i].countdown = m_platform.getCurrentTime() + m_angstCountdown;
            m_blocks[i].angst = true;
        }

        if (m_blocks[i].countdown <= m_platform.getCurrentTime()) {
            m_blocks[i].angst = false;
            m_blocks[i].countdown = LONG_MAX;
        }
    }
}

void GameLogic::renderFetchUser() {
    m_platform.beginRender();

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_background.draw();

    glColor4f(0.75f, 0.75f, 0.75f, 1.0f);
    bbutil_render_text(m_font, m_message, m_messagePosX, m_messagePosY);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    m_platform.finishRender();
}

void GameLogic::renderGame() {
    m_platform.beginRender();

    //Typical rendering pass
    glClear(GL_COLOR_BUFFER_BIT);

    //Draw background
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_background.draw();

    if (m_gamePaused) {
        glColor4f(0.75f, 0.75f, 0.75f, 1.0f);
        bbutil_render_text(m_font, m_message, m_messagePosX, m_messagePosY);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);

        m_platform.finishRender();

        return;
    }

    //Draw ground
    m_ground.draw();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    //Draw in game blocks
    for (unsigned int i = 0; i < m_numBlocks; i++) {
        glPushMatrix();
            b2Vec2 position = m_blocks[i].body->GetPosition();

            glTranslatef(static_cast<float>(position.x) / m_b2ScaleFactor, static_cast<float>(position.y) /m_b2ScaleFactor, 0.0f);
            glRotatef((180 * m_blocks[i].body->GetAngle() / M_PI), 0.0f, 0.0f, 1.0f);

            if (m_blocks[i].angst) {
                m_blocks[i].belligerent->draw();
            } else {
                m_blocks[i].resting->draw();
            }
        glPopMatrix();
    }

    //Draw blocks to be added
    float pos = 10.0f;

    for (unsigned int i = m_blocks.size(); i > m_numBlocks; i--) {
        pos += 0.55f * m_blocks[i - 1].Width() / 2;

        glPushMatrix();
            glTranslatef(m_sceneWidth - pos, m_sceneHeight - m_blocksToAddYOffset , 0.0f);
            glScalef(0.55f, 0.55f, 1.0f);
            m_blocks[i - 1].resting->draw();
        glPopMatrix();

        pos += 0.55f * m_blocks[i - 1].Width() / 2 + 1.5f;
    }

    //Display score
    char buf[100];
    sprintf(buf, "%i\0", m_score);
    glColor4f(0.75f, 0.75f, 0.75f, 1.0f);
    bbutil_render_text(m_scoreFont, buf, m_scorePosX, m_scorePosY);

    //Display timer if it is available
    if (m_showClock) {
        sprintf(buf, "Time left %i\0", m_time - m_scoreTime - 1l);
        bbutil_render_text(m_font, buf, m_timerPosX, m_timerPosY);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    m_platform.finishRender();
}

void GameLogic::renderLeadBoard() {
    //Render leader board screen
    m_platform.beginRender();

    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_background.draw();
    m_leaderBoard.draw();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    bbutil_render_text(m_font, m_message, m_messagePosX, m_messagePosY);

    if (m_leaderBoardReady) {
        m_playButton.draw();

        float posY, sizeX, sizeY;
        char buf[100];

        bbutil_measure_text(m_leaderboardFont, "123", &sizeX, &sizeY);
        posY = m_leaderBoard.PosY() + m_leaderBoard.Height() / 2 - sizeY - LEADERBOARD_LINE_OFFSET_Y;

        for (int i = 0; i < static_cast<int>(m_leaderboard.size()); i++) {
            sprintf(buf, "%i. %s", m_leaderboard[i].rank(), m_leaderboard[i].name().c_str());
            bbutil_render_text(m_leaderboardFont, buf, m_leaderBoard.PosX() - m_leaderBoard.Width() / 2 + LEADERBOARD_LINE_OFFSET_X, posY);

            sprintf(buf, "%i", m_leaderboard[i].score());
            bbutil_measure_text(m_leaderboardFont, buf, &sizeX, &sizeY);

            bbutil_render_text(m_leaderboardFont, buf, m_leaderBoard.PosX() + m_leaderBoard.Width() / 2 - sizeX - LEADERBOARD_LINE_OFFSET_X, posY);

            posY -= sizeY + 10.0f;
        }
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    m_platform.finishRender();
}

void GameLogic::addNextShape(float x, float y) {
       if (m_numBlocks == m_blocks.size()) {
           return;
    }

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(x * m_b2ScaleFactor, y * m_b2ScaleFactor);

    m_blocks[m_numBlocks].body = m_world.CreateBody(&bodyDef);

    b2PolygonShape dynamicBox;
       dynamicBox.SetAsBox((m_blocks[m_numBlocks].Width() / 2 - 1.5f) * m_b2ScaleFactor,
                           (m_blocks[m_numBlocks].Height() / 2 - 1.5f) * m_b2ScaleFactor);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &dynamicBox;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 0.7f;

    m_blocks[m_numBlocks].body->CreateFixture(&fixtureDef);

    m_numBlocks++;

    m_blockFall.play();
}

void GameLogic::endGamePlay(bool win) {
    m_gameFinished = true;
    m_state = LeaderBoard;

    if (win) {
        m_message = "Congratulations, you won!";
    } else {
        m_message = "Sorry, you lost. Another try maybe?";
    }

    float sizeX, sizeY;
    bbutil_measure_text(m_font, m_message, &sizeX, &sizeY);
    m_messagePosX = (m_sceneWidth - sizeX) / 2;
    m_messagePosY = m_sceneHeight - sizeY - MESSAGE_OFFSET_Y;

    if (win) {
        m_platform.submitScore(m_score);
    } else {
        m_platform.fetchLeaderboard();
    }
}

void GameLogic::reset() {
    //Initialize shape list
    for (unsigned int i = 0; i < m_blocks.size(); i++) {
        m_blocks[i].angst = false;
    }
    m_state = GamePlay;

    //Clear Box2D world
    for (unsigned int i = 0; i < m_numBlocks; i++) {
        m_world.DestroyBody(m_blocks[i].body);
    }

    m_numBlocks = 0;

    m_gamePaused = false;
    m_gameFinished = false;
    m_showClock = false;
    m_leaderBoardReady = false;

    m_scoreTime = 0;
    m_resumeTime = m_platform.getCurrentTime();
}

void GameLogic::onLeftRelease(float x, float y) {
    if (m_state == LeaderBoard && m_leaderBoardReady) {
        if (m_playButton.isPressed) {
            m_playButton.isPressed = false;
            m_playButton.textY--;
            if (m_playButton.isWithin(x, m_sceneHeight - y)) {
                reset();
            }
        }
    } else if (m_state == GamePlay) {
        if (m_gamePaused) {
            m_gamePaused = false;
            // We are "resuming" play
            m_resumeTime = m_platform.getCurrentTime();
            m_click1.play();
        } else {
            addNextShape(x, m_sceneHeight - y);
        }
    }
}

void GameLogic::onLeftPress(float x, float y) {
    if (m_state == LeaderBoard && m_leaderBoardReady) {
        if (m_playButton.isWithin(x, m_sceneHeight - y)) {
            m_playButton.isPressed = true;
            m_playButton.textY++;
            m_click1.play();
        }
    }
}

void GameLogic::button::setPosition(float x, float y) {
    posX = x;
    posY = y;
    regular->setPosition(x, y);
    pressed->setPosition(x, y);
}

void GameLogic::button::draw() const {
    if (isPressed == true) {
        pressed->draw();
    } else {
        regular->draw();
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    bbutil_render_text(font, text, posX + textX, posY + textY);
}

void GameLogic::onExit() {
    m_shutdown = true;
}

void GameLogic::onPause() {
    if (m_gamePaused == false && m_state == GamePlay) {
        m_gamePaused = true;

        m_message = "Game paused, tap screen to start";
        float textSizeX, textSizeY;
        bbutil_measure_text(m_font, m_message, &textSizeX, &textSizeY);
        m_messagePosX = (m_sceneWidth - textSizeX) / 2;
        m_messagePosY = (m_sceneHeight - textSizeY) / 2;
    }
    m_backgroundMusic.pause();
}

void GameLogic::onResume() {
    m_backgroundMusic.play();
}

void GameLogic::onSubmitScore() {
    // Score submitted, now lets fetch the leaderboard
    m_platform.fetchLeaderboard();
}

void GameLogic::onLeaderboardReady(const std::vector<Score>& leaderboard) {
    m_leaderBoardReady = true;
    m_leaderboard = leaderboard;
}

void GameLogic::onUserReady(const std::string& userName, bool isAnonymous, const std::string& errorString) {
    if (isAnonymous) {
        if (!errorString.empty()){
            m_platform.displayPrompt(errorString + ", please enter your name");
        } else {
            m_platform.displayPrompt("Please enter your name");
        }
    } else {
        // We've got the user name, proceed with game play (which starts paused).
        m_state = GamePlay;

        m_message = "Game paused, tap screen to start";
        float textSizeX, textSizeY;
        bbutil_measure_text(m_font, m_message, &textSizeX, &textSizeY);
        m_messagePosX = (m_sceneWidth - textSizeX) / 2;
        m_messagePosY = (m_sceneHeight - textSizeY) / 2;
    }
}

void GameLogic::onPromptOk(const std::string& input) {
    m_platform.submitUserName(input);
}

} /* namespace blocks */
