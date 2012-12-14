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

#include <fcntl.h>

#include <bps/bps.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <bps/screen.h>

#include <png.h>
#include <errno.h>

#include <screen/screen.h>
#include <sys/keycodes.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <math.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bbutil.h"

// This macro provides error checking for all calls to libscreen APIs.
static int rc;
#define SCREEN_API(x, y) rc = x; \
    if (rc) fprintf(stderr, "\n%s in %s: %d", y, __FUNCTION__, errno)

// Constants used when allocating memory for our graphical data.
static const int QUAD_COUNT = 41;
static const int VERTEX_COORD_COUNT = 328;    // QUAD_COUNT * 4 vertices per image * 2 vertex coords per vertex.
static const int INDEX_COUNT = 246;           // QUAD_COUNT * 6 indices per quad.
static const int TEXCOORD_COUNT = 328;        // QUAD_COUNT * 8 UVs per quad.

// Size and positions of all the controls.
static const float ANALOG0_X = 75.0f;
static const float ANALOG1_X  = 460.0f;
static const float ANALOG_Y = 75.0f;
static const float ANALOG_SIZE = 100.0f;

static const float DPAD_X = 25.0f;
static const float DPAD_Y = 275.0f;
static const float DPAD_LONG = 112.5f;
static const float DPAD_SHORT = 75.0f;

static const float BUTTONS_X = 400.0f;
static const float BUTTONS_Y = 275.0f;
static const float BUTTON_SIZE = 75.0f;

static const float LEFT_TRIGGERS_X = 25.0f;
static const float RIGHT_TRIGGERS_X = 450.0f;
static const float TRIGGERS_Y = 525.0f;
static const float TRIGGER_WIDTH = 175.0f;
static const float TRIGGER_HEIGHT = 50.0f;

static const float SELECT_X = 237.5f;
static const float SELECT_Y = 450.0f;

// Other constants.
static const int FONT_SIZE = 4;

// Texture coordinates for each image in our texture atlas.
static GLfloat _outerUVs[4] = { 0.0f, 1.0f, 0.25f, 0.75f };
static GLfloat _innerUVs[4] = { 0.25f, 1.0f, 0.5f, 0.75f };
static GLfloat _buttonDownUVs[4] = { 0.0f, 0.7109375f, 0.166015625f, 0.544921875f };
static GLfloat _buttonUpUVs[4] = { 0.166015625f, 0.7109375f, 0.33203125f, 0.544921875f };
static GLfloat _triggerDownUVs[4] = { 0.0f, 0.51171875f, 0.1474609375f, 0.466796875f };
static GLfloat _triggerUpUVs[4] = { 0.17578125f, 0.51171875f, 0.3232421875f, 0.466796875f };
static GLfloat _upDPadDownUVs[4] = { 0.0f, 0.267578125f, 0.1005859375f, 0.1201171875f };
static GLfloat _upDPadUpUVs[4] = { 0.390625f, 0.267578125f, 0.4912109375f, 0.1201171875f };
static GLfloat _downDPadDownUVs[4] = { 0.17578125f, 0.267578125f, 0.2763671875f, 0.1201171875f };
static GLfloat _downDPadUpUVs[4] = { 0.56640625f, 0.267578125f, 0.6669921875f, 0.1201171875f };
static GLfloat _leftDPadDownUVs[4] = { 0.0f, 0.4140625f, 0.1474609375f, 0.3125f };
static GLfloat _leftDPadUpUVs[4] = { 0.390625f, 0.4140625f, 0.5380859375f, 0.3125f };
static GLfloat _rightDPadDownUVs[4] = { 0.17578125f, 0.4140625f, 0.3232421875f, 0.3125f };
static GLfloat _rightDPadUpUVs[4] = { 0.56640625f, 0.4140625f, 0.7138671875f, 0.3125f } ;

// Each button type corresponds to a set of texture coordinates from those defined above.
typedef enum ButtonType_t {
    DPAD_UP,
    DPAD_DOWN,
    DPAD_LEFT,
    DPAD_RIGHT,
    TRIGGER,
    BUTTON
} ButtonType;

// This structure defines a quad that can be drawn to the screen.
typedef struct Quad_t {
    GLfloat x;
    GLfloat y;
    GLfloat width;
    GLfloat height;
    GLfloat* uvs;
} Quad;

// This structure contains everything needed to map a physical button on a device to a virtual button on the screen.
typedef struct Button_t {
    // A button's type determines which set of UVs it uses.
    ButtonType type;
    int mapping;
    Quad* quad;
    char* label;
} Button;

// The possible values for Button.mapping.
static int _buttonMappings[32];

// Objects used by the application.
static screen_context_t _screen_ctx;
static font_t* _font;

// Miscellaneous variables used by the application.
static bool _shutdown;
static bool _polling;
static float _surfaceHeight;
static float _surfaceWidth;

// Storage for our graphical data.
static unsigned int _gamepadTexture;
static GLfloat* _vertices;
static GLushort* _indices;
static GLfloat* _textureCoords;
static Quad _quads[41];

