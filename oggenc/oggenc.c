/* OggEnc
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2000, Michael Smith <msmith@labyrinth.net.au>
 *
 * Portions from Vorbize, (c) Kenneth Arnold <kcarnold@yahoo.com>
 * and libvorbis examples, (c) Monty <monty@xiph.org>
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <time.h>

#include "platform.h"
#include "encode.h"
#include "audio.h"
#include "utf8.h"

#define VERSION_STRING "OggEnc v0.8 (libvorbis rc2)\n"
#define COPYRIGHT "(c) 2001 Michael Smith <msmith@labyrinth.net.au)\n"

#define CHUNK 4096 /* We do reads, etc. in multiples of this */

struct option long_options[] = {
	{"quiet",0,0,'Q'},
	{"help",0,0,'h'},
	{"comment",1,0,'c'},
	{"artist",1,0,'a'},
	{"album",1,0,'l'},
	{"title",1,0,'t'},
	{"names",1,0,'n'},
	{"output",1,0,'o'},
	{"version",0,0,'v'},
	{"raw",0,0,'r'},
	{"raw-bits",1,0,'B'},
	{"raw-chan",1,0,'C'},
	{"raw-rate",1,0,'R'},
	{"bitrate",1,0,'b'},
	{"min-bitrate",1,0,'m'},
	{"max-bitrate",1,0,'M'},
	{"quality",1,0,'q'},
	{"date",1,0,'d'},
	{"tracknum",1,0,'N'},
	{"serial",1,0,'s'},
	{"encoding",1,0,'e'},
	{NULL,0,0,0}
};
	
char *generate_name_string(char *format, char *artist, char *title, char *album, char *track, char *date);
void parse_options(int argc, char **argv, oe_options *opt);
void build_comments(vorbis_comment *vc, oe_options *opt, int filenum, 
		char **artist, char **album, char **title, char **tracknum, char **date);
void usage(void);

