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

const char *riff_id = "RIFF";
const char *wave_id = "WAVE";

#define COLOR_PURPLE 0xffff00ff
#define WAV_RELATIVE_PATH "/app/native/sample.wav"

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

int
err (char *msg)
{
    perror (msg);
    return -1;
}

int
find_tag (FILE * fp, const char *tag)
{
    int     ret_val = 0;
    riff_tag tag_bfr = { "", 0 };

    // Keep reading until we find the tag or hit the EOF.
    while (fread ((unsigned char *) &tag_bfr, sizeof (tag_bfr), 1, fp))
    {

        // If this is our tag, set the length and break.
        if (strncmp (tag, tag_bfr.tag, sizeof tag_bfr.tag) == 0)
        {
        	ret_val = ENDIAN_LE32 (tag_bfr.length);
            break;
        }

        // Skip ahead the specified number of bytes in the stream
        fseek (fp, tag_bfr.length, SEEK_CUR);
    }

    // Return the result of our operation
    return (ret_val);
}

int
check_hdr (FILE * fp)
{
    riff_hdr riff_header = { "", 0 };

    // Read the header and, if successful, play the file
    // file or WAVE file.
    if (fread ((unsigned char *) &riff_header, sizeof (riff_hdr), 1, fp) == 0)
        return 0;

    if (strncmp (riff_header.Riff, riff_id, strlen (riff_id)) ||
        strncmp (riff_header.Wave, wave_id, strlen (wave_id)))
        return -1;

    return 0;
}