// Pointers to the quads and buttons we'll need to modify during frame updates.
static Quad* _analog0Inner[2];
static Quad* _analog1Inner[2];
static Button _buttons[2][16];

// Tapping an on-screen button will make it 'active', and the next gamepad button-press will map that gamepad button to this on-screen button.
static Button* _activeButton[2];

static Button _pollingButton;

// Structure representing a game controller.
typedef struct GameController_t {
    // Static device info.
    screen_device_t handle;
    int type;
    int analogCount;
    int buttonCount;
    char id[64];
    char vendor[64];
    char product[64];

    // Current state.
    int buttons;
    int analog0[3];
    int analog1[3];

    // Text to display to the user about this controller.
    char deviceString[256];
    char buttonsString[128];
    char analog0String[128];
    char analog1String[128];
} GameController;

// Controller information.
static int MAX_CONTROLLERS = 2;
static int MAX_BUTTONS = 16;
static GameController _controllers[2];

static void initController(GameController* controller, int player)
{
    // Initialize controller values.
    controller->handle = 0;
    controller->type = 0;
    controller->analogCount = 0;
    controller->buttonCount = 0;
    controller->buttons = 0;
    controller->analog0[0] = controller->analog0[1] = controller->analog0[2] = 0;
    controller->analog1[0] = controller->analog1[1] = controller->analog1[2] = 0;
    sprintf(controller->deviceString, "Player %d: No device detected.", player + 1);
}