int main(int argc, char **argv)
{
	/* Default values */
	oe_options opt = {"ISO-8859-1", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 
		0, NULL, 0, 0, 0,16,44100,2, NULL,NULL, 1,1,1, 0.5f,0}; 
	int i;

	char **infiles;
	int numfiles;
	int errors=0;

	parse_options(argc, argv, &opt);

	if(optind >= argc)
	{
		fprintf(stderr, VERSION_STRING COPYRIGHT "\nERROR: No input files specified. Use -h for help.\n");
		return 1;
	}
	else
	{
		infiles = argv + optind;
		numfiles = argc - optind;
	}

	/* Now, do some checking for illegal argument combinations */

	for(i = 0; i < numfiles; i++)
	{
		if(!strcmp(infiles[i], "-") && numfiles > 1)
		{
			fprintf(stderr, "ERROR: Multiple files specified when using stdin\n");
			exit(1);
		}
	}

	if(numfiles > 1 && opt.outfile)
	{
		fprintf(stderr, "ERROR: Multiple input files with specified output filename: suggest using -n\n");
		exit(1);
	}

	if(opt.serial == 0)
	{
		/* We randomly pick a serial number. This is then incremented for each file */
		srand(time(NULL));
		opt.serial = rand();
	}

	for(i = 0; i < numfiles; i++)
	{
		/* Once through the loop for each file */

		oe_enc_opt      enc_opts;
		vorbis_comment  vc;
		char *out_fn = NULL;
		FILE *in, *out = NULL;
		int foundformat = 0;
		int closeout = 0, closein = 0;
		char *artist=NULL, *album=NULL, *title=NULL, *track=NULL, *date=NULL;
		input_format *format;

		/* Set various encoding defaults */

		enc_opts.serialno = opt.serial++;
		enc_opts.progress_update = update_statistics_full;
		enc_opts.end_encode = final_statistics;
		enc_opts.error = encode_error;
		
		/* OK, let's build the vorbis_comments structure */
		build_comments(&vc, &opt, i, &artist, &album, &title, &track, &date);

		if(!strcmp(infiles[i], "-"))
		{
			setbinmode(stdin);
			in = stdin;
			if(!opt.outfile)
			{
				setbinmode(stdout);
				out = stdout;
			}
		}
		else
		{
			in = fopen(infiles[i], "rb");

			if(in == NULL)
			{
				fprintf(stderr, "ERROR: Cannot open input file \"%s\"\n", infiles[i]);
				free(out_fn);
				errors++;
				continue;
			}

			closein = 1;
		}

		/* Now, we need to select an input audio format - we do this before opening
		   the output file so that we don't end up with a 0-byte file if the input
		   file can't be read */

		if(opt.rawmode)
		{
			enc_opts.rate=opt.raw_samplerate;
			enc_opts.channels=opt.raw_channels;
			enc_opts.samplesize=opt.raw_samplesize;
			raw_open(in, &enc_opts);
			foundformat=1;
		}
		else
		{
			format = open_audio_file(in, &enc_opts);
			if(format)
			{
				fprintf(stderr, "Opening with %s module: %s\n", 
						format->format, format->description);
				foundformat=1;
			}

		}

		if(!foundformat)
		{
			fprintf(stderr, "ERROR: Input file \"%s\" is not a supported format\n", infiles[i]);
			errors++;
			continue;
		}

		/* Ok. We can read the file - so now open the output file */

		if(opt.outfile && !strcmp(opt.outfile, "-"))
		{
			setbinmode(stdout);
			out = stdout;
		}
		else if(out == NULL)
		{
			if(opt.outfile)
			{
				out_fn = strdup(opt.outfile);
			}
			else if(opt.namefmt)
			{
				out_fn = generate_name_string(opt.namefmt, artist, title, album, track,date);
			}
			else if(opt.title)
			{
				out_fn = malloc(strlen(title) + 5);
				strcpy(out_fn, title);
				strcat(out_fn, ".ogg");
			}
			else
			{
				/* Create a filename from existing filename, replacing extension with .ogg */
				char *start, *end;

				start = infiles[i];
				end = strrchr(infiles[i], '.');
				end = end?end:(start + strlen(infiles[i])+1);
			
				out_fn = malloc(end - start + 5);
				strncpy(out_fn, start, end-start);
				out_fn[end-start] = 0;
				strcat(out_fn, ".ogg");
			}


			out = fopen(out_fn, "wb");
			if(out == NULL)
			{
				if(closein)
					fclose(in);
				fprintf(stderr, "ERROR: Cannot open output file \"%s\"\n", out_fn);
				errors++;
				free(out_fn);
				continue;
			}	
			closeout = 1;
		}

		/* Now, set the rest of the options */
		enc_opts.out = out;
		enc_opts.comments = &vc;
		enc_opts.filename = out_fn;
		enc_opts.bitrate = opt.nominal_bitrate; 
		enc_opts.min_bitrate = opt.min_bitrate;
		enc_opts.max_bitrate = opt.max_bitrate;

		if(!enc_opts.total_samples_per_channel)
			enc_opts.progress_update = update_statistics_notime;

		if(opt.quiet)
		{
			enc_opts.progress_update = update_statistics_null;
			enc_opts.end_encode = final_statistics_null;
		}

		if(oe_encode(&enc_opts))
			errors++;

		if(out_fn) free(out_fn);
		vorbis_comment_clear(&vc);
		if(!opt.rawmode) 
			format->close_func(enc_opts.readdata);

		if(closein)
			fclose(in);
		if(closeout)
			fclose(out);
	}/* Finished this file, loop around to next... */

	return errors?1:0;

}

