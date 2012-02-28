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

/*
 * @file Platform.h
 *
 *
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <scoreloop/sc_client.h>
#include <scoreloop/sc_init.h>

#include <screen/screen.h>

#include <sqlite3.h>

#include <vector>
#include <string>

#include <time.h>


namespace blocks {

/**
 * A particular user's score from the leaderboard.
 */
class Score {
public:
    Score(int rank, const std::string& name, int score)
        : m_rank(rank), m_name(name), m_score(score) {};

    /**
     * Return the user's login name.
     */
    const std::string& name() const { return m_name; };

    /**
     * Return the user's score, which is time in this case.
     */
    long score() const { return m_score; };

    /**
     * Return the user's global leaderboard rank.
     */
    int rank() const { return m_rank; };

private:
    int m_rank;
    std::string m_name;
    int m_score;
};

/**
 * Methods of this class are called by Platform whenever the game may need to
 * "do something"
 *
 * This class constitutes an interface for receiving system events.  The game
 * calls Platform::setEventHandler with its own implementation of this class.
 * A StubHandler is provided as a console logging implementation.
 */
class PlatformEventHandler {
public:
    /**
     * A touch event, or a bluetooth mouse event occurred.
     *
     * @param x The left point along the x-axis
     * @param y The bottom point along the y-axis
     */
    virtual void onLeftPress(float x, float y) = 0;

    /**
     * A touch event, or a bluetooth mouse event occurred.
     *
     * @param x The left point along the x-axis
     * @param y The bottom point along the y-axis
     */
    virtual void onLeftRelease(float x, float y) = 0;

    /**
     * The app has received an exit event.
     *
     * The game should start closing up any resources it has opened.
     */
    virtual void onExit() = 0;

    /**
     * The app's window is no longer active.
     *
     * Either another application is running in the foreground, or perhaps the
     * device has gone to sleep.  The game should pause as appropriate.
     */
    virtual void onPause() = 0;

    /**
     * The app's window is now active.
     *
     * The user has switched back to the app and it is now running in the
     * foreground.
     */
    virtual void onResume() = 0;

    /**
     * The game has called Platform::submitScore and the score has finished
     * submitting.
     */
    virtual void onSubmitScore() = 0;

    /**
     * The game has called Platform::fetchLeaderboard and now the results are
     * ready.
     *
     * Platform::fetchLeaderboard makes a query to the Scoreloop servers.  It
     * is done asynchronously.  When the results are ready they show up here.
     *
     * @param leaderboard The leaderboard returned by the Scoreloop servers.
     */
    virtual void onLeaderboardReady(const std::vector<Score>& leaderboard) = 0;

    /**
     * The game has called Platform::fetchUser or Platform::submitUserName
     * and now the results are ready.
     *
     * Platform::fetchUser makes a query to the Scoreloop servers.  It is
     * done asynchronously.  When the results are ready they show up here.
     *
     * @param user The Scoreloop login/display name.
     * @param isAnonymous True if the Scoreloop login/display name is just a
     *                       generated stub.
     * @param errorString If a validation error occurred (particularly if
     *                       submitUserName was called), the description will be
     *                       provided here.
     */
    virtual void onUserReady(const std::string& userName, bool isAnonymous, const std::string& errorString) = 0;

    /**
     * The game has called Platform::displayPrompt and a user has pushed on
     * the "Ok" button.
     *
     * @param userInput The user typed input from the prompt's input field.
     */
    virtual void onPromptOk(const std::string& userInput) = 0;

protected:
    ~PlatformEventHandler() {};
};

class Platform {
public:
    /**
     * Fire up the Platform
     *
     */
    Platform();
    ~Platform();

    /**
     * Initialize the Platform
     *
     * Here we make use of Scoreloop's BBM Game API.  The library is not
     * included in the NDK (yet).  You can download the beta of this library at
     * https://www.blackberry.com/beta
     *
     * @return True if initialization occurred successfully.  False otherwise.
     */
    bool init();

    /**
     * Set a PlatformEventHandler.
     *
     * When System events are pulled off of the event queue (@sa processEvents) the appropriate
     * handler method is called.  The caller still retains ownership of the handler passed in,
     * Platform will never call delete on it.  Handler is held internally until setEventHandler
     * is called again with a new handler.
     *
     * @param handler The handler object whose methods will get called.
     * @return The old PlatformEventHandler that the new one replaces.  NULL if
     *            no previous handler was set.
     */
    PlatformEventHandler* setEventHandler(PlatformEventHandler* handler);