int init()
{
    // Initialize our static variables and controllers.
    _font = NULL;
    _shutdown = false;
    _polling = false;

    int i;
    for (i = 0; i < MAX_CONTROLLERS; ++i) {
        initController(&_controllers[i], i);
        _activeButton[i] = NULL;
    }

    // Populate an array of button mappings.
    for (i = 0; i < 32; ++i) {
        _buttonMappings[i] = 1 << i;
    }

    EGLint surface_width, surface_height;

    // Query width and height of the window surface created by utility code.
    eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &surface_width);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &surface_height);

    EGLint err = eglGetError();
    if (err != EGL_SUCCESS) {
        fprintf(stderr, "Unable to query EGL surface dimensions\n");
        return EXIT_FAILURE;
    }

    _surfaceHeight = (float) surface_height;
    _surfaceWidth = (float) surface_width;

    // Calculate our display's DPI and load our font using utility code.
    int dpi = bbutil_calculate_dpi(_screen_ctx);
    _font = bbutil_load_font("/usr/fonts/font_repository/monotype/cour.ttf", FONT_SIZE, dpi);

    if (!_font) {
        fprintf(stderr, "Unable to load font.\n");
        return EXIT_FAILURE;
    }

    // Load our gamepad texture atlas.
    int w, h;
    float tx, ty;
    if (bbutil_load_texture("./app/native/gamepad.png", &w, &h, &tx, &ty, &_gamepadTexture) == EXIT_FAILURE) {
        fprintf(stderr, "Unable to load texture.\n");
        return EXIT_FAILURE;
    }

    // Set the initial positions of all joysticks and buttons.
    for (i = 0; i < MAX_CONTROLLERS; ++i) {
        /**
         * Quads  | Buttons  | Description
         * =======|==========|=============
         * 0-5    | 0, 1     | Analog sticks and triggers.
         * 6-9    | 2-5      | D-Pad.  Up, Down, Left, Right.
         * 10-13  | 6-10     | A, B, X, Y Buttons.
         * 14-17  | 11-14    | Triggers: L1, L2, R1, R2.
         * 18, 19 | 15, 16   | Select, Start.
         */
        int controllerIndex = 20*i;
        float xOffset = (_surfaceWidth * 0.5f)*i;

        int j;
        // Assign quads to all buttons other than L3 and R3.
        for (j = 2; j < 16; ++j) {
            _buttons[i][j].quad = &_quads[j+4 + controllerIndex];
        }

        // D-Pad.
        for (j = 2; j < 6; ++j) {
            _buttons[i][j].type = DPAD_UP + j-2;
        }

        // Buttons.
        for (j = 6; j < 10; ++j) {
            _buttons[i][j].type = BUTTON;
            _buttons[i][j].quad->width = BUTTON_SIZE;
            _buttons[i][j].quad->height = BUTTON_SIZE;
            _buttons[i][j].quad->uvs = _buttonUpUVs;
        }

        // Triggers
        for (j = 10; j < 16; ++j) {
            _buttons[i][j].type = TRIGGER;
            _buttons[i][j].quad->width = TRIGGER_WIDTH;
            _buttons[i][j].quad->height = TRIGGER_HEIGHT;
            _buttons[i][j].quad->uvs = _triggerUpUVs;
        }

        // Set quad positions and sizes.

        // Right stick
        Quad* analog1Outer = &_quads[0 + controllerIndex];
        analog1Outer->x = ANALOG1_X + xOffset;
        analog1Outer->y = ANALOG_Y;
        analog1Outer->width = analog1Outer->height = ANALOG_SIZE;
        analog1Outer->uvs = _outerUVs;

        _analog1Inner[i] = &_quads[1 + controllerIndex];
        _analog1Inner[i]->x = ANALOG1_X + xOffset;
        _analog1Inner[i]->y = ANALOG_Y;
        _analog1Inner[i]->width = _analog1Inner[i]->height = ANALOG_SIZE;
        _analog1Inner[i]->uvs = _innerUVs;

        // R3
        _buttons[i][0].label = "R3";
        _buttons[i][0].type = BUTTON;
        _buttons[i][0].quad = &_quads[2 + controllerIndex];
        _buttons[i][0].quad->x = ANALOG1_X - BUTTON_SIZE*2.0f + xOffset;
        _buttons[i][0].quad->y = ANALOG_Y + BUTTON_SIZE;
        _buttons[i][0].quad->width = BUTTON_SIZE;
		_buttons[i][0].quad->height = BUTTON_SIZE;
		_buttons[i][0].quad->uvs = _buttonUpUVs;
        _buttons[i][0].mapping = SCREEN_R3_GAME_BUTTON;

        // Left stick
        Quad* analog0Outer = &_quads[3 + controllerIndex];
        analog0Outer->x = ANALOG0_X + xOffset;
        analog0Outer->y = ANALOG_Y;
        analog0Outer->width = analog0Outer->height = ANALOG_SIZE;
        analog0Outer->uvs = _outerUVs;

        _analog0Inner[i] = &_quads[4 + controllerIndex];
        _analog0Inner[i]->x = ANALOG0_X + xOffset;
        _analog0Inner[i]->y = ANALOG_Y;
        _analog0Inner[i]->width = _analog0Inner[i]->height = ANALOG_SIZE;
        _analog0Inner[i]->uvs = _innerUVs;

        // L3
        _buttons[i][1].quad = &_quads[5 + controllerIndex];
        _buttons[i][1].type = BUTTON;
        _buttons[i][1].label = "L3";
        _buttons[i][1].quad->x = ANALOG0_X + BUTTON_SIZE*2.0f + xOffset;
        _buttons[i][1].quad->y = ANALOG_Y + BUTTON_SIZE;
        _buttons[i][1].quad->width = BUTTON_SIZE;
		_buttons[i][1].quad->height = BUTTON_SIZE;
		_buttons[i][1].quad->uvs = _buttonUpUVs;
        _buttons[i][1].mapping = SCREEN_L3_GAME_BUTTON;

        // Up
        _buttons[i][2].label = "U";
        _buttons[i][2].quad->x = DPAD_X + DPAD_SHORT + xOffset;
        _buttons[i][2].quad->y = DPAD_Y + DPAD_LONG;
        _buttons[i][2].quad->width = DPAD_SHORT;
        _buttons[i][2].quad->height = DPAD_LONG;
        _buttons[i][2].quad->uvs = _upDPadUpUVs;
        _buttons[i][2].mapping = SCREEN_DPAD_UP_GAME_BUTTON;

        // Down
        _buttons[i][3].label = "D";
        _buttons[i][3].quad->x = DPAD_X + DPAD_SHORT + xOffset;
        _buttons[i][3].quad->y = DPAD_Y;
        _buttons[i][3].quad->width = DPAD_SHORT;
        _buttons[i][3].quad->height = DPAD_LONG;
        _buttons[i][3].quad->uvs = _downDPadUpUVs;
        _buttons[i][3].mapping = SCREEN_DPAD_DOWN_GAME_BUTTON;

        // Left
        _buttons[i][4].label = "L";
        _buttons[i][4].quad->x = DPAD_X + xOffset;
        _buttons[i][4].quad->y = DPAD_Y + DPAD_SHORT;
        _buttons[i][4].quad->width = DPAD_LONG;
        _buttons[i][4].quad->height = DPAD_SHORT;
        _buttons[i][4].quad->uvs = _leftDPadUpUVs;
        _buttons[i][4].mapping = SCREEN_DPAD_LEFT_GAME_BUTTON;

        // Right
        _buttons[i][5].label = "R";
        _buttons[i][5].quad->x = DPAD_X + DPAD_LONG + xOffset;
        _buttons[i][5].quad->y = DPAD_Y + DPAD_SHORT;
        _buttons[i][5].quad->width = DPAD_LONG;
        _buttons[i][5].quad->height = DPAD_SHORT;
        _buttons[i][5].quad->uvs = _rightDPadUpUVs;
        _buttons[i][5].mapping = SCREEN_DPAD_RIGHT_GAME_BUTTON;

        // A, B, X, Y
        _buttons[i][6].label = "A";
        _buttons[i][6].quad->x = BUTTONS_X + BUTTON_SIZE + xOffset;
        _buttons[i][6].quad->y = BUTTONS_Y;
        _buttons[i][6].mapping = SCREEN_A_GAME_BUTTON;

        _buttons[i][7].label = "B";
        _buttons[i][7].quad->x = BUTTONS_X + 2*BUTTON_SIZE + xOffset;
        _buttons[i][7].quad->y = BUTTONS_Y + BUTTON_SIZE;
        _buttons[i][7].mapping = SCREEN_B_GAME_BUTTON;

        _buttons[i][8].label = "X";
        _buttons[i][8].quad->x = BUTTONS_X + xOffset;
        _buttons[i][8].quad->y = BUTTONS_Y + BUTTON_SIZE;
        _buttons[i][8].mapping = SCREEN_X_GAME_BUTTON;

        _buttons[i][9].label = "Y";
        _buttons[i][9].quad->x = BUTTONS_X + BUTTON_SIZE + xOffset;
        _buttons[i][9].quad->y = BUTTONS_Y + 2*BUTTON_SIZE;
        _buttons[i][9].mapping = SCREEN_Y_GAME_BUTTON;

        // Triggers: L1, L2, R1, R2
        _buttons[i][10].label = "L1";
        _buttons[i][10].quad->x = LEFT_TRIGGERS_X + xOffset;
        _buttons[i][10].quad->y = TRIGGERS_Y;
        _buttons[i][10].mapping = SCREEN_L1_GAME_BUTTON;

        _buttons[i][11].label = "L2";
        _buttons[i][11].quad->x = LEFT_TRIGGERS_X + xOffset;
        _buttons[i][11].quad->y = TRIGGERS_Y + TRIGGER_HEIGHT + 25.0f;
        _buttons[i][11].mapping = SCREEN_L2_GAME_BUTTON;

        _buttons[i][12].label = "R1";
        _buttons[i][12].quad->x = RIGHT_TRIGGERS_X + xOffset;
        _buttons[i][12].quad->y = TRIGGERS_Y;
        _buttons[i][12].mapping = SCREEN_R1_GAME_BUTTON;

        _buttons[i][13].label = "R2";
        _buttons[i][13].quad->x = RIGHT_TRIGGERS_X + xOffset;
        _buttons[i][13].quad->y = TRIGGERS_Y + TRIGGER_HEIGHT + 25.0f;
        _buttons[i][13].mapping = SCREEN_R2_GAME_BUTTON;

        // Select, Start
        _buttons[i][14].label = "Select";
        _buttons[i][14].quad->x = SELECT_X + xOffset;
        _buttons[i][14].quad->y = SELECT_Y;
        _buttons[i][14].mapping = SCREEN_MENU1_GAME_BUTTON;

        _buttons[i][15].label = "Start";
        _buttons[i][15].quad->x = SELECT_X + xOffset;
        _buttons[i][15].quad->y = SELECT_Y + TRIGGER_HEIGHT + 25.0f;
        _buttons[i][15].mapping = SCREEN_MENU2_GAME_BUTTON;
    }

    // Finally, one last quad is used for the "polling" button.
    Quad* pollingQuad = &_quads[40];
    pollingQuad->x = (surface_width * 0.5f) - TRIGGER_WIDTH * 0.5f;
    pollingQuad->y = 5.0f;
    pollingQuad->width = TRIGGER_WIDTH;
    pollingQuad->height = TRIGGER_HEIGHT;
    pollingQuad->uvs = _triggerUpUVs;

    _pollingButton.quad = pollingQuad;
    _pollingButton.type = TRIGGER;
    _pollingButton.label = "Polling";

    // Create our vertex and texture coordinate arrays.
    _vertices = (GLfloat*) calloc(VERTEX_COORD_COUNT, sizeof(GLfloat));
    _textureCoords = (GLfloat*) calloc(TEXCOORD_COUNT, sizeof(GLfloat));

    // Create and initialize our index array.
    _indices = (GLushort*) calloc(INDEX_COUNT, sizeof(GLushort));
    _indices[0] = 0;
    _indices[1] = 1;
    _indices[2] = 2;
    _indices[3] = 3;
    _indices[4] = 3;
    _indices[5] = 4;

    int vertexCount = 4;
    for (i = 1; i < QUAD_COUNT; ++i) {
        _indices[i*6] = vertexCount;
        _indices[i*6 + 1] = 1 + vertexCount;
        _indices[i*6 + 2] = 2 + vertexCount;
        _indices[i*6 + 3] = 3 + vertexCount;
        _indices[i*6 + 4] = 3 + vertexCount;
        vertexCount += 4;
        _indices[i*6 + 5] = vertexCount;
    }

    // Initialize OpenGL for 2D rendering.
    glViewport(0, 0, surface_width, surface_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrthof(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Set world coordinates to coincide with screen pixels.
    glScalef(1.0f / (float)surface_width, 1.0f / _surfaceHeight, 1.0f);

    return EXIT_SUCCESS;
}

void finalize()
{
    // Free allocated resources.
    if (_vertices) {
        free(_vertices);
        _vertices = NULL;
    }

    if (_indices) {
        free(_indices);
        _indices = NULL;
    }

    if (_textureCoords) {
        free(_textureCoords);
        _textureCoords = NULL;
    }

    // Destroy the font.
    bbutil_destroy_font(_font);

    // Stop requesting events from libscreen.
    SCREEN_API(screen_stop_events(_screen_ctx), "stop_events");

    // Shut down BPS library for this process.
    bps_shutdown();

    // Use utility code to terminate EGL setup.
    bbutil_terminate();

    // Destroy libscreen context.
    SCREEN_API(screen_destroy_context(_screen_ctx), "destroy_context");
}

static void loadController(GameController* controller)
{
    // Query libscreen for information about this device.
    SCREEN_API(screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_TYPE, &controller->type), "SCREEN_PROPERTY_TYPE");
    SCREEN_API(screen_get_device_property_cv(controller->handle, SCREEN_PROPERTY_ID_STRING, sizeof(controller->id), controller->id), "SCREEN_PROPERTY_ID_STRING");
    SCREEN_API(screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_BUTTON_COUNT, &controller->buttonCount), "SCREEN_PROPERTY_BUTTON_COUNT");

    // Check for the existence of analog sticks.
    if (!screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_ANALOG0, controller->analog0)) {
    	++controller->analogCount;
    }

    if (!screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_ANALOG1, controller->analog0)) {
    	++controller->analogCount;
    }

    if (controller->type == SCREEN_EVENT_GAMEPAD) {
        sprintf(controller->deviceString, "Gamepad device ID: %s", controller->id);
    } else {
        sprintf(controller->deviceString, "Joystick device: %s", controller->id);
    }
}