void usage(void)
{
	fprintf(stdout, 
		VERSION_STRING
		COPYRIGHT
		"\n"
		"Usage: oggenc [options] input.wav [...]\n"
		"\n"
		"OPTIONS:\n"
		" General:\n"
		" -Q, --quiet          Produce no output to stderr\n"
		" -h, --help           Print this help text\n"
		" -r, --raw            Raw mode. Input files are read directly as PCM data\n"
		" -B, --raw-bits=n     Set bits/sample for raw input. Default is 16\n"
		" -C, --raw-chan=n     Set number of channels for raw input. Default is 2\n"
		" -R, --raw-rate=n     Set samples/sec for raw input. Default is 44100\n"
		" -b, --bitrate        Choose a nominal bitrate to encode at. Attempt\n"
		"                      to encode at a bitrate averaging this. Takes an\n"
		"                      argument in kbps.\n"
		" -m, --min-bitrate    Specify a minimum bitrate (in kbps). Useful for\n"
		"                      encoding for a fixed-size channel.\n"
		" -M, --max-bitrate    Specify a maximum bitrate in kbps. Usedful for\n"
		"                      streaming purposes.\n"
		" -q, --quality        Specify quality between 0 (low) and 10 (high),\n"
		"                      instead of specifying a particular bitrate.\n"
		"                      This is the normal mode of operation.\n"
		" -s, --serial         Specify a serial number for the stream. If encoding\n"
		"                      multiple files, this will be incremented for each\n"
		"                      stream after the first.\n"
		" -e, --encoding       Specify an encoding for the comments given (not\n"
		"                      supported on windows)\n"
		"\n"
		" Naming:\n"
		" -o, --output=fn      Write file to fn (only valid in single-file mode)\n"
		" -n, --names=string   Produce filenames as this string, with %%a, %%t, %%l,\n"
		"                      %%n, %%d replaces by artist, title, album, track number,\n"
		"                      and date, respectively (see below for specifying these).\n"
		"                      %%%% gives a literal %%.\n"
		" -c, --comment=c      Add the given string as an extra comment. This may be\n"
		"                      used multiple times.\n"
		" -d, --date           Date for track (usually date of performance)\n"
		" -N, --tracknum       Track number for this track\n"
		" -t, --title          Title for this track\n"
		" -l, --album          Name of album\n"
		" -a, --artist         Name of artist\n"
		"                      If multiple input files are given, then multiple\n"
		"                      instances of the previous five arguments will be used,\n"
		"                      in the order they are given. If fewer titles are\n"
		"                      specified than files, OggEnc will print a warning, and\n"
		"                      reuse the final one for the remaining files. If fewer\n"
		"                      track numbers are given, the remaining files will be\n"
		"                      unnumbered. For the others, the final tag will be reused\n"
		"                      for all others without warning (so you can specify a date\n"
		"                      once, for example, and have it used for all the files)\n"
		"\n"
		"INPUT FILES:\n"
		" OggEnc input files must currently be 16 or 8 bit PCM WAV, AIFF, or AIFF/C\n"
		" files. Files may be mono or stereo (or more channels) and sampling rates \n"
		" from 8 kHz up, but currently the encoder is only tuned for 44.1 and 48 kHz\n"
		" rates. Others will be accepted but quality will not be as good.\n"
		" Alternatively, the --raw option may be used to use a raw PCM data file, which\n"
		" must be 16bit stereo little-endian PCM ('headerless wav'), unless additional\n"
		" parameters for raw mode are specified.\n"
		" You can specify taking the file from stdin by using - as the input filename.\n"
		" In this mode, output is to stdout unless an outfile filename is specified\n"
		" with -o\n"
		"\n");
}

