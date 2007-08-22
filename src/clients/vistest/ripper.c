/********************************************************************

Derived from work with the following disclamer below:


Copyright (c) 2002-2004 Xiph.org Foundation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ********************************************************************/


/* Note that this is POSIX, not ANSI, code */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <vorbis/vorbisenc.h>

#ifdef _WIN32 /* We need the following two to set stdout to binary */
#include <io.h>
#include <fcntl.h>
#endif

#if defined(__MACOS__) && defined(__MWERKS__)
#include <console.h>			/* CodeWarrior's Mac "command-line" support */
#endif
#include <xmmsclient/xmmsclient.h>

#define x_exit(msg) \
	printf ("Error: %s\n", msg); \
	exit (EXIT_FAILURE);

#define READ 512
signed char readbuffer[READ*4]; /* out of the data segment, not the stack */

/* XMMS2 */
xmmsc_connection_t *x_connection;
int x_vis;
int intr = 0;

void
quit (int signum)
{
	intr = 1;
}

char* x_config[] = {
	"type", "pcm",
	"stereo", "1",
	"pcm.hardwire", "1",
	NULL
};

void
xmms2_quit ()
{
	xmmsc_visualization_shutdown (x_connection, x_vis);
	if (x_connection) {
		xmmsc_unref (x_connection);
	}
}

