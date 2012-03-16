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

#include <errno.h>
#include <fcntl.h>
#include <gulliver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/slogcodes.h>
#include <ctype.h>
#include <limits.h>
#include <screen/screen.h>
#include <sys/asoundlib.h>

#include <bps/bps.h>
#include <bps/audiomixer.h>
#include <bps/audiodevice.h>
#include <bps/dialog.h>
#include <bps/navigator.h>

#include "dialogutil.h"
/*
 * buffer to store messages that we will display in the dialog
 */
#define MSG_SIZE 1024
static char msg[MSG_SIZE];

const char *riff_id = "RIFF";
const char *wave_id = "WAVE";

#define WAV_RELATIVE_PATH "app/native/sample.wav"

#define SUCCESS  0
#define FAILURE -1


typedef struct
{
    char    tag[4];
    long    length;
}
riff_tag;

typedef struct
{
    char    Riff[4];
    long    Size;
    char    Wave[4];
}
riff_hdr;

typedef struct
{
    short   format_tag;
    short   channels;
    long    samples_per_sec;
    long    avg_bytes_per_sec;
    short   block_align;
    short   bits_per_sample;
}
wave_hdr;

int card = -1;

snd_pcm_t *pcm_handle;
snd_pcm_info_t info;
snd_pcm_channel_params_t pp;
snd_pcm_channel_setup_t setup;
snd_pcm_channel_info_t pi;

snd_mixer_t *mixer_handle;
snd_mixer_group_t group;

wave_hdr wav_header;
int sample_rate;
int sample_channels;
int sample_bits;

int
err(char *message)
{
    snprintf(msg, MSG_SIZE, "%s\n%s", message, strerror(errno));
    show_dialog_message(msg);
    return -1;
}

int
find_tag(FILE * fp, const char *tag)
{
    int ret_val = 0;
    riff_tag tag_bfr = { "", 0 };

    /* Keep reading until we find the tag or hit the EOF. */
    while (fread((unsigned char *) &tag_bfr, sizeof(tag_bfr), 1, fp)) {

        /* If this is our tag, set the length and break. */
        if (strncmp(tag, tag_bfr.tag, sizeof tag_bfr.tag) == 0) {
            ret_val = ENDIAN_LE32(tag_bfr.length);
            break;
        }

        /* Skip ahead the specified number of bytes in the stream */
        fseek(fp, tag_bfr.length, SEEK_CUR);
    }

    /* Return the result of our operation */
    return (ret_val);
}

int
check_hdr(FILE * fp)
{
    riff_hdr riff_header = { "", 0 };

    /* Read the header and make sure that this is indeed a Wave file. */
    if (fread((unsigned char *) &riff_header, sizeof(riff_hdr), 1, fp) == 0)
        return 0;

    if (strncmp(riff_header.Riff, riff_id, strlen(riff_id)) ||
        strncmp(riff_header.Wave, wave_id, strlen(wave_id)))
        return -1;

    return 0;
}

/*
 * General setup of the libasound audio mixer and pcm components.
 * Some of the settings are based on the format of the wav data.
 */
