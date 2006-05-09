/*
 * Simple wrapper around libsidplay allowing it to be used from C.
 */


#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>
#include <glib.h>


#define BEGIN_EXTERN extern "C" {
#define END_EXTERN }


struct sidplay_wrapper {
	sidplay2 *player;
	SidTune *currTune;
	sid2_config_t conf;
};

BEGIN_EXTERN

struct sidplay_wrapper *sidplay_wrapper_init () {
	struct sidplay_wrapper *res;
	res = g_new0 (struct sidplay_wrapper, 1);
	res->player = new sidplay2 ();
	return res;
}

void
sidplay_wrapper_destroy (struct sidplay_wrapper *wrap)
{
	wrap->player->stop ();
	sidbuilder *bldr = wrap->conf.sidEmulation;
	wrap->player->config (wrap->conf);
	delete bldr;
	delete wrap->player;
	delete wrap->currTune;
	g_free (wrap);
}


gint
sidplay_wrapper_play (struct sidplay_wrapper *wrap, void *buf, gint len)
{
	return wrap->player->play (buf, len);
}


gint
sidplay_wrapper_songinfo (struct sidplay_wrapper *wrap, gchar *artist,
                          gchar *title)
{
	gint err = 0;
	const SidTuneInfo *nfo = (wrap->player->info ()).tuneInfo;

	err = g_strlcpy (title, nfo->infoString[0], 32);
	if (err > 0) {
		err = g_strlcpy (artist, nfo->infoString[1], 32);
	}

	return err;
}


gint
sidplay_wrapper_subtunes (struct sidplay_wrapper *wrap)
{
	const SidTuneInfo *nfo = (wrap->player->info ()).tuneInfo;

	return nfo->songs;
}


void
sidplay_wrapper_set_subtune (struct sidplay_wrapper *wrap, gint subtune)
{
	wrap->currTune->selectSong (subtune);
	wrap->player->load (wrap->currTune);
}


gint
sidplay_wrapper_load (struct sidplay_wrapper *wrap, const void *buf, gint len)
{
	int res;

	wrap->currTune = new SidTune (0);
	res = wrap->currTune->read ((const uint_least8_t *)buf, len);
	if (!res) {
		return -2;
	}

	wrap->currTune->selectSong (1);

	res = wrap->player->load (wrap->currTune);
	if (res) {
		return -3;
	}

	ReSIDBuilder *rs = new ReSIDBuilder ("ReSID");
	if (!rs) {
		return -4;
	}

	if (!*rs) {
		return -5;
	}

	rs->create ((wrap->player->info ()).maxsids);
	if (!*rs) {
		return -6;
	}

	rs->filter (false);
	if (!*rs) {
		return -6;
	}

	rs->sampling (44100);
	if (!*rs) {
		return -6;
	}

	wrap->conf              = wrap->player->config ();
	wrap->conf.frequency    = 44100;
	wrap->conf.precision    = 16;
	wrap->conf.playback     = sid2_stereo;
	wrap->conf.sampleFormat = SID2_LITTLE_SIGNED;

	/* These should be configurable ... */
	wrap->conf.clockSpeed    = SID2_CLOCK_CORRECT;
	wrap->conf.clockForced   = true;
	wrap->conf.sidModel      = SID2_MODEL_CORRECT;
	wrap->conf.optimisation  = SID2_DEFAULT_OPTIMISATION;
	wrap->conf.sidSamples    = true;
	wrap->conf.clockDefault  = SID2_CLOCK_PAL;
	wrap->conf.sidDefault    = SID2_MOS6581;

	wrap->conf.sidEmulation = rs;
	res = wrap->player->config (wrap->conf);

	return res;
}

const char *
sidplay_wrapper_error (struct sidplay_wrapper *wrap)
{
	return wrap->player->error ();
}

END_EXTERN
