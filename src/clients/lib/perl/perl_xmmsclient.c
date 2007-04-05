#include "perl_xmmsclient.h"

SV *
perl_xmmsclient_new_sv_from_ptr (void *ptr, const char *class) {
	SV *obj;
	SV *sv;
	HV *stash;

	obj = (SV *)newHV ();
	sv_magic (obj, 0, PERL_MAGIC_ext, (const char *)ptr, 0);
	sv = newRV_noinc (obj);
	stash = gv_stashpv (class, 0);
	sv_bless (sv, stash);

	return sv;
}

void *
perl_xmmsclient_get_ptr_from_sv (SV *sv, const char *class) {
	MAGIC *mg;

	if (!(mg = perl_xmmsclient_get_magic_from_sv (sv, class))) {
		return NULL;
	}

	return (void *)mg->mg_ptr;
}

MAGIC *
perl_xmmsclient_get_magic_from_sv (SV *sv, const char *class) {
	MAGIC *mg;

	if (!sv || !SvOK (sv) || !SvROK (sv) || !sv_derived_from (sv, class) || !(mg = mg_find (SvRV (sv), PERL_MAGIC_ext))) {
		return NULL;
	}

	return mg;
}

void
_perl_xmmsclient_call_xs (pTHX_ void (*subaddr) (pTHX_ CV *), CV *cv, SV **mark) {
	dSP;
	PUSHMARK (mark);
	(*subaddr) (aTHX_ cv);
	PUTBACK;
}

PerlXMMSClientCallback *
perl_xmmsclient_callback_new (SV *func, SV *data, SV *wrapper, int n_params, PerlXMMSClientCallbackParamType param_types[]) {
	PerlXMMSClientCallback *cb;

	cb = (PerlXMMSClientCallback *)malloc (sizeof (PerlXMMSClientCallback));
	memset (cb, '\0', sizeof (PerlXMMSClientCallback));

	cb->func = newSVsv (func);

	if (data) {
		cb->data = newSVsv(data);
	}

	if (wrapper) {
		cb->wrapper = newSVsv(wrapper);
	}

	cb->n_params = n_params;

	if (cb->n_params) {
		if (!param_types) {
			croak ("n_params is %d but param_types is NULL in perl_xmmsclient_callback_new", n_params);
		}

		cb->param_types = (PerlXMMSClientCallbackParamType *)malloc (sizeof (PerlXMMSClientCallbackParamType) * n_params);
		memcpy (cb->param_types, param_types, n_params * sizeof (PerlXMMSClientCallbackParamType));
	}

#ifdef PERL_IMPLICIT_CONTEXT
	cb->priv = aTHX;
#endif

	return cb;
}

void
perl_xmmsclient_callback_destroy (PerlXMMSClientCallback *cb) {
	if (cb) {
		if (cb->func) {
			SvREFCNT_dec (cb->func);
			cb->func = NULL;
		}

		if (cb->data) {
			SvREFCNT_dec (cb->data);
			cb->data = NULL;
		}

		if (cb->param_types) {
			free (cb->param_types);
			cb->n_params = 0;
			cb->param_types = NULL;
		}

		free (cb);
	}
}

void
perl_xmmsclient_callback_invoke (PerlXMMSClientCallback *cb, ...) {
	va_list va_args;
	dPERL_XMMS_CLIENT_CALLBACK_MARSHAL_SP;

	if (cb == NULL) {
		croak("cb == NULL in perl_xmmsclient_callback_invoke");
	}

	PERL_XMMS_CLIENT_MARSHAL_INIT (cb);

	ENTER;
	SAVETMPS;

	PUSHMARK (sp);

	va_start (va_args, cb);

	if (cb->n_params > 0) {
		int i;

		for (i = 0; i < cb->n_params; i++) {
			SV *sv;
			switch (cb->param_types[i]) {
				case PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_CONNECTION:
				case PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_RESULT:
					if (!cb->wrapper) {
						croak("wrapper == NULL in perl_xmmsclient_callback_invoke");
					}

					sv = cb->wrapper;
					break;
				case PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_FLAG:
					sv = newSViv (va_arg (va_args, int));
					break;
				default:
					PUTBACK;
					croak ("Unknown PerlXMMSClientCallbackParamType in perl_xmmsclient_callback_invoke");
			}

			if (!sv) {
				PUTBACK;
				croak ("failed to convert value to sv");
			}

			XPUSHs (sv);
		}
	}

	va_end (va_args);

	if (cb->data)
		XPUSHs (cb->data);

	PUTBACK;

	call_sv (cb->func, G_DISCARD);

	FREETMPS;
	LEAVE;
}

char **
perl_xmmsclient_unpack_char_ptr_ptr (SV *arg) {
	AV *av;
	SV **ssv;
	int avlen, i;
	char **ret;

	if (!SvOK (arg)) {
		return NULL;
	}

	if (SvROK (arg) && (SvTYPE (SvRV (arg)) == SVt_PVAV)) {
		av = (AV *)SvRV (arg);

		avlen = av_len (av);
		ret = (char **)malloc (sizeof (char *) * (avlen + 2));

		for (i = 0; i <= avlen; ++i) {
			ssv = av_fetch (av, i, 0);
			ret[i] = SvPV_nolen (*ssv);
		}

		ret[avlen + 1] = NULL;
	}
	else {
		croak ("not an array reference");
	}

	return ret;
}

SV *
perl_xmmsclient_hv_fetch (HV *hv, const char *key, I32 klen) {
	SV **val;

	val = hv_fetch (hv, key, klen, 0);
	if (!val) {
		return NULL;
	}

	return *val;
}

perl_xmmsclient_playlist_t *
perl_xmmsclient_playlist_new (xmmsc_connection_t *conn, const char *playlist) {
	perl_xmmsclient_playlist_t *p;

	p = (perl_xmmsclient_playlist_t *)malloc (sizeof (perl_xmmsclient_playlist_t));

	if (!p) {
		croak ("Failed to allocate playlist");
	}

	xmmsc_ref (conn);
	p->conn = conn;

	p->name = strdup (playlist);

	return p;
}

void
perl_xmmsclient_playlist_destroy (perl_xmmsclient_playlist_t *p) {
	if (p->conn) {
		xmmsc_unref (p->conn);
		p->conn = NULL;
	}

	if (p->name) {
		free (p->name);
	}

	free (p);
}
