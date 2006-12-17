/* remote.h by Richard van Paasen <rvpaasen@dds.nl> */

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

 last mod: $Id: remote.h,v 1.1 2002/11/20 00:03:34 rvpaasen Exp $

 ********************************************************************/

void remote_mainloop(void);
int remote_playloop(void);
void remote_time(double current, double total, long bitrate);