static void pollDevices()
{
    int i;
    for (i = 0; i < MAX_CONTROLLERS; i++) {
        GameController* controller = &_controllers[i];

        if (controller->handle) {
            // Get the current state of a gamepad device.
            SCREEN_API(screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_BUTTONS, &controller->buttons), "SCREEN_PROPERTY_BUTTONS");

            if (controller->analogCount > 0) {
            	SCREEN_API(screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_ANALOG0, controller->analog0), "SCREEN_PROPERTY_ANALOG0");
            }

            if (controller->analogCount == 2) {
            	SCREEN_API(screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_ANALOG1, controller->analog1), "SCREEN_PROPERTY_ANALOG1");
            }
        }
    }
}

static void handleScreenEvent(bps_event_t *event)
{
    int eventType;

    screen_event_t screen_event = screen_event_get_event(event);
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &eventType);

    switch (eventType) {
        case SCREEN_EVENT_GAMEPAD:
        case SCREEN_EVENT_JOYSTICK:
        {
            if (!_polling) {
                // Determine which controller this is.
                screen_device_t device;
                SCREEN_API(screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_DEVICE, (void**)&device), "SCREEN_PROPERTY_DEVICE");

                GameController* controller = NULL;
                int i;
                for (i = 0; i < MAX_CONTROLLERS; ++i) {
                    if (device == _controllers[i].handle) {
                        controller = &_controllers[i];
                        break;
                    }
                }

                if (!controller) {
                    break;
                }

                // Store the controller's new state.
                SCREEN_API(screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS, &controller->buttons), "SCREEN_PROPERTY_BUTTONS");

                if (controller->analogCount > 0) {
                	SCREEN_API(screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_ANALOG0, controller->analog0), "SCREEN_PROPERTY_ANALOG0");
                }

                if (controller->analogCount == 2) {
                    SCREEN_API(screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_ANALOG1, controller->analog1), "SCREEN_PROPERTY_ANALOG1");
                }
            }
            break;
        }
        case SCREEN_EVENT_DEVICE:
        {
            // A device was attached or removed.
            screen_device_t device;
            int attached;
            int type;

            SCREEN_API(screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_DEVICE, (void**)&device), "SCREEN_PROPERTY_DEVICE");
            SCREEN_API(screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_ATTACHED, &attached), "SCREEN_PROPERTY_ATTACHED");

            if (attached) {
                SCREEN_API(screen_get_device_property_iv(device, SCREEN_PROPERTY_TYPE, &type), "SCREEN_PROPERTY_TYPE");
            }

            int i;
            if (attached && (type == SCREEN_EVENT_GAMEPAD || type == SCREEN_EVENT_JOYSTICK)) {
                for (i = 0; i < MAX_CONTROLLERS; ++i) {
                    if (!_controllers[i].handle) {
                        _controllers[i].handle = device;
                        loadController(&_controllers[i]);
                        break;
                    }
                }
            } else {
                for (i = 0; i < MAX_CONTROLLERS; ++i) {
                    if (device == _controllers[i].handle) {
                        initController(&_controllers[i], i);
                        break;
                    }
                }
            }

            break;
        }

        case SCREEN_EVENT_MTOUCH_RELEASE:
        {
            int pos[2];
            SCREEN_API(screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_SOURCE_POSITION, pos), "SCREEN_PROPERTY_SOURCE_POSITION");
            int x = pos[0];
            int y = _surfaceHeight - pos[1];

            int i;
            for (i = 0; i < MAX_CONTROLLERS; ++i) {
                bool buttonTapped = false;

                int j;
                for (j = 0; j < MAX_BUTTONS; ++j) {
                    Button* button = &_buttons[i][j];
                    Quad* quad = button->quad;

                    // Detect that a button was tapped.
                    if (x > quad->x && x < quad->x + quad->width &&
                        y > quad->y && y < quad->y + quad->height) {
                        _activeButton[i] = button;
                        buttonTapped = true;
                        break;
                    }
                }

                if (_activeButton[i] && !buttonTapped) {
                    // Cancel the button's active state.
                    _activeButton[i] = NULL;
                }
            }

            // The polling button is used to switch between handling all device events and polling devices once per frame.
            Quad* quad = _pollingButton.quad;
            if (x > quad->x && x < quad->x + quad->width &&
                y > quad->y && y < quad->y + quad->height) {
                _polling = !_polling;

                // The polling button lights up when polling is enabled.
                if (_polling) {
                    quad->uvs = _triggerDownUVs;
                } else {
                    quad->uvs = _triggerUpUVs;
                }
            }

            break;
        }
    }
}

