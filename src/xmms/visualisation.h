#ifndef __XMMS_VISUALISATION_H__
#define __XMMS_VISUALISATION_H__

/* this is in number of samples! */
#define FFT_BITS 10
#define FFT_LEN (1<<FFT_BITS)



typedef struct xmms_visualisation_St xmms_visualisation_t;

xmms_visualisation_t *xmms_visualisation_init ();
void xmms_visualisation_calc (xmms_visualisation_t *vis, gchar *buf, int len);
void xmms_visualisation_destroy (xmms_visualisation_t *vis);
void xmms_visualisation_samplerate_set (xmms_visualisation_t *vis, guint rate);

void xmms_visualisation_users_inc ();
void xmms_visualisation_users_dec ();


#endif