    /**
     * Process system events.
     *
     * Process system events by going through the event queue and calling the
     * appropriate PlatformEventHandler method.  The PlatformEventHandler
     * should be set before making this call.
     *
     * @sa setEventHandler
     */
    void processEvents();

    /**
     * Called at the beginning of the render phase.
     */
    void beginRender();

    /**
     * Called at the end of the render phase.
     */
    void finishRender();

    /**
     * Return the surface's width and height.
     *
     * @param width Will be set to the surface width.
     * @param height Will be set to the surface height.
     */
    void getSize(float& width, float& height) const;

    /**
     * Return the DPI of the primary screen.
     *
     * @return The DPI of primary screen.
     */
    int getDPI() const;

    /**
     * Return the current system time (in seconds).
     *
     * @return Current system time (in seconds).
     */
    time_t getCurrentTime() const;

    /**
     * Send the score to the Scoreloop servers for global leaderboard
     * inclusion.
     *
     * In the event of a network error, the score will be submitted
     * to a local database.
     *
     * @param score The score for the completed level.
     */
    void submitScore(int score);

    /**
     * Request the global leaderboard from the Scoreloop servers.
     *
     * Fetch the 5 top global results from the leaderboard.  If there is
     * a network error, it will read from a local database.
     *
     * @sa submitScore
     */
    void fetchLeaderboard();

    /**
     * Request user information from the Scoreloop servers.
     *
     * This is an asynchronous call.  Once the information is fetched, the
     * Platform will call PlatformEventHandler::onUserReady and pass in the
     * required information.
     *
     * User's are created on a per-device basis.  If the user is unknown to
     * Scoreloop an anonymous "stub" user is created.  If that's the case, the
     * game may choose to let the user pick a personalized display name.
     *
     * In the event of a network error, the user will be queried to type in
     * their name.  That name will be cached for the duration play (even if
     * the network comes back up).
     *
     * @sa PlatformEventHandler::onUserReady
     */
    void fetchUser();

    /**
     * Submit a new user name to the Scoreloop servers.
     *
     * User's are created on a per-device basis.  The stub name
     * isn't the most appealing.  So this allows you to change it.
     * Once the operation is complete, PlatformEventHandler::onUserReady will
     * be called with the results.
     *
     * @param userName A user selected name to replace the stub name.
     * @sa Platform::displayPrompt
     * @sa PlatformEventHandler::onUserReady
     */
    void submitUserName(const std::string& userName);

    /**
     * Displays a modal prompt dialog box for capturing user input.
     *
     * Display a prompt dialog with a single input field with the
     * specified "prompt" and a single "Ok" button.  Once the user clicks on
     * the button, PlatformEventHandler::onPromptOk will be called with the
     * results of the input field.
     *
     * @param prompt Text to display in the dialog box.
     * @sa PlatformEventHandler::onPromptOk
     */
    void displayPrompt(const std::string& prompt);

private:
    void submitScoreComplete(SC_Error_t result);
    void fetchLeaderboardComplete(SC_Error_t result);
    void fetchUserComplete(SC_Error_t result);

    static void submitScoreComplete(void* userData, SC_Error_t result) {
        (reinterpret_cast<Platform*>(userData))->submitScoreComplete(result);
    }

    static void fetchLeaderboardComplete(void* userData, SC_Error_t result) {
        (reinterpret_cast<Platform*>(userData))->fetchLeaderboardComplete(result);
    }

    static void fetchUserComplete(void* userData, SC_Error_t result) {
        (reinterpret_cast<Platform*>(userData))->fetchUserComplete(result);
    }

    void submitLocalScore(const char* login, double score);
    void fetchLocalScores(std::vector<Score>& scores);

    SC_InitData_t m_scoreloopInitData;
    std::vector<Score> m_leaderboard;

    PlatformEventHandler* m_handler;
    screen_context_t m_screenContext;
    SC_Client_h m_scoreloopClient;
    SC_UserController_h m_userController;
    SC_ScoreController_h m_scoreController;
    SC_ScoresController_h m_scoresController;
    SC_Score_h m_score;
    bool m_scoreOperationInProgress;
    bool m_leaderboardOperationInProgress;
    bool m_userOperationInProgress;
    bool m_promptInProgress;
    bool m_buttonPressed;
    bool m_isPaused;

    sqlite3* m_db;
};

} /* namespace blocks */
#endif /* PLATFORM_H_ */