char *generate_name_string(char *format, 
		char *artist, char *title, char *album, char *track, char *date)
{
	char *buffer;
	char next;
	int len;
	char *string;
	int used=0;
	int buflen;

	buffer = calloc(CHUNK+1,1);
	buflen = CHUNK;

	while(*format && used < buflen)
	{
		next = *format++;

		if(next == '%')
		{
			switch(*format++)
			{
				case '%':
					*(buffer+(used++)) = '%';
					break;
				case 'a':
					string = artist?artist:"(none)";
					len = strlen(string);
					strncpy(buffer+used, string, buflen-used);
					used += len;
					break;
				case 'd':
					string = date?date:"(none)";
					len = strlen(string);
					strncpy(buffer+used, string, buflen-used);
					used += len;
					break;
				case 't':
					string = title?title:"(none)";
					len = strlen(string);
					strncpy(buffer+used, string, buflen-used);
					used += len;
					break;
				case 'l':
					string = album?album:"(none)";
					len = strlen(string);
					strncpy(buffer+used, string, buflen-used);
					used += len;
					break;
				case 'n':
					string = track?track:"(none)";
					len = strlen(string);
					strncpy(buffer+used, string, buflen-used);
					used += len;
					break;
				default:
					fprintf(stderr, "WARNING: Ignoring illegal escape character '%c' in name format\n", *(format - 1));
					break;
			}
		}
		else
			*(buffer + (used++)) = next;
	}

	return buffer;
}

void parse_options(int argc, char **argv, oe_options *opt)
{
	int ret;
	int option_index = 1;

	while((ret = getopt_long(argc, argv, "a:b:B:c:C:d:e:hl:m:M:n:N:o:qQ:rR:s:t:v", 
					long_options, &option_index)) != -1)
	{
		switch(ret)
		{
			case 0:
				fprintf(stderr, "Internal error parsing command line options\n");
				exit(1);
				break;
			case 'a':
				opt->artist = realloc(opt->artist, (++opt->artist_count)*sizeof(char *));
				opt->artist[opt->artist_count - 1] = strdup(optarg);
				break;
			case 'c':
				opt->comments = realloc(opt->comments, (++opt->comment_count)*sizeof(char *));
				opt->comments[opt->comment_count - 1] = strdup(optarg);
				break;
			case 'd':
				opt->dates = realloc(opt->dates, (++opt->date_count)*sizeof(char *));
				opt->dates[opt->date_count - 1] = strdup(optarg);
				break;
			case 'e':
				opt->encoding = strdup(optarg);
				break;
			case 'l':
				opt->album = realloc(opt->album, (++opt->album_count)*sizeof(char *));
				opt->album[opt->album_count - 1] = strdup(optarg);
				break;
			case 's':
				/* Would just use atoi(), but that doesn't deal with unsigned
				 * ints. Damn */
				if(sscanf(optarg, "%u", &opt->serial) != 1)
					opt->serial = 0; /* Failed, so just set to zero */
				break;
			case 't':
				opt->title = realloc(opt->title, (++opt->title_count)*sizeof(char *));
				opt->title[opt->title_count - 1] = strdup(optarg);
				break;
			case 'b':
				opt->nominal_bitrate = atoi(optarg);
				break;
			case 'm':
				opt->min_bitrate = atoi(optarg);
				break;
			case 'M':
				opt->max_bitrate = atoi(optarg);
				break;
			case 'q':
				opt->quality = (float)(atof(optarg) * 0.1);
				if(opt->quality > 1.0f)
				{
					opt->quality = 1.0f;
					fprintf(stderr, "WARNING: quality setting too high, setting to maximum quality.\n");
				}
				else if(opt->quality < 0.f)
				{
					opt->quality = 0.f;
					fprintf(stderr, "WARNING: negative quality specified, setting to minimum.\n");
				}
				break;
			case 'n':
				if(opt->namefmt)
				{
					fprintf(stderr, "WARNING: Multiple name formats specified, using final\n");
					free(opt->namefmt);
				}
				opt->namefmt = strdup(optarg);
				break;
			case 'o':
				if(opt->outfile)
				{
					fprintf(stderr, "WARNING: Multiple output files specified, suggest using -n\n");
					free(opt->outfile);
				}
				opt->outfile = strdup(optarg);
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case 'Q':
				opt->quiet = 1;
				break;
			case 'r':
				opt->rawmode = 1;
				break;
			case 'v':
				fprintf(stderr, VERSION_STRING);
				exit(0);
				break;
			case 'B':
				if (opt->rawmode != 1)
				{
					opt->rawmode = 1;
					fprintf(stderr, "WARNING: Raw bits/sample specified for non-raw data. Assuming input is raw.\n");
				}
				if(sscanf(optarg, "%u", &opt->raw_samplesize) != 1)
				{
					opt->raw_samplesize = 16; /* Failed, so just set to 16 */
					fprintf(stderr, "WARNING: Invalid bits/sample specified, assuming 16.\n");
				}
				if((opt->raw_samplesize != 8) && (opt->raw_samplesize != 16))
				{
					fprintf(stderr, "WARNING: Invalid bits/sample specified, assuming 16.\n");
				}
				break;
			case 'C':
				if (opt->rawmode != 1)
				{
					opt->rawmode = 1;
					fprintf(stderr, "WARNING: Raw channel count specified for non-raw data. Assuming input is raw.\n");
				}
				if(sscanf(optarg, "%u", &opt->raw_channels) != 1)
				{
					opt->raw_channels = 2; /* Failed, so just set to 2 */
					fprintf(stderr, "WARNING: Invalid channel count specified, assuming 2.\n");
				}
				break;
			case 'N':
				opt->tracknum = realloc(opt->tracknum, (++opt->track_count)*sizeof(char *));
				opt->tracknum[opt->track_count - 1] = strdup(optarg);
				break;
			case 'R':
				if (opt->rawmode != 1)
				{
					opt->rawmode = 1;
					fprintf(stderr, "WARNING: Raw samplerate specified for non-raw data. Assuming input is raw.\n");
				}
				if(sscanf(optarg, "%u", &opt->raw_samplerate) != 1)
				{
					opt->raw_samplerate = 44100; /* Failed, so just set to 44100 */
					fprintf(stderr, "WARNING: Invalid samplerate specified, assuming 44100.\n");
				}
				break;
			case '?':
				fprintf(stderr, "WARNING: Unknown option specified, ignoring->\n");
				break;
			default:
				usage();
				exit(0);
		}
	}
}

