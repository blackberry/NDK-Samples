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

#include "Platform.h"
#include "bbutil.h"

#include <sqlite3.h>

#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <bps/screen.h>

#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifndef NDEBUG
#include <assert.h>
#define ASSERT(a) (assert(a))
#else
#define ASSERT(a)
#endif

namespace blocks {

static const char SCORELOOP_GAME_ID[] = "d346c484-12aa-49a2-a0a0-de2f87492d72";
static const char SCORELOOP_GAME_SECRET[] = "aAa+DehBfyGO/CYaE3nWomgu7SIbWFczUih+Qwf3/n7u0y3nyq5Hag==";
static const char SCORELOOP_GAME_VERSION[] = "1.0";
static const char SCORELOOP_GAME_CURRENCY[] = "ASC";
static const char SCORELOOP_GAME_LANGUAGE[] = "en";

static const int NUM_LEADERBOARD_SCORES = 5;

Platform::Platform()
    : m_handler(NULL)
    , m_screenContext(NULL)
    , m_scoreloopClient(NULL)
    , m_userController(NULL)
    , m_scoreController(NULL)
    , m_scoresController(NULL)
    , m_score(NULL)
    , m_scoreOperationInProgress(false)
    , m_leaderboardOperationInProgress(false)
    , m_userOperationInProgress(false)
    , m_promptInProgress(false)
    , m_buttonPressed(false)
    , m_isPaused(false)
    , m_db(NULL)
{

    // Start BPS and screen
    bps_initialize();

    screen_create_context(&m_screenContext, 0);

    screen_request_events(m_screenContext);
    navigator_request_events(0);
    dialog_request_events(0);

    // Lock in landscape mode.
    navigator_rotation_lock(true);
    bbutil_init_egl(m_screenContext);
}

Platform::~Platform() {
    sqlite3_close(m_db);

    SC_Score_Release(m_score);
    SC_ScoresController_Release(m_scoresController);
    SC_ScoreController_Release(m_scoreController);
    SC_UserController_Release(m_userController);
    SC_Client_Release(m_scoreloopClient);

    bbutil_terminate();
    screen_stop_events(m_screenContext);
    screen_destroy_context(m_screenContext);
    bps_shutdown();
}

bool Platform::init() {
    // Fire up BBM Game SDK (Scoreloop)
    SC_InitData_Init(&m_scoreloopInitData);

    SC_Error_t rc = SC_Client_New(&m_scoreloopClient,
            &m_scoreloopInitData,
            SCORELOOP_GAME_ID,
            SCORELOOP_GAME_SECRET,
            SCORELOOP_GAME_VERSION,
            SCORELOOP_GAME_CURRENCY,
            SCORELOOP_GAME_LANGUAGE);

    if (rc != SC_OK) {
        if (rc == SC_PAL_INITIALIZATION_FAILED) {
            fprintf(stderr, "Hint: check bar descriptor to ensure read_device_identifying_information is one of the requested actions.: %d\n", rc);
            return false;
        } else {
            fprintf(stderr, "Error initializing scoreloopcore: %d\n", rc);
            return false;
        }
    }

    rc = SC_Client_CreateUserController(m_scoreloopClient, &m_userController, fetchUserComplete, this);
    if (rc != SC_OK) {
        fprintf(stderr, "Error creating user controller: %d\n", rc);
        return false;
    }

    rc = SC_Client_CreateScoreController(m_scoreloopClient, &m_scoreController, submitScoreComplete, this);
    if (rc != SC_OK) {
        fprintf(stderr, "Error creating score controller: %d\n", rc);
        return false;
    }

    rc = SC_Client_CreateScoresController(m_scoreloopClient, &m_scoresController, fetchLeaderboardComplete, this);
    if (rc != SC_OK) {
        fprintf(stderr, "Error creating scores (leaderboard) controller: %d\n", rc);
        return false;
    }

    SC_ScoresController_SetMode(m_scoresController, 0);
    SC_ScoresController_SetSearchList(m_scoresController, SC_SCORE_SEARCH_LIST_GLOBAL);
    SC_ScoresController_SetTimeInterval(m_scoresController, SC_TIME_INTERVAL_GLOBAL);

    struct stat fileInfo;
    if (stat("data/scores.db", &fileInfo) == -1) {
        // copy the file into a directory we can write
        FILE *destination = fopen("data/scores.db", "wb");

        if (destination == NULL) {
            fprintf(stderr, "Error opening target scores.db for copy.\n");
            return false;
        }

        FILE *source = fopen("app/native/data/scores.db", "rb");

        if (source == NULL) {
            fprintf(stderr, "Error opening source scores.db for copy.\n");
            fclose(destination);
            return false;
        }

        while(!feof(source)) {
            char buffer[1024 * 4];
            size_t readData = fread(&buffer, 1, sizeof(buffer), source);

            if (readData == 0) {
                fprintf(stderr, "Error reading source file.  %d", errno);
                fclose(destination);
                fclose(source);
                return false;
            }

            size_t writeData = fwrite(&buffer, 1, readData, destination);

            if (readData != writeData) {
                fprintf(stderr, "Error copying file.  read %d, write %d\n", readData, writeData);
                fclose(destination);
                fclose(source);
                return false;
            }

        }

        fclose(destination);
        fclose(source);
    }

    // Now we can open it.
    rc = sqlite3_open_v2("data/scores.db", &m_db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Could not open scores db. %d\n", rc);
        return false;
    }

    return true;
}

// Process all input events before returning.  Really we should be moving
// fast enough that there shouldn't be more than a single event queued up.
void Platform::processEvents() {
    ASSERT(m_handler);

    while (true) {
        bps_event_t* event = NULL;
        int rc = bps_get_event(&event, 0);

        ASSERT(BPS_SUCCESS == rc);
        if (rc != BPS_SUCCESS) {
            fprintf(stderr, "BPS error getting an event: %d\n", errno);
            break;
        }

        if (event == NULL) {
            // No more events in the queue
            break;
        }

        // Give Scoreloop the first shot at handling the event
        // (for callbacks)
        if (SC_HandleBPSEvent(&m_scoreloopInitData, event) == BPS_SUCCESS) {
            continue;
        }

        // Not a ScoreLoopCore event, continue processing...
        int domain = bps_event_get_domain(event);
        int code = bps_event_get_code(event);
        if (navigator_get_domain() == domain) {
            switch(code) {
            case NAVIGATOR_EXIT:
                m_handler->onExit();
                break;
            case NAVIGATOR_WINDOW_INACTIVE:
                m_isPaused = true;
                m_handler->onPause();
                break;
            case NAVIGATOR_WINDOW_ACTIVE:
                if (m_isPaused) {
                    m_handler->onResume();
                    m_isPaused = false;
                }
                break;
            }
        } else if (screen_get_domain() == domain) {
            screen_event_t screenEvent = screen_event_get_event(event);
            int screenEventType;
            int screenEventPosition[2];

            screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_TYPE, &screenEventType);
            screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_SOURCE_POSITION, screenEventPosition);

            if (screenEventType == SCREEN_EVENT_MTOUCH_TOUCH) {
                m_handler->onLeftPress(static_cast<float>(screenEventPosition[0]), static_cast<float>(screenEventPosition[1]));
            } else if (screenEventType == SCREEN_EVENT_MTOUCH_RELEASE) {
                m_handler->onLeftRelease(static_cast<float>(screenEventPosition[0]), static_cast<float>(screenEventPosition[1]));
            } else if (screenEventType == SCREEN_EVENT_POINTER) {
                int pointerButton;
                screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_BUTTONS, &pointerButton);

                if (pointerButton == SCREEN_LEFT_MOUSE_BUTTON) {
                    ASSERT(!m_buttonPressed);
                    m_buttonPressed = true;
                    m_handler->onLeftPress(static_cast<float>(screenEventPosition[0]), static_cast<float>(screenEventPosition[1]));
                } else if (m_buttonPressed) {
                    m_handler->onLeftRelease(static_cast<float>(screenEventPosition[0]), static_cast<float>(screenEventPosition[1]));
                    m_buttonPressed = false;
                }
            }
        } else if (dialog_get_domain() == domain) {
            if (DIALOG_RESPONSE == code) {
                ASSERT(m_promptInProgress);
                m_promptInProgress = false;
                m_handler->onPromptOk(dialog_event_get_prompt_input_field(event));
            }
        } else {
            fprintf(stderr, "Unrecognized and unrequested event! domain: %d, code: %d\n", domain, code);
        }
    }
}

