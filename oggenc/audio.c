/* OggEnc
 **
 ** This program is distributed under the GNU General Public License, version 2.
 ** A copy of this license is included with this source.
 **
 ** Copyright 2000, Michael Smith <msmith@labyrinth.net.au>
 **
 ** AIFF/AIFC support from OggSquish, (c) 1994-1996 Monty <xiphmont@xiph.org>
 **/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include "audio.h"
#include "platform.h"
#include "i18n.h"

#define WAV_HEADER_SIZE 44

/* Macros to read header data */
#define READ_U32_LE(buf) \
	(((buf)[3]<<24)|((buf)[2]<<16)|((buf)[1]<<8)|((buf)[0]&0xff))

#define READ_U16_LE(buf) \
	(((buf)[1]<<8)|((buf)[0]&0xff))

#define READ_U32_BE(buf) \
	(((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|((buf)[3]&0xff))

#define READ_U16_BE(buf) \
	(((buf)[0]<<8)|((buf)[1]&0xff))

/* Define the supported formats here */
input_format formats[] = {
	{wav_id, 12, wav_open, wav_close, "wav", N_("WAV file reader")},
	{aiff_id, 12, aiff_open, wav_close, "aiff", N_("AIFF/AIFC file reader")},
	{NULL, 0, NULL, NULL, NULL, NULL}
};

input_format *open_audio_file(FILE *in, oe_enc_opt *opt)
{
	int j=0;
	unsigned char *buf=NULL;
	int buf_size=0, buf_filled=0;
	int size,ret;

	while(formats[j].id_func)
	{
		size = formats[j].id_data_len;
		if(size >= buf_size)
		{
			buf = realloc(buf, size);
			buf_size = size;
		}

		if(buf_size > buf_filled)
		{
			ret = fread(buf+buf_filled, 1, buf_size-buf_filled, in);
			buf_filled += ret;

			if(buf_filled != buf_size)
			{ /* File truncated */
				buf_size = buf_filled;
				j++;
				continue;
			}
		}

		if(formats[j].id_func(buf, size))
		{
			/* ok, we now have something that can handle the file */
			if(formats[j].open_func(in, opt, buf, size))
				return &formats[j];
		}
		j++;
	}

	return NULL;
}

static int seek_forward(FILE *in, int length)
{
	if(fseek(in, length, SEEK_CUR))
	{
		/* Failed. Do it the hard way. */
		unsigned char buf[1024];
		int seek_needed = length, seeked;
		while(seek_needed > 0)
		{
			seeked = fread(buf, 1, seek_needed>1024?1024:seek_needed, in);
			if(!seeked)
				return 0; /* Couldn't read more, can't read file */
			else
				seek_needed -= seeked;
		}
	}
	return 1;
}


static int find_wav_chunk(FILE *in, char *type, unsigned int *len)
{
	unsigned char buf[8];

	while(1)
	{
		if(fread(buf,1,8,in) < 8) /* Suck down a chunk specifier */
		{
			fprintf(stderr, _("Warning: Unexpected EOF in reading WAV header\n"));
			return 0; /* EOF before reaching the appropriate chunk */
		}

		if(memcmp(buf, type, 4))
		{
			*len = READ_U32_LE(buf+4);
			if(!seek_forward(in, *len))
				return 0;

			buf[4] = 0;
			fprintf(stderr, _("Skipping chunk of type \"%s\", length %d\n"), buf, *len);
		}
		else
		{
			*len = READ_U32_LE(buf+4);
			return 1;
		}
	}
}

static int find_aiff_chunk(FILE *in, char *type, unsigned int *len)
{
	unsigned char buf[8];

	while(1)
	{
		if(fread(buf,1,8,in) <8)
		{
			fprintf(stderr, _("Warning: Unexpected EOF in AIFF chunk\n"));
			return 0;
		}

		*len = READ_U32_BE(buf+4);

		if(memcmp(buf,type,4))
		{
			if((*len) & 0x1)
				(*len)++;

			if(!seek_forward(in, *len))
				return 0;
		}
		else
			return 1;
	}
}



double read_IEEE80(unsigned char *buf)
{
	int s=buf[0]&0xff;
	int e=((buf[0]&0x7f)<<8)|(buf[1]&0xff);
	double f=((unsigned long)(buf[2]&0xff)<<24)|
		((buf[3]&0xff)<<16)|
		((buf[4]&0xff)<<8) |
		 (buf[5]&0xff);

	if(e==32767)
	{
		if(buf[2]&0x80)
			return HUGE_VAL; /* Really NaN, but this won't happen in reality */
		else
		{
			if(s)
				return -HUGE_VAL;
			else
				return HUGE_VAL;
		}
	}

	f=ldexp(f,32);
	f+= ((buf[6]&0xff)<<24)|
		((buf[7]&0xff)<<16)|
		((buf[8]&0xff)<<8) |
		 (buf[9]&0xff);

	return ldexp(f, e-16446);
}

/* AIFF/AIFC support adapted from the old OggSQUISH application */
int aiff_id(unsigned char *buf, int len)
{
	if(len<12) return 0; /* Truncated file, probably */

	if(memcmp(buf, "FORM", 4))
		return 0;

	if(memcmp(buf+8, "AIF",3))
		return 0;

	if(buf[11]!='C' && buf[11]!='F')
		return 0;

	return 1;
}

int aiff_open(FILE *in, oe_enc_opt *opt, unsigned char *buf, int buflen)
{
	int aifc; /* AIFC or AIFF? */
	unsigned int len;
	unsigned char *buffer;
	unsigned char buf2[8];
	aiff_fmt format;
	aifffile *aiff = malloc(sizeof(aifffile));

	if(buf[11]=='C')
		aifc=1;
	else
		aifc=0;

	if(!find_aiff_chunk(in, "COMM", &len))
	{
		fprintf(stderr, _("Warning: No common chunk found in AIFF file\n"));
		return 0; /* EOF before COMM chunk */
	}

	if(len < 18) 
	{
		fprintf(stderr, _("Warning: Truncated common chunk in AIFF header\n"));
		return 0; /* Weird common chunk */
	}

	buffer = alloca(len);

	if(fread(buffer,1,len,in) < len)
	{
		fprintf(stderr, _("Warning: Unexpected EOF in reading AIFF header\n"));
		return 0;
	}

	format.channels = READ_U16_BE(buffer);
	format.totalframes = READ_U32_BE(buffer+2);
	format.samplesize = READ_U16_BE(buffer+6);
	format.rate = (int)read_IEEE80(buffer+8);

	if(aifc)
	{
		if(len < 22)
		{
			fprintf(stderr, _("Warning: AIFF-C header truncated.\n"));
			return 0;
		}
		else if(memcmp(buffer+18, "NONE", 4)) 
		{
			fprintf(stderr, _("Warning: Can't handle compressed AIFF-C\n"));
			return 0; /* Compressed. Can't handle */
		}
	}

	if(!find_aiff_chunk(in, "SSND", &len))
	{
		fprintf(stderr, _("Warning: No SSND chunk found in AIFF file\n"));
		return 0; /* No SSND chunk -> no actual audio */
	}

	if(len < 8) 
	{
		fprintf(stderr, _("Warning: Corrupted SSND chunk in AIFF header\n"));
		return 0; 
	}

	if(fread(buf2,1,8, in) < 8)
	{
		fprintf(stderr, _("Warning: Unexpected EOF reading AIFF header\n"));
		return 0;
	}

	format.offset = READ_U32_BE(buf2);
	format.blocksize = READ_U32_BE(buf2+4);

	if( format.blocksize == 0 &&
		(format.samplesize == 16 || format.samplesize == 8))
	{
		/* From here on, this is very similar to the wav code. Oh well. */
		
		if(format.rate != 44100 && format.rate != 48000)
			fprintf(stderr, _("Warning: Vorbis is not currently tuned for this input (%.3f kHz).\n"
				" At other than 44.1/48 kHz quality will be degraded.\n"),
				(float)format.rate * 1.0e-3);

		opt->rate = format.rate;
		opt->channels = format.channels;
		opt->read_samples = wav_read; /* Similar enough, so we use the same */
		opt->total_samples_per_channel = format.totalframes;

		aiff->f = in;
		aiff->samplesread = 0;
		aiff->channels = format.channels;
		aiff->samplesize = format.samplesize;
		aiff->totalsamples = format.totalframes;
		aiff->bigendian = 1;

		opt->readdata = (void *)aiff;

		seek_forward(in, format.offset); /* Swallow some data */
		return 1;
	}
	else
	{
		fprintf(stderr, 
				_("Warning: OggEnc does not support this type of AIFF/AIFC file\n"
				" Must be 8 or 16 bit PCM.\n"));
		return 0;
	}
}


int wav_id(unsigned char *buf, int len)
{
	unsigned int flen;
	
	if(len<12) return 0; /* Something screwed up */

	if(memcmp(buf, "RIFF", 4))
		return 0; /* Not wave */

	flen = READ_U32_LE(buf+4); /* We don't use this */

	if(memcmp(buf+8, "WAVE",4))
		return 0; /* RIFF, but not wave */

	return 1;
}

int wav_open(FILE *in, oe_enc_opt *opt, unsigned char *oldbuf, int buflen)
{
	unsigned char buf[16];
	unsigned int len;
	int samplesize;
	wav_fmt format;
	wavfile *wav = malloc(sizeof(wavfile));

	/* Ok. At this point, we know we have a WAV file. Now we have to detect
	 * whether we support the subtype, and we have to find the actual data
	 * We don't (for the wav reader) need to use the buffer we used to id this
	 * as a wav file (oldbuf)
	 */

	if(!find_wav_chunk(in, "fmt ", &len))
		return 0; /* EOF */

	if(len < 16) 
	{
		fprintf(stderr, _("Warning: Unrecognised format chunk in WAV header\n"));
		return 0; /* Weird format chunk */
	}

	/* A common error is to have a format chunk that is not 16 or 18 bytes
	 * in size.  This is incorrect, but not fatal, so we only warn about 
	 * it instead of refusing to work with the file.  Please, if you
	 * have a program that's creating format chunks of sizes other than
	 * 16 or 18 bytes in size, report a bug to the author.
	 */
	if(len!=16 && len!=18)
		fprintf(stderr, 
				_("Warning: INVALID format chunk in wav header.\n"
				" Trying to read anyway (may not work)...\n"));

	if(fread(buf,1,16,in) < 16)
	{
		fprintf(stderr, _("Warning: Unexpected EOF in reading WAV header\n"));
		return 0;
	}

	/* Deal with stupid broken apps. Don't use these programs.
	 */
	if(len - 16 > 0 && !seek_forward(in, len-16))
	    return 0;

	format.format =      READ_U16_LE(buf); 
	format.channels =    READ_U16_LE(buf+2); 
	format.samplerate =  READ_U32_LE(buf+4);
	format.bytespersec = READ_U32_LE(buf+8);
	format.align =       READ_U16_LE(buf+12);
	format.samplesize =  READ_U16_LE(buf+14);

	if(!find_wav_chunk(in, "data", &len))
		return 0; /* EOF */

	if(format.format == 1)
	{
		samplesize = format.samplesize/8;
		opt->read_samples = wav_read;
	}
	else if(format.format == 3)
	{
		samplesize = 4;
		opt->read_samples = wav_ieee_read;
	}
	else
	{
		fprintf(stderr, 
				_("ERROR: Wav file is unsupported type (must be standard PCM\n"
				" or type 3 floating point PCM\n"));
		return 0;
	}



	if( format.align == format.channels*samplesize &&
			format.samplesize == samplesize*8)
	{
		if(format.samplerate != 44100 && format.samplerate != 48000)
			fprintf(stderr, _("Warning: Vorbis is not currently tuned for this input (%.3f kHz).\n"
					" At other than 44.1/48 kHz quality will be degraded.\n"),
					(float)format.samplerate * 1.0e-3);

		/* OK, good - we have the one supported format,
		   now we want to find the size of the file */
		opt->rate = format.samplerate;
		opt->channels = format.channels;

		wav->f = in;
		wav->samplesread = 0;
		wav->bigendian = 0;
		wav->channels = format.channels; /* This is in several places. The price
											of trying to abstract stuff. */
		wav->samplesize = format.samplesize;

		if(len)
        {
			opt->total_samples_per_channel = len/(format.channels*samplesize);
		}
		else
		{
			long pos;
			pos = ftell(in);
			if(fseek(in, 0, SEEK_END) == -1)
			{
				opt->total_samples_per_channel = 0; /* Give up */
			}
			else
			{
				opt->total_samples_per_channel = (ftell(in) - pos)/
                    (format.channels*samplesize);
				fseek(in,pos, SEEK_SET);
			}
		}
		wav->totalsamples = opt->total_samples_per_channel;

		opt->readdata = (void *)wav;
		return 1;
	}
	else
	{
		fprintf(stderr, 
				_("ERROR: Wav file is unsupported subformat (must be 16 bit PCM\n"
				"or floating point PCM\n"));
		return 0;
	}
}

long wav_read(void *in, float **buffer, int samples)
{
	wavfile *f = (wavfile *)in;
	int sampbyte = f->samplesize / 8;
	signed char *buf = alloca(samples*sampbyte*f->channels);
	long bytes_read = fread(buf, 1, samples*sampbyte*f->channels, f->f);
	int i,j;
	long realsamples;

	if(f->totalsamples && f->samplesread + 
			bytes_read/(sampbyte*f->channels) > f->totalsamples) 
		bytes_read = sampbyte*f->channels*(f->totalsamples - f->samplesread);

	realsamples = bytes_read/(sampbyte*f->channels);
	f->samplesread += realsamples;
		
	if(f->samplesize==8)
	{
		unsigned char *bufu = (unsigned char *)buf;
		for(i = 0; i < realsamples; i++)
		{
			for(j=0; j < f->channels; j++)
			{
				buffer[j][i]=((int)(bufu[i*f->channels + j])-128)/128.0f;
			}
		}
	}
	else
	{
		if(!f->bigendian)
		{
			for(i = 0; i < realsamples; i++)
			{
				for(j=0; j < f->channels; j++)
				{
					buffer[j][i] = ((buf[i*2*f->channels + 2*j + 1]<<8) |
							        (buf[i*2*f->channels + 2*j] & 0xff))/32768.0f;
				}
			}
		}
		else
		{
			for(i = 0; i < realsamples; i++)
			{
				for(j=0; j < f->channels; j++)
				{
					buffer[j][i]=((buf[i*2*f->channels + 2*j]<<8) |
							      (buf[i*2*f->channels + 2*j + 1] & 0xff))/32768.0f;
				}
			}
		}
	}

	return realsamples;
}

long wav_ieee_read(void *in, float **buffer, int samples)
{
	wavfile *f = (wavfile *)in;
	float *buf = alloca(samples*4*f->channels); /* de-interleave buffer */
	long bytes_read = fread(buf,1,samples*4*f->channels, f->f);
	int i,j;
	long realsamples;


	if(f->totalsamples && f->samplesread +
			bytes_read/(4*f->channels) > f->totalsamples)
		bytes_read = 4*f->channels*(f->totalsamples - f->samplesread);
	realsamples = bytes_read/(4*f->channels);
	f->samplesread += realsamples;

	for(i=0; i < realsamples; i++)
		for(j=0; j < f->channels; j++)
			buffer[j][i] = buf[i*f->channels + j];

	return realsamples;
}


void wav_close(void *info)
{
	wavfile *f = (wavfile *)info;

	free(f);
}

int raw_open(FILE *in, oe_enc_opt *opt)
{
	wav_fmt format; /* fake wave header ;) */
	wavfile *wav = malloc(sizeof(wavfile));

	if(opt->rate != 44100 && opt->rate != 48000)
		fprintf(stderr, _("Warning: Vorbis is not currently tuned for this input (%.3f kHz).\n"
				" At other than 44.1/48 kHz quality will be significantly degraded.\n"),
				(float)opt->rate * 1.0e-3);

	/* construct fake wav header ;) */
	format.format =      2; 
	format.channels =    opt->channels;
	format.samplerate =  opt->rate;
	format.samplesize =  opt->samplesize;
	format.bytespersec = opt->channels * opt->rate * opt->samplesize / 8;
	format.align =       format.bytespersec;
	wav->f =             in;
	wav->samplesread =   0;
	wav->bigendian =     0;
	wav->channels =      format.channels;
	wav->samplesize =    opt->samplesize;
    wav->totalsamples =  0;

	opt->read_samples = wav_read;
	opt->readdata = (void *)wav;
	opt->total_samples_per_channel = 0; /* raw mode, don't bother */
	return 1;
}
