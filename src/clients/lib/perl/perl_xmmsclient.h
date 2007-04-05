#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include <proto.h>

#define NEED_sv_2pvbyte
#define NEED_sv_2pv_nolen_GLOBAL
#define NEED_newRV_noinc_GLOBAL
#include "ppport.h"

#include <xmmsclient/xmmsclient.h>

#define PERL_XMMSCLIENT_CALL_BOOT(name) \
	{ \
		EXTERN_C XS(name); \
		_perl_xmmsclient_call_xs (aTHX_ name, cv, mark); \
	}

#ifdef PERL_IMPLICIT_CONTEXT

#define dPERL_XMMS_CLIENT_CALLBACK_MARSHAL_SP \
	SV **sp;

#define PERL_XMMS_CLIENT_MARSHAL_INIT(cb) \
	PERL_SET_CONTEXT (cb->priv); \
	SPAGAIN;

#else

#define dPERL_XMMS_CLIENT_CALLBACK_MARSHAL_SP \
	dSP;

#define PERL_XMMS_CLIENT_MARSHAL_INIT(cb) \
	/* nothing to do */

#endif

typedef struct perl_xmmsclient_playlist_St {
	xmmsc_connection_t *conn;
	char *name;
} perl_xmmsclient_playlist_t;

typedef enum {
	PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_UNKNOWN,
	PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_CONNECTION,
	PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_RESULT,
	PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_FLAG
} PerlXMMSClientCallbackParamType;

typedef struct _PerlXMMSClientCallback PerlXMMSClientCallback;
struct _PerlXMMSClientCallback {
	SV *func;
	SV *data;
	SV *wrapper;
	int n_params;
	PerlXMMSClientCallbackParamType *param_types;
#ifdef PERL_IMPLICIT_CONTEXT
	void *priv;
#endif
};

void _perl_xmmsclient_call_xs (pTHX_ void (*subaddr) (pTHX_ CV *cv), CV *cv, SV **mark);

SV *perl_xmmsclient_new_sv_from_ptr (void *con, const char *class);

MAGIC *perl_xmmsclient_get_magic_from_sv (SV *sv, const char *class);

void *perl_xmmsclient_get_ptr_from_sv (SV *sv, const char *class);

PerlXMMSClientCallback *perl_xmmsclient_callback_new (SV *func, SV *data, SV *wrapper, int n_params, PerlXMMSClientCallbackParamType param_types[]);

void perl_xmmsclient_callback_destroy (PerlXMMSClientCallback *cb);

void perl_xmmsclient_callback_invoke (PerlXMMSClientCallback *cb, ...);

SV *perl_xmmsclient_xmms_result_cast_value (xmmsc_result_value_type_t type, const void *value);

char **perl_xmmsclient_unpack_char_ptr_ptr (SV *sv);

SV *perl_xmmsclient_hv_fetch (HV *hv, const char *key, I32 klen);

perl_xmmsclient_playlist_t *perl_xmmsclient_playlist_new (xmmsc_connection_t *c, const char *playlist);

void perl_xmmsclient_playlist_destroy (perl_xmmsclient_playlist_t *p);