static void handleNavigatorEvent(bps_event_t *event)
{
    switch (bps_event_get_code(event)) {
        case NAVIGATOR_EXIT:
            _shutdown = true;
            break;
        default:
            break;
    }
}

static void handleEvents()
{
    int domain;

    // Get the first event in the queue.
    bps_event_t *event = NULL;
    if (BPS_SUCCESS != bps_get_event(&event, 0)) {
        fprintf(stderr, "bps_get_event() failed\n");
        return;
    }

    // Handle all events in the queue.
    // If we don't do this in a loop, we'll only handle one event per frame.
    // If many events are triggered quickly, e.g. by spinning the analog sticks,
    // the queue will grow and the user will see the analog sticks lag.
    while (event) {
        if (event) {
            domain = bps_event_get_domain(event);
            if (domain == navigator_get_domain()) {
                handleNavigatorEvent(event);
            } else if (domain == screen_get_domain()) {
                handleScreenEvent(event);
            }
        }

        if (BPS_SUCCESS != bps_get_event(&event, 0)) {
            fprintf(stderr, "bps_get_event() failed\n");
            return;
        }
    }
}

static void discoverControllers()
{
    // Get an array of all available devices.
    int deviceCount;
    SCREEN_API(screen_get_context_property_iv(_screen_ctx, SCREEN_PROPERTY_DEVICE_COUNT, &deviceCount), "SCREEN_PROPERTY_DEVICE_COUNT");
    screen_device_t* devices = (screen_device_t*)calloc(deviceCount, sizeof(screen_device_t));
    SCREEN_API(screen_get_context_property_pv(_screen_ctx, SCREEN_PROPERTY_DEVICES, (void**)devices), "SCREEN_PROPERTY_DEVICES");

    // Scan the list for gamepad and joystick devices.
    int i;
    int controllerIndex = 0;
    for (i = 0; i < deviceCount; i++) {
        int type;
        SCREEN_API(screen_get_device_property_iv(devices[i], SCREEN_PROPERTY_TYPE, &type), "SCREEN_PROPERTY_TYPE");

        if (!rc && (type == SCREEN_EVENT_GAMEPAD || type == SCREEN_EVENT_JOYSTICK)) {
            // Assign this device to control Player 1 or Player 2.
            GameController* controller = &_controllers[controllerIndex];
            controller->handle = devices[i];
            loadController(controller);

            // We'll just use the first compatible devices we find.
            controllerIndex++;
            if (controllerIndex == MAX_CONTROLLERS) {
                break;
            }
        }
    }

    free(devices);
}

