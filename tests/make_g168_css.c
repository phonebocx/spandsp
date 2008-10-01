/*
 * SpanDSP - a series of DSP components for telephony
 *
 * makecss.c - Create the composite source signal (CSS) for G.168 testing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: make_g168_css.c,v 1.15 2008/08/29 09:28:13 steveu Exp $
 */

/*! \page makecss_page CSS construction for G.168 testing
\section makecss_page_sec_1 What does it do?
???.

\section makecss_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <audiofile.h>
#if defined(HAVE_FFTW3_H)
#include <fftw3.h>
#else
#include <fftw.h>
#endif
#if defined(HAVE_MATH_H)
#define GEN_CONST
#endif

#include "spandsp.h"
#include "spandsp/g168models.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif

#define FAST_SAMPLE_RATE    44100.0

#define C1_VOICED_SAMPLES   2144    /* 48.62ms at 44100 samples/second => 2144.142 */
#define C1_NOISE_SAMPLES    8820    /* 200ms at 44100 samples/second => 8820.0 */
#define C1_SILENCE_SAMPLES  4471    /* 101.38ms at 44100 samples/second => 4470.858 */

#define C3_VOICED_SAMPLES   3206    /* 72.69ms at 44100 samples/second => 3205.629 */
#define C3_NOISE_SAMPLES    8820    /* 200ms at 44100 samples/second => 8820.0 */
#define C3_SILENCE_SAMPLES  5614    /* 127.31ms at 44100 samples/second => 5614.371 */

static double scaling(double f, double start, double end, double start_gain, double end_gain)
{
    double scale;

    scale = start_gain + (f - start)*(end_gain - start_gain)/(end - start);
    return scale;
}
/*- End of function --------------------------------------------------------*/

static double peak(const int16_t amp[], int len)
{
    int16_t peak;
    int i;

    peak = 0;
    for (i = 0;  i < len;  i++)
    {
        if (abs(amp[i]) > peak)
            peak = abs(amp[i]);
    }
    return peak/32767.0;
}
/*- End of function --------------------------------------------------------*/

static double rms(const int16_t amp[], int len)
{
    double ms;
    int i;

    ms = 0.0;
    for (i = 0;  i < len;  i++)
        ms += amp[i]*amp[i];
    return sqrt(ms/len)/32767.0;
}
/*- End of function --------------------------------------------------------*/

static double rms_to_dbm0(double rms)
{
    return 20.0*log10(rms) + DBM0_MAX_POWER;
}
/*- End of function --------------------------------------------------------*/