void Platform::beginRender() {
    // TODO
}

void Platform::finishRender() {
    bbutil_swap();
}

int Platform::getDPI() const {
    return bbutil_calculate_dpi(m_screenContext);
}

void Platform::getSize(float& width, float& height) const {
    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;

    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surfaceHeight);

    EGLint rc = eglGetError();

    if (rc != 0x3000) {
        fprintf(stderr, "Unable to query egl surface dimensions: %d\n", rc);
    }

    width = static_cast<float>(surfaceWidth);
    height = static_cast<float>(surfaceHeight);
}

time_t Platform::getCurrentTime() const {
    return time(NULL);
}

void Platform::submitScore(int score) {

    if (m_scoreOperationInProgress) {
        // It is a GameLogic error to call submitScore more than once before
        // submitScoreComplete is executed.
        ASSERT(!"submitScore called while operation in progress");
        return;
    }

    ASSERT(!m_score);
    SC_Error_t rc = SC_Score_New(&m_score);
    if (rc != SC_OK) {
        fprintf(stderr, "Error creating score: %d\n", rc);
        return;
    }

    SC_Score_SetResult(m_score, static_cast<double>(score));
    SC_Score_SetMode(m_score, 0);

    rc = SC_ScoreController_SubmitScore(m_scoreController, m_score);

    if (rc != SC_OK) {
        SC_Score_Release(m_score);
        m_score = NULL;
        fprintf(stderr, "Error submitting score: %d\n", rc);
        return;
    }

    m_scoreOperationInProgress = true;
}