void update()
{
    handleEvents();

    if (_polling) {
        pollDevices();
    }

    int i;
    for (i = 0; i < MAX_CONTROLLERS; ++i) {
        GameController* controller = &_controllers[i];

        // If a button is active, map it to the first gamepad button pressed.
        if (_activeButton[i]) {
            int j;
            for (j = 0; j < controller->buttonCount; ++j) {
                if (controller->buttons & _buttonMappings[j]) {
                    // Erase old button mapping if one exists.
                    int k;
                    for (k = 0; k < 16; ++k) {
                        Button* button = &_buttons[i][k];
                        if (_buttonMappings[j] & button->mapping) {
                            button->mapping = 0;
                            break;
                        }
                    }

                    // Set new button mapping.
                    _activeButton[i]->mapping = _buttonMappings[j];
                    _activeButton[i] = NULL;
                    break;
                }
            }
        }

        float xOffset = (_surfaceWidth * 0.5f)*i;

        // Set the inner joystick positions.
        if (controller->analogCount > 0) {
			_analog0Inner[i]->x = ANALOG0_X + xOffset + (controller->analog0[0] >> 2);
			_analog0Inner[i]->y = ANALOG_Y - (controller->analog0[1] >> 2);
			sprintf(controller->analog0String, "Analog 0: (%4d, %4d)", controller->analog0[0], controller->analog0[1]);
        } else {
        	sprintf(controller->analog0String, "Analog 0: N/A");
        }

        if (controller->analogCount == 2) {
			_analog1Inner[i]->x = ANALOG1_X + xOffset + (controller->analog1[0] >> 2);
			_analog1Inner[i]->y = ANALOG_Y - (controller->analog1[1] >> 2);
			sprintf(controller->analog1String, "Analog 1: (%4d, %4d)", controller->analog1[0], controller->analog1[1]);
        } else {
        	sprintf(controller->analog1String, "Analog 1: N/A");
        }

        // Set the button UVs to correspond to their states, as well as the button text.
        sprintf(controller->buttonsString, "Buttons: ");
        int j;
        for (j = 0; j < 16; ++j) {
            Button* button = &_buttons[i][j];

            if ((controller->buttons & button->mapping) || button == _activeButton[i]) {
                switch (button->type) {
                case DPAD_LEFT:
                    button->quad->uvs = _leftDPadDownUVs;
                    break;
                case DPAD_RIGHT:
                    button->quad->uvs = _rightDPadDownUVs;
                    break;
                case DPAD_UP:
                    button->quad->uvs = _upDPadDownUVs;
                    break;
                case DPAD_DOWN:
                    button->quad->uvs = _downDPadDownUVs;
                    break;
                case TRIGGER:
                    button->quad->uvs = _triggerDownUVs;
                    break;
                case BUTTON:
                    button->quad->uvs = _buttonDownUVs;
                    break;
                }
            } else {
                switch (button->type) {
                case DPAD_LEFT:
                    button->quad->uvs = _leftDPadUpUVs;
                    break;
                case DPAD_RIGHT:
                    button->quad->uvs = _rightDPadUpUVs;
                    break;
                case DPAD_UP:
                    button->quad->uvs = _upDPadUpUVs;
                    break;
                case DPAD_DOWN:
                    button->quad->uvs = _downDPadUpUVs;
                    break;
                case TRIGGER:
                    button->quad->uvs = _triggerUpUVs;
                    break;
                case BUTTON:
                    button->quad->uvs = _buttonUpUVs;
                    break;
                }
            }
        }

        // Append button indices to the string showing which buttons are pressed,
        // regardless of whether or not the buttons are mapped.
        char buttonString[4];
        for (j = 0; j < controller->buttonCount; ++j) {
            if (_buttonMappings[j] & controller->buttons) {
                sprintf(buttonString, "%d ", j);
                strcat(controller->buttonsString, buttonString);
            }
        }
    }
}