static double rms_to_db(double rms)
{
    return 20.0*log10(rms);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
#if defined(HAVE_FFTW3_H)
    double in[8192][2];
    double out[8192][2];
#else
    fftw_complex in[8192];
    fftw_complex out[8192];
#endif
    fftw_plan p;
    int16_t voiced_sound[8192];
    int16_t noise_sound[8830];
    int16_t silence_sound[8192];
    int i;
    int outframes;
    int voiced_length;
    double f;
    double pk;
    double ms;
    double scale;
    AFfilehandle filehandle;
    AFfilesetup filesetup;
    awgn_state_t noise_source;

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, FAST_SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    if ((filehandle = afOpenFile("sound_c1.wav", "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }

    printf("Generate C1\n");
    /* The set of C1 voice samples is ready for use in the output file. */
    voiced_length = sizeof(css_c1)/sizeof(css_c1[0]);
    for (i = 0;  i < voiced_length;  i++)
        voiced_sound[i] = css_c1[i];
    pk = peak(voiced_sound, voiced_length);
    ms = rms(voiced_sound, voiced_length);
    printf("Voiced level = %.2fdB, crest factor = %.2fdB\n", rms_to_dbm0(ms), rms_to_db(pk/ms));

#if defined(HAVE_FFTW3_H)
    p = fftw_plan_dft_1d(8192, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
#else
    p = fftw_create_plan(8192, FFTW_BACKWARD, FFTW_ESTIMATE);
#endif
    for (i = 0;  i < 8192;  i++)
    {
#if defined(HAVE_FFTW3_H)
        in[i][0] = 0.0;
        in[i][1] = 0.0;
#else
        in[i].re = 0.0;
        in[i].im = 0.0;
#endif
    }
    for (i = 1;  i <= 3715;  i++)
    {
        f = FAST_SAMPLE_RATE*i/8192.0;

#if 1
        if (f < 50.0)
            scale = -60.0;
        else if (f < 100.0)
            scale = scaling(f, 50.0, 100.0, -25.8, -12.8);
        else if (f < 200.0)
            scale = scaling(f, 100.0, 200.0, -12.8, 17.4);
        else if (f < 215.0)
            scale = scaling(f, 200.0, 215.0, 17.4, 17.8);
        else if (f < 500.0)
            scale = scaling(f, 215.0, 500.0, 17.8, 12.2);
        else if (f < 1000.0)
            scale = scaling(f, 500.0, 1000.0, 12.2, 7.2);
        else if (f < 2850.0)
            scale = scaling(f, 1000.0, 2850.0, 7.2, 0.0);
        else if (f < 3600.0)
            scale = scaling(f, 2850.0, 3600.0, 0.0, -2.0);
        else if (f < 3660.0)
            scale = scaling(f, 3600.0, 3660.0, -2.0, -20.0);
        else if (f < 3680.0)
            scale = scaling(f, 3600.0, 3680.0, -20.0, -30.0);
        else
            scale = -60.0;
#else
        scale = 0.0;
#endif
#if defined(HAVE_FFTW3_H)
        in[i][0] = ((rand() >> 10) & 0x1)  ?  1.0  :  -1.0;
        in[i][0] *= pow(10.0, scale/20.0)*35.0; //305360
        in[8192 - i][0] = -in[i][0];
#else
        in[i].re = ((rand() >> 10) & 0x1)  ?  1.0  :  -1.0;
        in[i].re *= pow(10.0, scale/20.0)*35.0; //305360
        in[8192 - i].re = -in[i].re;
#endif
    }
#if defined(HAVE_FFTW3_H)
    fftw_execute(p);
#else
    fftw_one(p, in, out);
#endif
    for (i = 0;  i < 8192;  i++)
    {
#if defined(HAVE_FFTW3_H)
        noise_sound[i] = out[i][1];
#else
        noise_sound[i] = out[i].im;
#endif
    }
    pk = peak(noise_sound, 8192);
    ms = rms(noise_sound, 8192);
    printf("Noise level = %.2fdB, crest factor = %.2fdB\n", rms_to_dbm0(ms), rms_to_db(pk/ms));
    
    for (i = 0;  i < 8192;  i++)
        silence_sound[i] = 0.0;

    for (i = 0;  i < 16;  i++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 16*voiced_length);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              C1_NOISE_SAMPLES - 8192);
    printf("%d samples of noise\n", C1_NOISE_SAMPLES);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              C1_SILENCE_SAMPLES);
    printf("%d samples of silence\n", C1_SILENCE_SAMPLES);

    /* Now phase invert the C1 set of voice samples. */
    voiced_length = sizeof(css_c1)/sizeof(css_c1[0]);
    for (i = 0;  i < voiced_length;  i++)
        voiced_sound[i] = -css_c1[i];
    pk = peak(voiced_sound, voiced_length);
    ms = rms(voiced_sound, voiced_length);
    printf("Voiced level = %.2fdB, crest factor = %.2fdB\n", rms_to_dbm0(ms), rms_to_db(pk/ms));

    for (i = 0;  i < 8192;  i++)
        noise_sound[i] = -noise_sound[i];

    for (i = 0;  i < 16;  i++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 16*voiced_length);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              C1_NOISE_SAMPLES - 8192);
    printf("%d samples of noise\n", C1_NOISE_SAMPLES);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              C1_SILENCE_SAMPLES);
    printf("%d samples of silence\n", C1_SILENCE_SAMPLES);

    if (afCloseFile(filehandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "sound_c1.wav");
        exit(2);
    }

    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, FAST_SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    filehandle = afOpenFile("sound_c3.wav", "w", filesetup);
    if (filehandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }

    printf("Generate C3\n");
    /* Take the supplied set of C3 voice samples. */
    voiced_length = (sizeof(css_c3)/sizeof(css_c3[0]));
    for (i = 0;  i < voiced_length;  i++)
        voiced_sound[i] = css_c3[i];
    pk = peak(voiced_sound, voiced_length);
    ms = rms(voiced_sound, voiced_length);
    printf("Voiced level = %.2fdB, crest factor = %.2fdB\n", rms_to_dbm0(ms), rms_to_db(pk/ms));

    awgn_init_dbm0(&noise_source, 7162534, rms_to_dbm0(ms));
    for (i = 0;  i < 8192;  i++)
        noise_sound[i] = awgn(&noise_source);
    pk = peak(noise_sound, 8192);
    ms = rms(noise_sound, 8192);
    printf("Noise level = %.2fdB, crest factor = %.2fdB\n", rms_to_dbm0(ms), rms_to_db(pk/ms));

    for (i = 0;  i < 14;  i++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 14*voiced_length);

    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              C3_NOISE_SAMPLES - 8192);
    printf("%d samples of noise\n", C3_NOISE_SAMPLES);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              C3_SILENCE_SAMPLES);
    printf("%d samples of silence\n", C3_SILENCE_SAMPLES);

    /* Now phase invert the set of voice samples. */
    voiced_length = (sizeof(css_c3)/sizeof(css_c3[0]));
    for (i = 0;  i < voiced_length;  i++)
        voiced_sound[i] = -css_c3[i];
    pk = peak(voiced_sound, voiced_length);
    ms = rms(voiced_sound, voiced_length);
    printf("Voiced level = %.2fdB, crest factor = %.2fdB\n", rms_to_dbm0(ms), rms_to_db(pk/ms));

    /* Now phase invert the set of noise samples. */
    for (i = 0;  i < 8192;  i++)
        noise_sound[i] = -noise_sound[i];

    for (i = 0;  i < 14;  i++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 14*i);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              C3_NOISE_SAMPLES - 8192);
    printf("%d samples of noise\n", C3_NOISE_SAMPLES);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              C3_SILENCE_SAMPLES);
    printf("%d samples of silence\n", C3_SILENCE_SAMPLES);

    if (afCloseFile(filehandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "sound_c3.wav");
        exit(2);
    }
    afFreeFileSetup(filesetup);

    fftw_destroy_plan(p);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/