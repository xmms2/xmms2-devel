#ifndef __PULSE_BACKEND_H__
#define __PULSE_BACKEND_H__

typedef struct xmms_pulse xmms_pulse;

xmms_pulse* xmms_pulse_backend_new(const char *server, const char *name,
                                   int *rerror);
void xmms_pulse_backend_free(xmms_pulse *s);
gboolean xmms_pulse_backend_set_stream(xmms_pulse *p,
                                       const char *stream_name,
                                       const char *sink,
                                       xmms_sample_format_t format,
                                       int samplerate, int channels,
                                       int *rerror);
void xmms_pulse_backend_close_stream(xmms_pulse *p);
gboolean xmms_pulse_backend_write(xmms_pulse *p, const char *data,
                                  size_t length, int *rerror);
gboolean xmms_pulse_backend_drain(xmms_pulse *p, int *rerror);
gboolean xmms_pulse_backend_flush(xmms_pulse *p, int *rerror);
int xmms_pulse_backend_get_latency(xmms_pulse *s, int *rerror);
int xmms_pulse_backend_volume_set(xmms_pulse *p, unsigned int vol);
int xmms_pulse_backend_volume_get(xmms_pulse *p, unsigned int *vol);

#endif
