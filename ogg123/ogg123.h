/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE IS GOVERNED BY *
 * THE GNU PUBLIC LICENSE 2, WHICH IS INCLUDED WITH THIS SOURCE.    *
 * PLEASE READ THESE TERMS BEFORE DISTRIBUTING.                     *
 *                                                                  *
 * THE Ogg123 SOURCE CODE IS (C) COPYRIGHT 2000-2001                *
 * by Kenneth C. Arnold <ogg@arnoldnet.net> AND OTHER CONTRIBUTORS  *
 * http://www.xiph.org/                                             *
 *                                                                  *
 ********************************************************************

 last mod: $Id: ogg123.h,v 1.12 2001/12/24 15:58:03 volsung Exp $

 ********************************************************************/

#ifndef __OGG123_H__
#define __OGG123_H__

#include <ogg/os_types.h>
#include "audio.h"

typedef struct ogg123_options_t {
  long int verbosity;         /* Verbose output if > 1, quiet if 0 */

  int shuffle;                /* Should we shuffle playing? */
  ogg_int64_t delay;          /* delay (in millisecs) for skip to next song */
  int nth;                    /* Play every nth chunk */
  int ntimes;                 /* Play every chunk n times */
  double seekpos;             /* Position in file to seek to */

  long buffer_size;           /* Size of audio buffer */
  float prebuffer;            /* Percent of buffer to fill before playing */
  long input_buffer_size;     /* Size of input audio buffer */
  float input_prebuffer;

  char *default_device;       /* Name of default driver to use */

  audio_device_t *devices;    /* Audio devices to use */

  double status_freq;         /* Number of status updates per second */
} ogg123_options_t;

typedef struct signal_request_t {
  int skipfile;
  int exit;
  int pause;
  ogg_int64_t last_ctrl_c;
} signal_request_t;

#endif /* __OGG123_H__ */