int
setup_snd(const char * name)
{
    int fragsize = -1;
    int num_frags = -1;
    int rtn;
    char *dev_name;

    if (NULL == name) {
        dev_name = "pcmPreferred";
    } else {
        dev_name = (char *) name;
    }

    if ((rtn = snd_pcm_open_name(&pcm_handle, dev_name, SND_PCM_OPEN_PLAYBACK)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_open_name failed: %s\n", snd_strerror(rtn));
        show_dialog_message(msg);
        return FAILURE;
    }

    if ((rtn = snd_pcm_info(pcm_handle, &info)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_info failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }
    card = info.card;

    /* disabling mmap is not actually required in this example but it is included to
     * demonstrate how it is used when it is required.
     */
    if ((rtn = snd_pcm_plugin_set_disable(pcm_handle, PLUGIN_DISABLE_MMAP)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_plugin_set_disable failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }

    memset(&pi, 0, sizeof(pi));
    pi.channel = SND_PCM_CHANNEL_PLAYBACK;
    if ((rtn = snd_pcm_plugin_info(pcm_handle, &pi)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_plugin_info failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }

    memset(&pp, 0, sizeof(pp));

    pp.mode = SND_PCM_MODE_BLOCK;
    pp.channel = SND_PCM_CHANNEL_PLAYBACK;
    pp.start_mode = SND_PCM_START_FULL;
    pp.stop_mode = SND_PCM_STOP_STOP;
    pp.buf.block.frag_size = pi.max_fragment_size;

    if (fragsize != -1) {
        pp.buf.block.frag_size = fragsize;
    }

    pp.buf.block.frags_max = num_frags;
    pp.buf.block.frags_min = 1;

    pp.format.interleave = 1;
    pp.format.rate = sample_rate;
    pp.format.voices = sample_channels;

    if (ENDIAN_LE16(wav_header.format_tag) == 6)
        pp.format.format = SND_PCM_SFMT_A_LAW;
    else if (ENDIAN_LE16(wav_header.format_tag) == 7)
        pp.format.format = SND_PCM_SFMT_MU_LAW;
    else if (sample_bits == 8)
        pp.format.format = SND_PCM_SFMT_U8;
    else if (sample_bits == 24)
        pp.format.format = SND_PCM_SFMT_S24;
    else
        pp.format.format = SND_PCM_SFMT_S16_LE;

    strcpy(pp.sw_mixer_subchn_name, "Wave playback channel");
    if ((rtn = snd_pcm_plugin_params(pcm_handle, &pp)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_plugin_params failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }

    if ((rtn = snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_plugin_prepare failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }

    memset(&setup, 0, sizeof(setup));
    memset(&group, 0, sizeof(group));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    setup.mixer_gid = &group.gid;

    if ((rtn = snd_pcm_plugin_setup(pcm_handle, &setup)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_pcm_plugin_setup failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }

    if (group.gid.name[0] == 0) {
        snprintf(msg, MSG_SIZE, "Mixer Pcm Group [%s] Not Set \n", group.gid.name);
        goto setup_failure;
    }

    if ((rtn = snd_mixer_open(&mixer_handle, card, setup.mixer_device)) < 0) {
        snprintf(msg, MSG_SIZE, "snd_mixer_open failed: %s\n", snd_strerror(rtn));
        goto setup_failure;
    }

    char tmp[MSG_SIZE];
    snprintf(msg, MSG_SIZE, "Format %s \n", snd_pcm_get_format_name(setup.format.format));
    snprintf(tmp, MSG_SIZE, "Frag Size %d \n", setup.buf.block.frag_size);
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Total Frags %d \n", setup.buf.block.frags);
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Rate %d \n", setup.format.rate);
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Voices %d \n", setup.format.voices);
    strlcat(msg, tmp, MSG_SIZE);
    snprintf(tmp, MSG_SIZE, "Mixer Pcm Group [%s]\n", group.gid.name);
    strlcat(msg, tmp, MSG_SIZE);
    show_dialog_message(msg);

    return SUCCESS;

setup_failure:
    show_dialog_message(msg);
    snd_pcm_close(pcm_handle);
    return FAILURE;
}

int
main(int argc, char **argv)
{
    FILE *file;
    int  samples;
    char *sample_buffer;

    int rtn, final_return_code = -1, exit_application = 0;

    int bsize, bytes_read, total_written = 0;
    fd_set rfds, wfds;
    char input_file[PATH_MAX];
    char cwd[PATH_MAX];


    /*
     * Before we can listen for events from the BlackBerry Tablet OS platform
     * services, we need to initialize the BPS infrastructure
     */
    bps_initialize();

    if (setup_screen() != EXIT_SUCCESS) {
        printf("Unable to set up the screen. Exiting.");
        exit(-1);
    }

    /*
     * Once the BPS infrastructure has been initialized we can register for
     * events from the various BlackBerry Tablet OS platform services. The
     * Navigator service manages and delivers application life cycle and
     * visibility events.
     *
     * For this sample, we request Navigator events so we can track when
     * the system is terminating the application (NAVIGATOR_EXIT event).
     * This allows us to clean up application resources.
     * 
     * We request Audio Device events because we want to make sure that
     * we properly handle changes in audio output.
     *
     * We request dialog events to properly initialize the dialog
     * subsystem in order to display status and error messages.  
     */
    if (BPS_SUCCESS != navigator_request_events(0)) {
        fprintf(stderr, "Error requesting navigator events: %s", strerror(errno));
        exit(-1);
    }

    if (BPS_SUCCESS != dialog_request_events(0)) {
        fprintf(stderr, "Error requesting dialog events: %s", strerror(errno));
        exit(-1);
    }

    if (BPS_SUCCESS != audiodevice_request_events(0)) {
        fprintf(stderr, "Error requesting audio device events: %s", strerror(errno));
        exit(-1);
    }

    /*
     * Create and display the dialog.
     */
    create_dialog();

    /*
     * Open and check the input file.
     */
    getcwd(cwd, PATH_MAX);
    rtn = snprintf(input_file, PATH_MAX, "%s/%s", cwd, WAV_RELATIVE_PATH);
    if (rtn > PATH_MAX - 1) {
        err("File name and path too long");
        goto fail1;
    }

    if ((file = fopen(input_file, "r")) == 0) {
        err("File open failed");
        goto fail1;
    }

    if (check_hdr(file) == -1) {
        err("check_hdr failed");
        goto fail2;
    }

    /*
     * Parse the headers in the wav file to figure out what kind of data we
     * are dealing with in the file.
     */
    samples = find_tag(file, "fmt ");
    fread(&wav_header, sizeof(wav_header), 1, file);
    fseek(file,(samples - sizeof(wave_hdr)), SEEK_CUR);

    sample_rate = ENDIAN_LE32(wav_header.samples_per_sec);
    sample_channels = ENDIAN_LE16(wav_header.channels);
    sample_bits = ENDIAN_LE16(wav_header.bits_per_sample);

    snprintf(msg, MSG_SIZE, "SampleRate = %d, channels = %d, SampleBits = %d\n", sample_rate, sample_channels,
        sample_bits);
    show_dialog_message(msg);

    if (setup_snd(NULL)) {
        goto fail2;
    }

    bsize = setup.buf.block.frag_size;
    samples = find_tag(file, "data");
    sample_buffer = malloc(bsize);
    if (!sample_buffer) {
        goto fail3;
    }

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    bytes_read = 1;
    while (total_written < samples && bytes_read > 0 ) {
        bps_event_t *event = NULL;

        while (BPS_SUCCESS == bps_get_event(&event, 0) && event) {
            /*
             * If it is a NAVIGATOR_EXIT event then we are done so stop
             * processing events, clean up and exit
             */
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                if (NAVIGATOR_EXIT == bps_event_get_code(event)) {
                    exit_application = 1;
                    goto success;
                }
            }

            if (bps_event_get_domain(event) == audiodevice_get_domain()) {
                /*
                 * If it is a audio device event then it means a new audio device
                 * has been enabled and a switch is required.  We close the old,
                 * open the new audio device using the path and get the card number so
                 * that we can close and re-open the mixer with the new card
                 * number.
                 */

                const char * audiodevice_path = audiodevice_event_get_path(event);

                if (NULL == audiodevice_path) {
                    snprintf(msg, MSG_SIZE, "audiodevice_event_get_path failed: %s\n", snd_strerror(rtn));
                    show_dialog_message(msg);
                    goto fail5;
                }

                if ((rtn = snd_mixer_close(mixer_handle)) < 0) {
                    snprintf(msg, MSG_SIZE, "snd_mixer_close failed: %s\n", snd_strerror(rtn));
                    show_dialog_message(msg);
                    goto fail4;
                }

                if ((rtn = snd_pcm_close(pcm_handle)) < 0) {
                    snprintf(msg, MSG_SIZE, "snd_pcm_close failed: %s\n", snd_strerror(rtn));
                    show_dialog_message(msg);
                    goto fail3;
                }

                if (setup_snd(audiodevice_path)) {
                    /*
                     * setup_snd() closes pcm and mixer handles in the case of error so we
                     * skip clean up of the handles in the case of failure.
                     */
                    goto fail3;
                }
            }
        }

        if (tcgetpgrp(0) == getpid())
            FD_SET(STDIN_FILENO, &rfds);
        FD_SET(snd_mixer_file_descriptor(mixer_handle), &rfds);
        FD_SET(snd_pcm_file_descriptor(pcm_handle, SND_PCM_CHANNEL_PLAYBACK), &wfds);

        rtn = max(snd_mixer_file_descriptor(mixer_handle),
                   snd_pcm_file_descriptor(pcm_handle, SND_PCM_CHANNEL_PLAYBACK));

        if (select(rtn + 1, &rfds, &wfds, NULL, NULL) == -1) {
            err("select");
            goto fail5;
        }

        if (FD_ISSET(snd_pcm_file_descriptor(pcm_handle, SND_PCM_CHANNEL_PLAYBACK), &wfds)) {
            snd_pcm_channel_status_t status;
            int written = 0;

            if ((bytes_read = fread(sample_buffer, 1, min(samples - total_written, bsize), file)) <= 0)
                continue;
            written = snd_pcm_plugin_write(pcm_handle, sample_buffer, bytes_read);
            if (written < bytes_read) {
                memset(&status, 0, sizeof(status));
                status.channel = SND_PCM_CHANNEL_PLAYBACK;
                if (snd_pcm_plugin_status(pcm_handle, &status) < 0) {
                    show_dialog_message("underrun: playback channel status error\n");
                    goto fail5;
                }

                if (status.status == SND_PCM_STATUS_READY ||
                    status.status == SND_PCM_STATUS_UNDERRUN) {
                    if (snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK) < 0) {
                        show_dialog_message("underrun: playback channel prepare error\n");
                        goto fail5;
                    }
                }
                if (written < 0)
                    written = 0;
                written += snd_pcm_plugin_write(pcm_handle, sample_buffer + written, bytes_read - written);
            }
            total_written += written;
        }
    }

success:
    bytes_read = snd_pcm_plugin_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
    final_return_code = 0;
    /*
     * there are return codes to these close calls, but we would do the same
     * thing regardless of error or success so we do not check the return codes.
     */
fail5:
    snd_mixer_close(mixer_handle);
fail4:
    snd_pcm_close(pcm_handle);
fail3:
    free(sample_buffer);
    sample_buffer = NULL;
fail2:
    fclose(file);
fail1:


    while (!exit_application) {
        /*
         * Something went wrong so there is probably an error message and
         * we don't want to exit right away because we want the user to see
         * the message in the dialog.
         *
         * Using a negative timeout (-1) in the call to bps_get_event(...)
         * ensures that we don't busy wait by blocking until an event is
         * available.
         */
        bps_event_t *event = NULL;
        bps_get_event(&event, -1);

        if (event) {
            /*
             * If it is a NAVIGATOR_EXIT event then we are done so stop
             * processing events, clean up and exit
             */
            if (bps_event_get_domain(event) == navigator_get_domain()) {
                if (NAVIGATOR_EXIT == bps_event_get_code(event)) {
                    exit_application = 1;
                }
            }
        }
    }

    /*
     * Destroy the dialog, if it exists and cleanup screen resources.
     */
    destroy_dialog();
    cleanup_screen();
    bps_shutdown();

    return final_return_code;
}
