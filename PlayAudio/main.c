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
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mm/renderer.h>
#include <bps/bps.h>
#include <bps/audiomixer.h>
#include <screen/screen.h>

#if ! defined(DEFAULT_AUDIO_OUT)
	#define DEFAULT_AUDIO_OUT "audio:default"
#endif

#define COLOR_PURPLE 0xffff00ff
#define WAV_RELATIVE_PATH "/app/native/farewell.mp3"

static const char *errlist[] = {
	#define ERR( code ) [ code ] = #code
	ERR( MMR_ERROR_NONE ),
	ERR( MMR_ERROR_UNKNOWN ),
	ERR( MMR_ERROR_INVALID_PARAMETER ),
	ERR( MMR_ERROR_INVALID_STATE ),
	ERR( MMR_ERROR_UNSUPPORTED_VALUE ),
	ERR( MMR_ERROR_UNSUPPORTED_MEDIA_TYPE ),
	ERR( MMR_ERROR_MEDIA_PROTECTED ),
	ERR( MMR_ERROR_UNSUPPORTED_OPERATION ),
	ERR( MMR_ERROR_READ ),
	ERR( MMR_ERROR_WRITE ),
	ERR( MMR_ERROR_MEDIA_UNAVAILABLE ),
	ERR( MMR_ERROR_MEDIA_CORRUPTED ),
	ERR( MMR_ERROR_OUTPUT_UNAVAILABLE ),
	ERR( MMR_ERROR_NO_MEMORY ),
	ERR( MMR_ERROR_RESOURCE_UNAVAILABLE ),
	#undef ERR
};
#define NERRS ( sizeof(errlist) / sizeof(errlist[0]) )

static int err (char *msg)
{
    perror (msg);
    return -1;
}

static void mmrerror( mmr_context_t *ctxt, const char *msg ) {
	const mmr_error_info_t *err = mmr_error_info( ctxt );
	unsigned errcode = err->error_code;
	const char *name;
	if ( errcode >= NERRS || ( name = errlist[ errcode ] ) == NULL ) {
		name = "bad error code";
	}
	fprintf( stderr, "%s: error %d (%s)\n", msg, errcode, name );
}

/* Sleep for 5 seconds and then fill the screen with a random color.
 * We don't worry about failure here.
 * In that case the color doesn't change.
 */
void sleepAndFill(screen_context_t screen_cxt, screen_buffer_t screen_buf, screen_window_t screen_win) {
    const int color = 0xFF000000 | rand() << 16  | rand();
	const int screen_fill_attribs[] = { SCREEN_BLIT_COLOR, color, SCREEN_BLIT_END };
	int screen_dirty[4] = { 0, 0, 1024, 600 }; //start with sane default values

	sleep(5);
    screen_fill(screen_cxt, screen_buf, screen_fill_attribs);
    screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, screen_dirty+2);
    screen_post_window(screen_win, screen_buf, 1, screen_dirty, 0);
}

