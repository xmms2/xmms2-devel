


#include <glib.h>

#include "effect.h"

struct xmms_effect_St {
	struct xmms_effect_St *next;
	void (*samplerate_change) (xmms_effect_t *, guint rate);
	void (*run) (xmms_effect_t *, gchar *buf, guint len);
	gpointer *privdata;
	guint rate;
};

void
xmms_effect_samplerate_set (xmms_effect_t *effects, guint rate)
{

	for (; effects; effects = effects->next) {
		if (rate != effects->rate) {
			effects->rate = rate;
			effects->samplerate_change (effects, rate);
		}
	}
}

void
xmms_effect_run (xmms_effect_t *effects, gchar *buf, guint len)
{
	for (; effects; effects = effects->next) {
		effects->run (effects, buf, len);
	}
}