void Platform::submitScoreComplete(SC_Error_t result) {
    ASSERT(m_scoreOperationInProgress);
    ASSERT(m_score);

    m_scoreOperationInProgress = false;

    if (result == SC_HTTP_SERVER_ERROR) {
        // Just store scores locally.
        SC_User_h user = SC_UserController_GetUser(m_userController);
        const char *login = SC_String_GetData(SC_User_GetLogin(user));
        double score = SC_Score_GetResult(m_score);
        submitLocalScore(login, score);
    } else if (result != SC_OK) {
        // A result of SC_HTTP_SERVER_ERROR means the network is toast.
        fprintf(stderr, "Submit score failed: %d\n", result);
    }

    SC_Score_Release(m_score);
    m_score = NULL;
    m_handler->onSubmitScore();
}

void Platform::fetchLeaderboard() {
    if (m_leaderboardOperationInProgress) {
        // It is a GameLogic error to call fetchLeaderboard more than once
        // before fetchLeaderboardComplete is executed.
        ASSERT(!"fetchLeaderboard called while leaderboard operation in progress");
        return;
    }

    SC_Error_t rc = SC_ScoresController_LoadRange(m_scoresController, 0, NUM_LEADERBOARD_SCORES);
    if (rc != SC_OK) {
        fprintf(stderr, "Error loading leaderboard score range: %d\n", rc);
        return;
    }

    m_leaderboardOperationInProgress = true;
}

void Platform::fetchLeaderboardComplete(SC_Error_t result) {
    ASSERT(m_leaderboardOperationInProgress);
    m_leaderboardOperationInProgress = false;

    m_leaderboard.clear();

    if (result == SC_OK) {
        SC_ScoreList_h scoreList = SC_ScoresController_GetScores(m_scoresController);
        if (scoreList == NULL) {
            fprintf(stderr, "Could not copy results to the score list.\n");
            return;
        }

        const unsigned int numScores = SC_ScoreList_GetScoresCount(scoreList);

        for (unsigned int i = 0; i < numScores; i++) {
            SC_Score_h score = SC_ScoreList_GetScore(scoreList, i);
            SC_User_h user = SC_Score_GetUser(score);

            std::string login = "Unknown";

            if (user) {
                login = SC_String_GetData(SC_User_GetLogin(user));
            }

            Score leaderboardScore(SC_Score_GetRank(score), login, static_cast<int>(SC_Score_GetResult(score)));
            m_leaderboard.push_back(leaderboardScore);
        }
    } else if (result == SC_HTTP_SERVER_ERROR) {
        fetchLocalScores(m_leaderboard);
    } else {
        fprintf(stderr, "Fetching leaderboard failed: %d\n", result);
    }

    ASSERT(m_handler);
    m_handler->onLeaderboardReady(m_leaderboard);
}

void Platform::fetchUser() {
    if (m_userOperationInProgress) {
        // It is a GameLogic error to call fetchUser more than once
        // before fetchUserComplete is executed.
        ASSERT(!"fetchUser called while user operation in progress");
        return;
    }

    SC_Error_t result = SC_UserController_RequestUser(m_userController);
    if (result != SC_OK) {
        fprintf(stderr, "Error requesting scoreloop user: %d\n", result);
    }

    m_userOperationInProgress = true;
}

void Platform::fetchUserComplete(SC_Error_t result) {
    ASSERT(m_userOperationInProgress);
    m_userOperationInProgress = false;
    std::string errorString;

    if (result != SC_OK) {
        if (result == SC_INVALID_USER_DATA) {
            SC_UserValidationError_t err = SC_UserController_GetValidationErrors(m_userController);

            switch (err) {
            case SC_USERNAME_ALREADY_TAKEN:
                errorString = "Username already taken";
                break;
            case SC_USERNAME_FORMAT_INVALID:
                errorString = "Username format is invalid";
                break;
            case SC_USERNAME_TOO_SHORT:
                errorString = "Username format is invalid";
                break;
            default:
                errorString = "Something bad happened";
                break;
            }
        } else if (result == SC_HTTP_SERVER_ERROR) {
            SC_User_h user = SC_UserController_GetUser(m_userController);
            const SC_String_h login = SC_User_GetLogin(user);
            if (login == NULL) {
                m_handler->onUserReady("", true, "");
            } else {
                m_handler->onUserReady(SC_String_GetData(login), false, "");
            }
            return;
        } else {
            m_handler->onUserReady("", true, "Error fetching user result.");
            return;
        }
    }

    SC_User_h user = SC_UserController_GetUser(m_userController);

    const bool isAnonymous = (SC_User_GetState(user) == SC_USER_STATE_ANONYMOUS);
    const SC_String_h login = SC_User_GetLogin(user);
    ASSERT(m_handler);
    m_handler->onUserReady(SC_String_GetData(login), isAnonymous, errorString);
}