void render()
{
    // Clear the screen.
    glClear(GL_COLOR_BUFFER_BIT);

    // Populate vertex and texture coordinate arrays.
    int i;
    for (i = 0; i < QUAD_COUNT; ++i) {
        const GLfloat x = _quads[i].x;
        const GLfloat y = _quads[i].y;
        const GLfloat width = _quads[i].width;
        const GLfloat height = _quads[i].height;
        _vertices[i*8 + 0] = x;
        _vertices[i*8 + 1] = y;
        _vertices[i*8 + 2] = x + width;
        _vertices[i*8 + 3] = y;
        _vertices[i*8 + 4] = x;
        _vertices[i*8 + 5] = y + height;
        _vertices[i*8 + 6] = x + width;
        _vertices[i*8 + 7] = y + height;

        const GLfloat u1 = _quads[i].uvs[0];
        const GLfloat v1 = _quads[i].uvs[1];
        const GLfloat u2 = _quads[i].uvs[2];
        const GLfloat v2 = _quads[i].uvs[3];
        _textureCoords[i*8 + 0] = u1;
        _textureCoords[i*8 + 1] = v2;
        _textureCoords[i*8 + 2] = u2;
        _textureCoords[i*8 + 3] = v2;
        _textureCoords[i*8 + 4] = u1;
        _textureCoords[i*8 + 5] = v1;
        _textureCoords[i*8 + 6] = u2;
        _textureCoords[i*8 + 7] = v1;
    }

    // Draw the virtual gamepad.
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);

    glVertexPointer(2, GL_FLOAT, 0, _vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, _textureCoords);
    glBindTexture(GL_TEXTURE_2D, _gamepadTexture);

    if (_controllers[0].handle || _controllers[1].handle) {
		// Draw the polling button.
		glDrawElements(GL_TRIANGLE_STRIP, 6, GL_UNSIGNED_SHORT, _indices + 240);
    }

    // Draw only connected controllers.
    // Only draw the analog sticks and their buttons (L3, R3) if they're present.
    for (i = 0; i < MAX_CONTROLLERS; ++i) {
    	GameController* controller = &_controllers[i];
    	if (controller->handle) {
    		if (controller->analogCount == 2) {
    			glDrawElements(GL_TRIANGLE_STRIP, 120, GL_UNSIGNED_SHORT, _indices + i*120);
    		} else if (controller->analogCount == 1) {
    			glDrawElements(GL_TRIANGLE_STRIP, 102, GL_UNSIGNED_SHORT, _indices + 18 + i*120);
    		} else {
    			glDrawElements(GL_TRIANGLE_STRIP, 84, GL_UNSIGNED_SHORT, _indices + 36 + i*120);
    		}
    	}
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    // Use utility code to render text.
    // Only draw L3 and R3 labels if they're present.
    for (i = 0; i < MAX_CONTROLLERS; ++i) {
		GameController* controller = &_controllers[i];
		if (controller->handle) {
			if (controller->analogCount == 2) {
    			bbutil_render_text(_font, _buttons[i][0].label, _buttons[i][0].quad->x + 30, _buttons[i][0].quad->y + 30, 1.0f, 0.0f, 0.0f, 1.0f);
    			bbutil_render_text(_font, _buttons[i][1].label, _buttons[i][1].quad->x + 30, _buttons[i][1].quad->y + 30, 1.0f, 0.0f, 0.0f, 1.0f);
			} else if (controller->analogCount == 1) {
    			bbutil_render_text(_font, _buttons[i][1].label, _buttons[i][1].quad->x + 30, _buttons[i][1].quad->y + 30, 1.0f, 0.0f, 0.0f, 1.0f);
			}
		}
    }

    // Now render the rest of the text.
    for (i = 0; i < MAX_CONTROLLERS; ++i) {
        GameController* controller = &_controllers[i];
        float xOffset = (_surfaceWidth * 0.5f)*i;

        bbutil_render_text(_font, controller->deviceString, 5 + xOffset, _surfaceHeight - 20, 1.0f, 0.0f, 0.0f, 1.0f);

        if (controller->handle) {
            // Controller is connected; display info about its current state.
            bbutil_render_text(_font, controller->buttonsString, 5 + xOffset, _surfaceHeight - 40, 1.0f, 0.0f, 0.0f, 1.0f);
            bbutil_render_text(_font, controller->analog0String, 5 + xOffset, _surfaceHeight - 60, 1.0f, 0.0f, 0.0f, 1.0f);
            bbutil_render_text(_font, controller->analog1String, 5 + xOffset, _surfaceHeight - 80, 1.0f, 0.0f, 0.0f, 1.0f);

            // Button labels.
            int j;
            for (j = 2; j < MAX_BUTTONS; ++j) {
                Button* button = &_buttons[i][j];
                if (button->type == TRIGGER) {
                    bbutil_render_text(_font, button->label, button->quad->x + 20, button->quad->y + 20, 1.0f, 0.0f, 0.0f, 1.0f);
                } else if (button->type == DPAD_UP) {
                    bbutil_render_text(_font, button->label, button->quad->x + 30, button->quad->y + 70, 1.0f, 0.0f, 0.0f, 1.0f);
                } else if (button->type == DPAD_RIGHT) {
                    bbutil_render_text(_font, button->label, button->quad->x + 70, button->quad->y + 30, 1.0f, 0.0f, 0.0f, 1.0f);
                } else {
                    bbutil_render_text(_font, button->label, button->quad->x + 30, button->quad->y + 30, 1.0f, 0.0f, 0.0f, 1.0f);
                }
            }
        }
    }

    if (_controllers[0].handle || _controllers[1].handle) {
        bbutil_render_text(_font, _pollingButton.label, _pollingButton.quad->x + 20, _pollingButton.quad->y + 20, 1.0f, 0.0f, 0.0f, 1.0f);
    }

    // Use utility code to update the screen.
    bbutil_swap();
}