int main( int argc, char **argv ) {
	const char *mmrname = NULL;
	const char *ctxtname = "testplayer";
	const char *audioout = NULL;
	const char *inputtype = "track";
	int count;
	char cwd[PATH_MAX];
	char inputurl[PATH_MAX];
	float volume;
	int rc;
	int     final_return_code = EXIT_FAILURE;

    screen_context_t screen_cxt;
    screen_window_t screen_win;
    screen_buffer_t screen_buf;
    int screen_fill_attribs[] = { SCREEN_BLIT_COLOR, COLOR_PURPLE, SCREEN_BLIT_END };
    int screen_dirty[4] = { 0, 0, 1024, 600 }; //start with sane default values

    int idle_mode = SCREEN_IDLE_MODE_KEEP_AWAKE;
    int usage = SCREEN_USAGE_NATIVE;

    if (screen_create_context(&screen_cxt, 0) != 0)
    {
    	err("failed to create context");
    	return final_return_code;
    }

    if (screen_create_window(&screen_win, screen_cxt) != 0)
    {
    	err("failed to create window");
    	goto fail1;
    }

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage) != 0)
    {
    	err("failed to set native usage mode");
    	goto fail2;
    }

    if (screen_create_window_buffers(screen_win, 1) != 0)
    {
    	err("failed to set native usage mode");
    	goto fail2;
    }

    if(screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf) != 0)
    {
    	err("failed to get screen buffer");
    	goto fail2;
    }

    if (screen_fill(screen_cxt, screen_buf, screen_fill_attribs) != 0) {
    	err("failed to fill the screen");
    	goto fail3;
    }

    if (screen_get_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, screen_dirty+2) != 0) {
    	err("failed to get window size");
    	goto fail3;
    }

    if (screen_post_window(screen_win, screen_buf, 1, screen_dirty, 0) != 0) {
    	err("failed to post the window");
    	goto fail3;
    }

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_IDLE_MODE, &idle_mode) != 0)
    {
    	err("failed to set idle mode");
    	goto fail3;
    }

	getcwd(cwd, PATH_MAX);
	rc = snprintf(inputurl, PATH_MAX, "file://%s%s", cwd, WAV_RELATIVE_PATH);
	if (rc > PATH_MAX - 1)
	{
	  	fprintf (stderr, "File name and path too long");
	   	goto fail3;
	}

	if (bps_initialize() == BPS_FAILURE) {
	  	err ("bps_initialize failed");
	   	goto fail3;
	}

	mode_t mode = S_IRUSR | S_IXUSR;
	int audio_oid; // output ID
	strm_dict_t *aoparams = NULL; // output parameters

	mmr_connection_t *connection;
	mmr_context_t *ctxt;

	audioout = DEFAULT_AUDIO_OUT;

	if ( ( connection = mmr_connect( mmrname ) ) == NULL ) {
		perror( "mmr_connect" );
	} else if ( ( ctxt = mmr_context_create( connection, ctxtname, 0, mode ) ) == NULL ) {
		perror( ctxtname );
	} else if ( audioout && ( audio_oid = mmr_output_attach( ctxt, audioout, "audio" ) ) < 0 ) {
		mmrerror( ctxt, audioout );
	} else if ( aoparams && mmr_output_parameters( ctxt, audio_oid, aoparams ) ) {
		mmrerror( ctxt, "output parameters (audio)" );
	} else if ( mmr_input_attach( ctxt, inputurl, inputtype ) < 0 ) {
		mmrerror( ctxt, inputurl );
	} else if ( mmr_play( ctxt ) < 0 ) {
		mmrerror( ctxt, "mmr_play" );
	} else {

		//3 minutes and 5 seconds is the total length of the sample song.
		rc = audiomixer_get_output_level(AUDIOMIXER_OUTPUT_SPEAKER, &volume);
		if (rc == BPS_SUCCESS) {
			fprintf(stderr, "Successfully queried the output level.\n");
			fprintf(stderr, "current volume is %f\n", volume);
		} else {
			fprintf(stderr, "Failed to query the ouptut level.\n");
		}

		//play for 30 seconds at normal volume
		for (count = 0; count < 30; count += 5)
			sleepAndFill(screen_cxt, screen_buf, screen_win);

		//decrease the volume and play for 30 seconds
		audiomixer_set_output_level(AUDIOMIXER_OUTPUT_SPEAKER, volume / 2.0);
		if (rc == BPS_SUCCESS) {
			fprintf(stderr, "Successfully decreased the volume.\n");
		} else {
			fprintf(stderr, "Failed to decrease the volume.\n");
		}

		//play for 30 seconds at normal volume
		for (count = 0; count < 30; count += 5)
			sleepAndFill(screen_cxt, screen_buf, screen_win);

		//set the volume to original level and play for 30 seconds
		audiomixer_set_output_level(AUDIOMIXER_OUTPUT_SPEAKER, volume);
		if (rc == BPS_SUCCESS) {
			fprintf(stderr, "Successfully set the volume.\n");
		} else {
			fprintf(stderr, "Failed to set the volume.\n");
		}

		//play for 30 seconds at normal volume
		for (count = 0; count < 30; count += 5)
			sleepAndFill(screen_cxt, screen_buf, screen_win);

		//mute for 5 seconds
		audiomixer_set_output_mute(AUDIOMIXER_OUTPUT_SPEAKER, true);
		if (rc == BPS_SUCCESS) {
			fprintf(stderr, "Successfully muted.\n");
		} else {
			fprintf(stderr, "Failed to mute.\n");
		}
		sleepAndFill(screen_cxt, screen_buf, screen_win);

		//toggle the mute setting and play for one and a half minutes (the rest of the song)
		audiomixer_toggle_output_mute(AUDIOMIXER_OUTPUT_SPEAKER);
		if (rc == BPS_SUCCESS) {
			fprintf(stderr, "Successfully toggled the mute setting.\n");
		} else {
			fprintf(stderr, "Failed to toggle the mute setting.\n");
		}
		for (count = 0; count < 90; count += 5)
			sleepAndFill(screen_cxt, screen_buf, screen_win);


		mmr_stop( ctxt );             // Not really necessary -- mmr_input_detach() would take care of this
		mmr_input_detach( ctxt );     // Not really necessary -- mmr_context_destroy()  would take care of this
		mmr_context_destroy( ctxt );  // Not really necessary -- mmr_disconnect() would take care of this
		mmr_disconnect( connection ); // Not really necessary -- exiting would take care of this
		final_return_code = EXIT_SUCCESS;
	}
fail3:
    screen_destroy_buffer(screen_buf);
fail2:
    screen_destroy_window(screen_win);
fail1:
    screen_destroy_context(screen_cxt);
    return final_return_code;

}