void Platform::submitUserName(const std::string& userName) {
    if (m_userOperationInProgress) {
        fprintf(stderr, "Already fetching the user!\n");
        return;
    }

    const SC_User_h user = SC_UserController_GetUser(m_userController);

    SC_Error_t rc = SC_User_SetLogin(user, userName.c_str());
    if (SC_OK != rc) {
        fprintf(stderr, "Error setting login: %d\n", rc);
        return;
    }

    rc = SC_UserController_UpdateUser(m_userController);
    if (SC_OK != rc) {
        fprintf(stderr, "Error updating server with user info: %d\n", rc);
        return;
    }

    m_userOperationInProgress = true;
}

void Platform::displayPrompt(const std::string& prompt) {
    if (m_promptInProgress) {
        fprintf(stderr, "There's already a dialog prompt being displayed!\n");
        ASSERT(!"Prompt already displayed");
        return;
    }
    dialog_instance_t displayNamePrompt;
    dialog_create_prompt(&displayNamePrompt);
    dialog_set_size(displayNamePrompt, DIALOG_SIZE_SMALL);

    dialog_set_prompt_message_text(displayNamePrompt, prompt.c_str());
    dialog_add_button(displayNamePrompt, DIALOG_OK_LABEL, true, NULL, true);
    dialog_set_default_button_index(displayNamePrompt, 0);
    dialog_show(displayNamePrompt);

    m_promptInProgress = true;
}

PlatformEventHandler* Platform::setEventHandler(PlatformEventHandler* handler) {
    PlatformEventHandler* oldHandler = m_handler;
    if (handler) {
        m_handler = handler;
    } else {
        fprintf(stderr, "Attempt to set event handler to NULL\n");
        ASSERT(!"Should not set the event handler to NULL");
    }

    return oldHandler;
}

void Platform::submitLocalScore(const char* login, double score) {
    ASSERT(m_db);

    static const char* insertSql = "INSERT INTO 'scores' values (?, ?)";
    sqlite3_stmt* insertStmt;
    int rc = sqlite3_prepare_v2(m_db, insertSql, strlen(insertSql), &insertStmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Sqlite prepare error: %d\n", rc);
        return;
    }

    rc = sqlite3_bind_text(insertStmt, 1, login, strlen(login), NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Sqlite bind username error: %d\n", rc);
        sqlite3_finalize(insertStmt);
        return;
    }

    rc = sqlite3_bind_double(insertStmt, 2, score);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Sqlite bind score error: %d\n", rc);
        sqlite3_finalize(insertStmt);
        return;
    }

    rc = sqlite3_step(insertStmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Sqlite insert step error: %d\n", rc);
        sqlite3_finalize(insertStmt);
        return;
    }

    sqlite3_finalize(insertStmt);

    // Now purge rows that will never be displayed

    static const char* purgeSql = "DELETE FROM 'scores' WHERE ROWID NOT IN (SELECT ROWID FROM 'scores' ORDER BY score DESC LIMIT 5)";
    sqlite3_stmt* purgeStmt;
    rc = sqlite3_prepare_v2(m_db, purgeSql, strlen(purgeSql), &purgeStmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Sqlite prepare (purge) error: %d\n", rc);
        return;
    }

    rc = sqlite3_step(purgeStmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Sqlite purge step error: %d\n", rc);
        sqlite3_finalize(purgeStmt);
        return;
    }

    sqlite3_finalize(purgeStmt);
}

void Platform::fetchLocalScores(std::vector<Score>& scores) {
    ASSERT(m_db);
    static const char* selectSql = "SELECT name,score FROM 'scores' ORDER BY score DESC LIMIT 5";
    sqlite3_stmt* selectStmt;
    int rc = sqlite3_prepare_v2(m_db, selectSql, strlen(selectSql), &selectStmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Sqlite select prepare error: %d\n", rc);
        return;
    }

    int rank = 1;
    rc = sqlite3_step(selectStmt);
    while (rc == SQLITE_ROW) {
        const char* user = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 0));
        Score score(rank, user, sqlite3_column_int(selectStmt, 1));
        scores.push_back(score);
        rank++;
        rc = sqlite3_step(selectStmt);
    }

    sqlite3_finalize(selectStmt);
}

} /* namespace blocks */