void add_tag(vorbis_comment *vc, oe_options *opt, char *name, char *value)
{
	char *utf8;
	if(utf8_encode(value, &utf8, opt->encoding) == 0)
	{
		if(name == NULL)
			vorbis_comment_add(vc, utf8);
		else
			vorbis_comment_add_tag(vc, name, utf8);
		free(utf8);
	}
	else
		fprintf(stderr, "Couldn't convert comment to UTF8, cannot add\n");
}

void build_comments(vorbis_comment *vc, oe_options *opt, int filenum, 
		char **artist, char **album, char **title, char **tracknum, char **date)
{
	int i;

	vorbis_comment_init(vc);

	for(i = 0; i < opt->comment_count; i++)
		add_tag(vc, opt, NULL, opt->comments[i]);

	if(opt->title_count)
	{
		if(filenum >= opt->title_count)
		{
			if(!opt->quiet)
				fprintf(stderr, "WARNING: Insufficient titles specified, defaulting to final title.\n");
			i = opt->title_count-1;
		}
		else
			i = filenum;

		*title = opt->title[i];
		add_tag(vc, opt, "title", opt->title[i]);
	}

	if(opt->artist_count)
	{
		if(filenum >= opt->artist_count)
			i = opt->artist_count-1;
		else
			i = filenum;
	
		*artist = opt->artist[i];
		add_tag(vc, opt, "artist", opt->artist[i]);
	}

	if(opt->date_count)
	{
		if(filenum >= opt->date_count)
			i = opt->date_count-1;
		else
			i = filenum;
	
		*date = opt->dates[i];
		add_tag(vc, opt, "date", opt->dates[i]);
	}
	
	if(opt->album_count)
	{
		if(filenum >= opt->album_count)
		{
			i = opt->album_count-1;
		}
		else
			i = filenum;

		*album = opt->album[i];	
		add_tag(vc, opt, "album", opt->album[i]);
	}

	if(filenum < opt->track_count)
	{
		i = filenum;
		*tracknum = opt->tracknum[i];
		add_tag(vc, opt, "tracknumber", opt->tracknum[i]);
	}
}