int
main (int argc, char **argv)
{
    int     card = -1;
    int     dev = 0;
    snd_pcm_t *pcm_handle;
    FILE   *file;
    wave_hdr wav_header;
    int     samples;
    int     sample_rate;
    int     sample_channels;
    int     sample_bits;
    char   *sample_buffer;
    int     fragsize = -1;
    int     verbose = 0;

    int     rtn;
    int     final_return_code = -1;
    snd_pcm_channel_info_t pi;
    snd_mixer_t *mixer_handle;
    snd_mixer_group_t group;
    snd_pcm_channel_params_t pp;
    snd_pcm_channel_setup_t setup;
    int     bsize, bytes_read, total_written = 0;
    fd_set  rfds, wfds;
    uint32_t voice_mask[] = { 0, 0, 0, 0 };
    snd_pcm_voice_conversion_t voice_conversion;
    int     voice_override = 0;
    int     num_frags = -1;
    char input_file[PATH_MAX];
    char cwd[PATH_MAX];
    screen_context_t screen_cxt;
    screen_window_t screen_win;
    screen_buffer_t screen_buf;
    int screen_fill_attribs[] = { SCREEN_BLIT_COLOR, COLOR_PURPLE, SCREEN_BLIT_END };
    int screen_dirty[4] = { 0, 0, 1024, 600 }; //start with sane default values

    int idle_mode = SCREEN_IDLE_MODE_KEEP_AWAKE;
    int usage = SCREEN_USAGE_NATIVE;

    if (screen_create_context(&screen_cxt, 0) != 0)
    {
    	return err("failed to create context");
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

    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_IDLE_MODE, &idle_mode) != 0)
    {
    	err("failed to set idle mode");
    	goto fail3;
    }

    if (screen_post_window(screen_win, screen_buf, 1, screen_dirty, 0) != 0) {
    	err("failed to post the window");
    	goto fail3;
    }

    if ((rtn = snd_pcm_open_preferred (&pcm_handle, &card, &dev, SND_PCM_OPEN_PLAYBACK)) < 0)
    {
        err ("device open");
        goto fail3;
    }

    getcwd(cwd, PATH_MAX);
    rtn = snprintf(input_file, PATH_MAX, "%s%s", cwd, WAV_RELATIVE_PATH);
    if (rtn > PATH_MAX - 1)
    {
    	err ("File name and path too long");
    	goto fail4;
    }

    if ((file = fopen (input_file, "r")) == 0)
    {
    	err ("File open failed");
    	goto fail4;
    }

    if (check_hdr (file) == -1) {
        err ("check_hdr failed");
        goto fail5;
    }

    samples = find_tag (file, "fmt ");
    fread (&wav_header, sizeof (wav_header), 1, file);
    fseek (file, (samples - sizeof (wave_hdr)), SEEK_CUR);

    sample_rate = ENDIAN_LE32 (wav_header.samples_per_sec);
    sample_channels = ENDIAN_LE16 (wav_header.channels);
    sample_bits = ENDIAN_LE16 (wav_header.bits_per_sample);

    printf ("SampleRate = %d, channels = %d, SampleBits = %d\n", sample_rate, sample_channels,
        sample_bits);

    /* disabling mmap is not actually required in this example but it is included to
     * demonstrate how it is used when it is required.
     */
    if ((rtn = snd_pcm_plugin_set_disable (pcm_handle, PLUGIN_DISABLE_MMAP)) < 0)
    {
        fprintf (stderr, "snd_pcm_plugin_set_disable failed: %s\n", snd_strerror (rtn));
        goto fail5;
    }

    memset (&pi, 0, sizeof (pi));
    pi.channel = SND_PCM_CHANNEL_PLAYBACK;
    if ((rtn = snd_pcm_plugin_info (pcm_handle, &pi)) < 0)
    {
        fprintf (stderr, "snd_pcm_plugin_info failed: %s\n", snd_strerror (rtn));
        goto fail5;
    }

    memset (&pp, 0, sizeof (pp));

    pp.mode = SND_PCM_MODE_BLOCK;
    pp.channel = SND_PCM_CHANNEL_PLAYBACK;
    pp.start_mode = SND_PCM_START_FULL;
    pp.stop_mode = SND_PCM_STOP_STOP;
    pp.buf.block.frag_size = pi.max_fragment_size;

    if (fragsize != -1)
    {
        pp.buf.block.frag_size = fragsize;
    }
    pp.buf.block.frags_max = num_frags;
    pp.buf.block.frags_min = 1;

    pp.format.interleave = 1;
    pp.format.rate = sample_rate;
    pp.format.voices = sample_channels;

    if (ENDIAN_LE16 (wav_header.format_tag) == 6)
        pp.format.format = SND_PCM_SFMT_A_LAW;
    else if (ENDIAN_LE16 (wav_header.format_tag) == 7)
        pp.format.format = SND_PCM_SFMT_MU_LAW;
    else if (sample_bits == 8)
        pp.format.format = SND_PCM_SFMT_U8;
    else if (sample_bits == 24)
        pp.format.format = SND_PCM_SFMT_S24;
    else
        pp.format.format = SND_PCM_SFMT_S16_LE;

    strcpy (pp.sw_mixer_subchn_name, "Wave playback channel");
    if ((rtn = snd_pcm_plugin_params (pcm_handle, &pp)) < 0)
    {
        fprintf (stderr, "snd_pcm_plugin_params failed: %s\n", snd_strerror (rtn));
        goto fail5;
    }

    if ((rtn = snd_pcm_plugin_prepare (pcm_handle, SND_PCM_CHANNEL_PLAYBACK)) < 0) {
        fprintf (stderr, "snd_pcm_plugin_prepare failed: %s\n", snd_strerror (rtn));
        goto fail5;
    }

    if (voice_override)
    {
        snd_pcm_plugin_get_voice_conversion (pcm_handle, SND_PCM_CHANNEL_PLAYBACK,
            &voice_conversion);
        voice_conversion.matrix[0] = voice_mask[0];
        voice_conversion.matrix[1] = voice_mask[1];
        voice_conversion.matrix[2] = voice_mask[2];
        voice_conversion.matrix[3] = voice_mask[3];
        snd_pcm_plugin_set_voice_conversion (pcm_handle, SND_PCM_CHANNEL_PLAYBACK,
            &voice_conversion);
    }

    memset (&setup, 0, sizeof (setup));
    memset (&group, 0, sizeof (group));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    setup.mixer_gid = &group.gid;

    if ((rtn = snd_pcm_plugin_setup (pcm_handle, &setup)) < 0)
    {
        fprintf (stderr, "snd_pcm_plugin_setup failed: %s\n", snd_strerror (rtn));
        goto fail5;
    }

    printf ("Format %s \n", snd_pcm_get_format_name (setup.format.format));
    printf ("Frag Size %d \n", setup.buf.block.frag_size);
    printf ("Total Frags %d \n", setup.buf.block.frags);
    printf ("Rate %d \n", setup.format.rate);
    printf ("Voices %d \n", setup.format.voices);
    bsize = setup.buf.block.frag_size;

    if (group.gid.name[0] == 0)
    {
        fprintf (stderr, "Mixer Pcm Group [%s] Not Set \n", group.gid.name);
        goto fail5;
    }

    printf ("Mixer Pcm Group [%s]\n", group.gid.name);
    if ((rtn = snd_mixer_open (&mixer_handle, card, setup.mixer_device)) < 0)
    {
        fprintf (stderr, "snd_mixer_open failed: %s\n", snd_strerror (rtn));
        goto fail5;
    }

    samples = find_tag (file, "data");
    sample_buffer = malloc (bsize);
    FD_ZERO (&rfds);
    FD_ZERO (&wfds);
    bytes_read = 1;
    while (total_written < samples && bytes_read > 0)
    {
        if (tcgetpgrp (0) == getpid ())
            FD_SET (STDIN_FILENO, &rfds);
        FD_SET (snd_mixer_file_descriptor (mixer_handle), &rfds);
        FD_SET (snd_pcm_file_descriptor (pcm_handle, SND_PCM_CHANNEL_PLAYBACK), &wfds);

        rtn = max (snd_mixer_file_descriptor (mixer_handle),
            snd_pcm_file_descriptor (pcm_handle, SND_PCM_CHANNEL_PLAYBACK));

        if (select (rtn + 1, &rfds, &wfds, NULL, NULL) == -1)
        {
        	err ("select");
        	goto fail6;
        }

        if (FD_ISSET (snd_pcm_file_descriptor (pcm_handle, SND_PCM_CHANNEL_PLAYBACK), &wfds))
        {
            snd_pcm_channel_status_t status;
            int     written = 0;

            if ((bytes_read = fread (sample_buffer, 1, min (samples - total_written, bsize), file)) <= 0)
                continue;
            written = snd_pcm_plugin_write (pcm_handle, sample_buffer, bytes_read);
            if (verbose)
                printf ("bytes written = %d \n", written);

            if (written < bytes_read)
            {
                memset (&status, 0, sizeof (status));
                status.channel = SND_PCM_CHANNEL_PLAYBACK;
                if (snd_pcm_plugin_status (pcm_handle, &status) < 0)
                {
                    fprintf (stderr, "underrun: playback channel status error\n");
                    goto fail6;
                }

                if (status.status == SND_PCM_STATUS_READY ||
                    status.status == SND_PCM_STATUS_UNDERRUN)
                {
                    if (snd_pcm_plugin_prepare (pcm_handle, SND_PCM_CHANNEL_PLAYBACK) < 0)
                    {
                        fprintf (stderr, "underrun: playback channel prepare error\n");
                        goto fail6;
                    }
                }
                if (written < 0)
                    written = 0;
                written += snd_pcm_plugin_write (pcm_handle, sample_buffer + written, bytes_read - written);
            }
            total_written += written;
        }
    }

    bytes_read = snd_pcm_plugin_flush (pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
    final_return_code = 0;

fail6:
    rtn = snd_mixer_close (mixer_handle);
fail5:
    fclose (file);
fail4:
    rtn = snd_pcm_close (pcm_handle);
fail3:
    screen_destroy_buffer(screen_buf);
fail2:
    screen_destroy_window(screen_win);
fail1:
    screen_destroy_context(screen_cxt);
    return final_return_code;
}