int main(int argc, char **argv)
{
    // Create a screen context that will be used to create an EGL surface to receive libscreen events.
    SCREEN_API(screen_create_context(&_screen_ctx, SCREEN_APPLICATION_CONTEXT), "create_context");

    // Initialize BPS library.
    bps_initialize();

    // Use utility code to initialize EGL for rendering with GL ES 1.1.
    if (EXIT_SUCCESS != bbutil_init_egl(_screen_ctx)) {
        fprintf(stderr, "Unable to initialize EGL.\n");
        screen_destroy_context(_screen_ctx);
        return 0;
    }

    // Initialize app data.
    if (EXIT_SUCCESS != init()) {
        fprintf(stderr, "Unable to initialize app logic.\n");
        bbutil_terminate();
        screen_destroy_context(_screen_ctx);
        return 0;
    }

    // Signal BPS library that navigator and screen events will be requested.
    if (BPS_SUCCESS != screen_request_events(_screen_ctx)) {
        fprintf(stderr, "screen_request_events failed\n");
        bbutil_terminate();
        screen_destroy_context(_screen_ctx);
        return 0;
    }

    if (BPS_SUCCESS != navigator_request_events(0)) {
        fprintf(stderr, "navigator_request_events failed\n");
        bbutil_terminate();
        screen_destroy_context(_screen_ctx);
        return 0;
    }

    // Look for attached gamepad and joystick devices.
    discoverControllers();

    // Enter the event loop.
    while (!_shutdown) {
        update();

        render();
    }

    // Clean up resources and shut everything down.
    finalize();

    return 0;
}
