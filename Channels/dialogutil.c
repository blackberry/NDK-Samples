/*
 * Copyright (c) 2011 Research In Motion Limited.
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

#include <stdio.h>
#include <stdlib.h>

#include <screen/screen.h>
#include <bps/bps.h>
#include <bps/dialog.h>
#include <bps/navigator.h>
#include "dialogutil.h"

static screen_context_t screen_ctx = 0;
static screen_window_t screen_win = 0;
static dialog_instance_t top_dialog = 0;
static dialog_instance_t bottom_dialog = 0;


/**
 * Use the PID to set the window group id.
 */
static char *
get_window_group_id()
{
    static char s_window_group_id[16] = "";

    if (s_window_group_id[0] == '\0') {
        snprintf(s_window_group_id, sizeof(s_window_group_id), "%d", getpid());
    }

    return s_window_group_id;
}

int
setup_screen()
{
    if (screen_create_context(&screen_ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        return EXIT_FAILURE;
    }

    //Signal BPS library that navigator orientation is to be locked
    if (BPS_SUCCESS != navigator_rotation_lock(true)) {
        screen_destroy_context(screen_ctx);
        return EXIT_FAILURE;
    }

    if (screen_create_window(&screen_win, screen_ctx) != 0) {
        screen_destroy_context(screen_ctx);
        return EXIT_FAILURE;
    }

    int usage = SCREEN_USAGE_NATIVE;
    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) != 0) goto fail;

    int size[2];
    if (screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, size) != 0) goto fail;

    screen_display_t screen_disp;
    screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_DISPLAY, (void **)&screen_disp);

    screen_display_mode_t screen_mode;
    if (screen_get_display_property_pv(screen_disp, SCREEN_PROPERTY_MODE, (void**)&screen_mode) != 0) goto fail;

    int buffer_size[2] = {size[0], size[1]};

    int angle = atoi(getenv("ORIENTATION"));
    if ((angle == 0) || (angle == 180)) {
       if (((screen_mode.width > screen_mode.height) && (size[0] < size[1])) ||
          ((screen_mode.width < screen_mode.height) && (size[0] > size[1]))) {
            buffer_size[1] = size[0];
        buffer_size[0] = size[1];
       }
    } else if ((angle == 90) || (angle == 270)){
       if (((screen_mode.width > screen_mode.height) && (size[0] > size[1])) ||
          ((screen_mode.width < screen_mode.height && size[0] < size[1]))) {
        buffer_size[1] = size[0];
        buffer_size[0] = size[1];
        }
    } else {
        goto fail;
    }

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, buffer_size) != 0) goto fail;

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ROTATION, &angle) != 0) goto fail;

    if (screen_create_window_buffers(screen_win, 1) != 0) goto fail;

    if (screen_create_window_group(screen_win, get_window_group_id()) != 0) goto fail;

    screen_buffer_t buff;
    if (screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void*)&buff) != 0) goto fail;

    int attribs[1] = {SCREEN_BLIT_END};
    if (screen_fill(screen_ctx, buff, attribs) != 0) goto fail;

    int dirty_rects[4] = {0, 0, buffer_size[0], buffer_size[1]};
    if (screen_post_window(screen_win, buff, 1, (const int*)dirty_rects, 0) != 0) goto fail;

    return EXIT_SUCCESS;

fail:
    perror(NULL);
    cleanup_screen();
    return EXIT_FAILURE;
}


void cleanup_screen() {
    screen_destroy_window(screen_win);
    screen_destroy_context(screen_ctx);
    screen_win = 0;
    screen_ctx = 0;
}



void
create_dialogs()
{
    if (bottom_dialog == NULL) {
        dialog_create_alert(&bottom_dialog);
        dialog_set_alert_message_text(bottom_dialog, "\n");
        dialog_set_background_alpha(bottom_dialog, 0.0);
        dialog_set_size(bottom_dialog, DIALOG_SIZE_SMALL);
        dialog_set_position(bottom_dialog, DIALOG_POSITION_BOTTOM_CENTER);
        dialog_set_group_id(bottom_dialog, get_window_group_id());
        dialog_set_cancel_required(bottom_dialog, true);
        dialog_show(bottom_dialog);
    }

    if (top_dialog == NULL) {
        dialog_create_alert(&top_dialog);
        dialog_set_alert_message_text(top_dialog, "\n");
        dialog_set_background_alpha(top_dialog, 0.0);
        dialog_set_size(top_dialog, DIALOG_SIZE_SMALL);
        dialog_set_position(top_dialog, DIALOG_POSITION_TOP_CENTER);
        dialog_set_group_id(top_dialog, get_window_group_id());
        dialog_set_cancel_required(top_dialog, true);
        dialog_show(top_dialog);
    }

}

void
destroy_dialogs() {
    if (top_dialog) {
        dialog_destroy(top_dialog);
    }
    top_dialog = 0;

    if (bottom_dialog) {
        dialog_destroy(bottom_dialog);
    }
    bottom_dialog = 0;

}

void
show_top_dialog_message(const char * msg) {
    dialog_set_alert_message_text(top_dialog, msg);
    dialog_update(top_dialog);
    fprintf(stderr, "%s\n", msg);
}



void
show_bottom_dialog_message(const char * msg) {
    dialog_set_alert_message_text(bottom_dialog, msg);
    dialog_update(bottom_dialog);
    fprintf(stderr, "%s\n", msg);

}