void xmms2_init ()
{
	xmmsc_result_t *res;
	char *path = getenv ("XMMS_PATH");
	x_connection = xmmsc_init ("xmms2-ripper");

	if (!x_connection || !xmmsc_connect (x_connection, path)){
		printf ("%s\n", xmmsc_get_last_error (x_connection));
		x_exit ("couldn't connect to xmms2d!");
	}

	x_vis = xmmsc_visualization_init (x_connection);
	res = xmmsc_visualization_properties_set (x_connection, x_vis, x_config);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		x_exit (xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	if (!xmmsc_visualization_start (x_connection, x_vis)) {
		printf ("%s\n", xmmsc_get_last_error (x_connection));
		x_exit ("Couldn't setup visualization transfer!");
	}

	atexit (xmms2_quit);
}

int main ()
{
	ogg_stream_state os; /* take physical pages, weld into a logical
													stream of packets */
	ogg_page         og; /* one Ogg bitstream page.	Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */

	vorbis_info      vi; /* struct that stores all the static vorbis bitstream
													settings */
	vorbis_comment   vc; /* struct that stores all the user comments */

	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */

	struct sigaction siga;
	int eos = 0, ret;
	char stat = '-';
	int stats;

#if defined(macintosh) && defined(__MWERKS__)
	int argc = 0;
	char **argv = NULL;
	argc = ccommand (&argv); /* get a "command line" from the Mac user */
#endif

#ifdef _WIN32 /* We need to set stdout to binary mode. Damn windows. */
	/* if we were writing a file, it would also need to in
		 binary mode, eg, fopen("file.wav","wb"); */
	_setmode (_fileno (stdout), _O_BINARY);
#endif

	siga.sa_handler = quit;
	sigemptyset (&siga.sa_mask);
	siga.sa_flags = SA_RESTART;

	/********** Init xmms2 **************/
	xmms2_init ();

	/********** Encode setup ************/

	vorbis_info_init (&vi);

	/* TODO: command line quality setting */
	ret = vorbis_encode_init_vbr (&vi, 2, 44100, 0.4);
	if (ret) {
		x_exit ("Setting up vorbis format failed!");
	}

	/* add a comment */
	vorbis_comment_init (&vc);
	vorbis_comment_add_tag (&vc, "ENCODER", "xmms2");

	/* set up the analysis state and auxiliary encoding storage */
	vorbis_analysis_init (&vd, &vi);
	vorbis_block_init (&vd, &vb);

	/* set up our packet->stream encoder */
	/* pick a random serial number; that way we can more likely build
		 chained streams just by concatenation */
	srand (time (NULL));
	ogg_stream_init (&os, rand ());

	/* Vorbis streams begin with three headers; the initial header (with
		 most of the codec setup parameters) which is mandated by the Ogg
		 bitstream spec.	The second header holds any comment fields.	The
		 third header holds the bitstream codebook.	We merely need to
		 make the headers, then pass them to libvorbis one at a time;
		 libvorbis handles the additional Ogg bitstream constraints */
	{
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout (&vd, &vc, &header, &header_comm, &header_code);
		ogg_stream_packetin (&os, &header); /* automatically placed in its own
                                             page */
		ogg_stream_packetin (&os, &header_comm);
		ogg_stream_packetin (&os, &header_code);

	/* This ensures the actual
	 * audio data will start on a new page, as per spec
	 */
		while (1) {
			int result = ogg_stream_flush (&os, &og);
			if (!result) {
				break;
			}
			fwrite (og.header, 1, og.header_len, stdout);
			fwrite (og.body, 1, og.body_len, stdout);
		}
	}

	sigaction(SIGINT, &siga, (struct sigaction*)NULL);

	fprintf (stderr, "Ready to encode...  ");
	stats = -1;
	while (!eos) {
		long i;
		long bytes = xmmsc_visualization_chunk_get (x_connection, x_vis, (short*)readbuffer, -1, 1000) * sizeof(short);
		if (bytes == 0) {
			continue;
		}
		if (bytes < 0 || intr) {
			/* 	 Tell the library we're at end of stream so that it can handle
				 the last frame and mark end of stream in the output properly */
			vorbis_analysis_wrote (&vd, 0);
			eos = 1;
		} else {
			if (stats == -1) {
				fprintf (stderr, "\rEncoding:  ");
			}
			/* data to encode */
			if (++stats == 10) {
				fprintf (stderr, "\b%c", stat);
				switch (stat) {
					case '-':  stat = '\\'; break;
					case '\\': stat = '|'; break;
					case '|':  stat = '/'; break;
					default:   stat = '-'; break;
				}
				stats = 0;
			}

			/* expose the buffer to submit data */
			float **buffer = vorbis_analysis_buffer (&vd, READ);

			/* uninterleave samples */
			for (i = 0; i < bytes/4; i++) {
				buffer[0][i] = ((readbuffer[i*4+1] << 8) |
					            (0x00ff & (int)readbuffer[i*4]))
				                /32768.f;
				buffer[1][i] = ((readbuffer[i*4+3] << 8) |
					            (0x00ff & (int)readbuffer[i*4+2]))
				                /32768.f;
			}

			/* tell the library how much we actually submitted */
			vorbis_analysis_wrote (&vd, i);
		}

		/* vorbis does some data preanalysis, then divvies up blocks for
			 more involved (potentially parallel) processing. Get a single
			 block for encoding now */
		while (vorbis_analysis_blockout (&vd, &vb) == 1) {

			/* analysis, assume we want to use bitrate management */
			vorbis_analysis (&vb, NULL);
			vorbis_bitrate_addblock (&vb);

			while (vorbis_bitrate_flushpacket (&vd, &op)) {

				/* weld the packet into the bitstream */
				ogg_stream_packetin (&os, &op);

				/* write out pages (if any) */
				while (!ogg_page_eos (&og)) {
					int result = ogg_stream_pageout (&os, &og);
					if (result == 0) {
						break;
					}
					fwrite (og.header, 1, og.header_len, stdout);
					fwrite (og.body, 1, og.body_len, stdout);
				}
			}
		}
	}

	/* clean up and exit.	vorbis_info_clear () must be called last */

	ogg_stream_clear (&os);
	vorbis_block_clear (&vb);
	vorbis_dsp_clear (&vd);
	vorbis_comment_clear (&vc);
	vorbis_info_clear (&vi);

	/* ogg_page and ogg_packet structs always point to storage in
		 libvorbis.	They're never freed or manipulated directly */

	fprintf (stderr, "\rDone.           \n");
	return (0);
}
