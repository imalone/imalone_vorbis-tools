#ifndef __ENCODE_H
#define __ENCODE_H

#include <stdio.h>
#include <vorbis/codec.h>


typedef void TIMER;
typedef long (*audio_read_func)(void *src, float **buffer, int samples);
typedef void (*progress_func)(char *fn, long totalsamples, 
		long samples, double time);
typedef void (*enc_end_func)(char *fn, double time, int rate, 
		long samples, long bytes);
typedef void (*error_func)(char *errormessage);


void *timer_start(void);
double timer_time(void *);
void timer_clear(void *);

void update_statistics_full(char *fn, long total, long done, double time);
void update_statistics_notime(char *fn, long total, long done, double time);
void update_statistics_null(char *fn, long total, long done, double time);
void final_statistics(char *fn, double time, int rate, long total_samples,
		long bytes);
void final_statistics_null(char *fn, double time, int rate, long total_samples,
		long bytes);
void encode_error(char *errmsg);

typedef struct
{
	char **title;
	int title_count;
	char **artist;
	int artist_count;
	char **album;
	int album_count;
	char **comments;
	int comment_count;
	char **tracknum;
	int track_count;
	char **dates;
	int date_count;

	int quiet;
	int rawmode;

	char *namefmt;
	char *outfile;
	int kbps;
} oe_options;

typedef struct
{
	vorbis_comment *comments;
	long serialno;

	audio_read_func read_samples;
	progress_func progress_update;
	enc_end_func end_encode;
	error_func error;
	
	void *readdata;

	long total_samples_per_channel;
	int channels;
	long rate;
	int bitrate;

	FILE *out;
	char *filename;
} oe_enc_opt;


int oe_encode(oe_enc_opt *opt);

#endif /* __ENCODE_H */
