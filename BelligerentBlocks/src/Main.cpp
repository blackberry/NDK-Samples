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
 * @file Main.cpp
 */

#include "Platform.h"
#include "GameLogic.h"

#include <AL/alut.h>

#include <stdlib.h>

using blocks::GameLogic;
using blocks::Platform;

int main(int argc, char ** argv) {

    Platform platform;
    if (!platform.init()) {
        fprintf(stderr, "Could not init platform\n");
        return EXIT_FAILURE;
    }

    if (!alutInit(0, 0)) {
        fprintf(stderr, "Could not start sound: %d\n", alutGetError());
        return EXIT_FAILURE;
    }

    GameLogic game(platform);
    game.run();

    alutExit();

    return EXIT_SUCCESS;
}
